#include "pch.h"
#include "MonitorWnd.h"
#include "Enities.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <atlstr.h>
#include <algorithm>
#include <cmath>


BEGIN_MESSAGE_MAP(CMonitorWnd, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

CMonitorWnd::CMonitorWnd() {}

CMonitorWnd::~CMonitorWnd() {
    StopMonitor();
}

bool CMonitorWnd::CreateMonitorWindow(CWnd* pParent, CclientSocket* pSocket) {
    if (!pParent) return false;
    m_pSocket = pSocket;

    // 注册并创建一个普通窗口用于显示画布
    LPCTSTR className = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW));
    CRect rc(100, 100, 100 + 1024, 100 + 768);
    if (!CWnd::CreateEx(0, className, _T("屏幕监视窗口"), WS_OVERLAPPEDWINDOW, rc, pParent, 0)) {
        return false;
    }

    // 创建全屏大小的画布用于合成（以屏幕分辨率为准）
    const int sw = GetSystemMetrics(SM_CXSCREEN);
    const int sh = GetSystemMetrics(SM_CYSCREEN);
    m_canvas.Create(sw, sh, 24);

    // 启动后台线程
    m_bIsMonitoring = true;
    m_thread = std::thread(&CMonitorWnd::MonitorThreadFunc, this);
    return true;
}

void CMonitorWnd::StopMonitor() {
    m_bIsMonitoring = false;
    if (m_thread.joinable()) m_thread.join();
    if (m_canvas) m_canvas.Destroy();
}

void CMonitorWnd::OnDestroy() {
    StopMonitor();
    CWnd::OnDestroy();
}

BOOL CMonitorWnd::OnEraseBkgnd(CDC* /*pDC*/) {
    // 返回 TRUE，告诉系统我们已经处理背景擦除（实际在 OnPaint 中双缓冲绘制），以避免闪烁
    return TRUE;
}

void CMonitorWnd::OnPaint() {
    CPaintDC dc(this);
    std::lock_guard<std::mutex> lock(m_canvasMutex);
    CRect rc;
    GetClientRect(&rc);
    int clientW = rc.Width();
    int clientH = rc.Height();

    // 双缓冲：在内存 DC 上先绘制（包含 letterbox 背景），然后一次性 blit 到窗口，避免闪烁
    CDC memDC;
    memDC.CreateCompatibleDC(&dc);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&dc, clientW, clientH);
    CBitmap* pOldBmp = memDC.SelectObject(&bmp);

    // 背景填充（letterbox 区域）
    memDC.FillSolidRect(&rc, RGB(0, 0, 0));

    if (m_canvas) {
        // 按窗口大小等比缩放并居中绘制画布，画布保持原始全屏分辨率用于合成/保存
        int canvasW = m_canvas.GetWidth();
        int canvasH = m_canvas.GetHeight();
        if (canvasW > 0 && canvasH > 0 && clientW > 0 && clientH > 0) {
            double scale = (std::min)((double)clientW / (double)canvasW, (double)clientH / (double)canvasH);
            int destW = (std::max)(1, (int)std::lround(canvasW * scale));
            int destH = (std::max)(1, (int)std::lround(canvasH * scale));
            int destLeft = (clientW - destW) / 2;
            int destTop = (clientH - destH) / 2;

            // 在内存 DC 上绘制缩放后的画布
            m_canvas.Draw(memDC.GetSafeHdc(), destLeft, destTop, destW, destH, 0, 0, canvasW, canvasH);
        }
    }

    // 一次性拷贝到窗口，减少闪烁
    dc.BitBlt(0, 0, clientW, clientH, &memDC, 0, 0, SRCCOPY);

    // 恢复并清理
    memDC.SelectObject(pOldBmp);
    bmp.DeleteObject();
    memDC.DeleteDC();
}

void CMonitorWnd::MonitorThreadFunc() {
    if (!m_pSocket) return;

    while (m_bIsMonitoring) {
        // 发送截屏请求
        Cpacket req(CMD_SCREEN_CAPTURE, {});
        if (!m_pSocket->SendPacket(req)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        std::optional<Cpacket> resp = m_pSocket->RecvPacket();
        if (!resp || resp->sCmd != CMD_SCREEN_CAPTURE) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        const std::vector<BYTE>& data = resp->data;
        if (data.size() < 16) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        int x = 0, y = 0, w = 0, h = 0;
        memcpy(&x, data.data(), 4);
        memcpy(&y, data.data()+4, 4);
        memcpy(&w, data.data()+8, 4);
        memcpy(&h, data.data()+12, 4);

        size_t pngSize = data.size() - 16;
        const BYTE* pngData = data.data() + 16;

        if (pngSize == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // 将 PNG 数据加载到 CImage
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, pngSize);
        if (!hMem) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        void* pMem = GlobalLock(hMem);
        memcpy(pMem, pngData, pngSize);
        GlobalUnlock(hMem);

        IStream* pStream = nullptr;
        if (FAILED(CreateStreamOnHGlobal(hMem, TRUE, &pStream))) {
            GlobalFree(hMem);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        CImage diffImg;
        HRESULT hr = diffImg.Load(pStream);
        pStream->Release();
        if (FAILED(hr)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // 合成到画布
        {
            std::lock_guard<std::mutex> lock(m_canvasMutex);
            HDC hCanvasDC = m_canvas.GetDC();
            if (hCanvasDC) {
                diffImg.Draw(hCanvasDC, x, y, w, h, 0, 0, w, h);
                m_canvas.ReleaseDC();
            }
        }

        // 如果正在录制，保存整帧
        if (m_bIsRecording) {
            SaveFrameToDisk();
        }

        // 刷新窗口显示
        Invalidate(FALSE);

        // 控制帧率
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void CMonitorWnd::SaveFrameToDisk() {
    try {
        std::filesystem::create_directories("recordings");
    } catch (...) {
        return;
    }

    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    std::ostringstream ss;
    ss << "recordings\\frame_" << std::put_time(&tm, "%Y%m%d_%H%M%S_") << m_frameIndex++ << ".png";
    std::string path = ss.str();

    // 保存时加锁，防止画布被写入
    std::lock_guard<std::mutex> lock(m_canvasMutex);
    if (m_canvas) {
        CString cpath(path.c_str());
        m_canvas.Save(cpath);
    }
}
