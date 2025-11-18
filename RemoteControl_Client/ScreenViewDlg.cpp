#include "pch.h"
#include "framework.h"
#include "RemoteControl_Client.h"
#include "ScreenViewDlg.h"
#include "Enities.h"

IMPLEMENT_DYNAMIC(CScreenViewDlg, CDialogEx)

// GDI+ 全局变量
static ULONG_PTR g_gdiplusToken = 0;
static bool g_gdiplusInitialized = false;

// GDI+初始化函数实现
bool CScreenViewDlg::InitializeGDIPlus()
{
    if (g_gdiplusInitialized) return true;
    
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);
    g_gdiplusInitialized = true;
    OutputDebugStringA("GDI+ initialized successfully\n");
    return true;
}

// GDI+关闭函数实现
void CScreenViewDlg::ShutdownGDIPlus()
{
    if (g_gdiplusInitialized) {
        GdiplusShutdown(g_gdiplusToken);
        g_gdiplusInitialized = false;
        OutputDebugStringA("GDI+ shutdown completed\n");
    }
}

CScreenViewDlg::CScreenViewDlg(CClientSocket* pSocket, CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_DIALOG_SCREEN_VIEW, pParent), m_pSocket(pSocket)
{
    // 初始化GDI+
    InitializeGDIPlus();
}

CScreenViewDlg::~CScreenViewDlg()
{
    if (!m_image.IsNull()) m_image.Destroy();
    StopReceiveLoop();
    
    // 注：在这里不调用ShutdownGDIPlus，避免多实例时的问题
    // 可以在应用程序退出时统一调用
}

void CScreenViewDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_STATIC_SCREEN, m_pictureCtrl);
}

BEGIN_MESSAGE_MAP(CScreenViewDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_CLOSE()
    ON_WM_SIZE()
    ON_MESSAGE(WM_UPDATE_SCREEN, &CScreenViewDlg::OnUpdateScreen)
    ON_WM_DRAWITEM()
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

BOOL CScreenViewDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // 使用 socket 的回调机制：注册本窗口句柄并发送开始监控命令
    if (m_pSocket) {
        m_pSocket->SetScreenViewWnd(GetSafeHwnd());
        Cpacket pkt(REQ_START_WATCH, NULL, 0);
        if (!m_pSocket->Send(pkt)) {
            OutputDebugString(_T("Failed to send start watch command"));
        }
    } else {
        CClientSocket& client = CClientSocket::GetInstance();
        client.SetScreenViewWnd(GetSafeHwnd());
        Cpacket pkt(REQ_START_WATCH, NULL, 0);
        if (!client.Send(pkt)) {
            OutputDebugString(_T("Failed to send start watch command"));
        }
        m_pSocket = &client;
    }

    StartReceiveLoop();

    // 强制一次初始重绘（防止首帧在对话尚未显示时被跳过）
    if (IsWindow(m_pictureCtrl.GetSafeHwnd())) {
        m_pictureCtrl.RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
    } else {
        RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
    }

    return TRUE;
}

void CScreenViewDlg::OnClose()
{
    // 发送停止命令
    // 只解除注册窗口句柄，避免发送未在服务器端实现的停止命令（11 会被服务器解释为删除文件）
    if (m_pSocket) {
        Cpacket pkt(REQ_STOP_WATCH, NULL, 0);
        if (!m_pSocket->Send(pkt)) {
            OutputDebugString(_T("Failed to send stop watch command"));
        }
        m_pSocket->SetScreenViewWnd(NULL);
    } else {
        Cpacket pkt(REQ_STOP_WATCH, NULL, 0);
        if (!CClientSocket::GetInstance().Send(pkt)) {
            OutputDebugString(_T("Failed to send stop watch command"));
        }
        CClientSocket::GetInstance().SetScreenViewWnd(NULL);
    }
    StopReceiveLoop();
    CDialogEx::OnClose();
}

void CScreenViewDlg::OnPaint()
{
    CPaintDC dlgDc(this);

    // 优先在图片控件上绘制，避免被静态控件覆盖
    if (!m_image.IsNull() && IsWindow(m_pictureCtrl.GetSafeHwnd())) {
        CRect ctrlRect;
        m_pictureCtrl.GetClientRect(&ctrlRect);

        // 检查目标区域的有效性
        if (ctrlRect.Width() <= 0 || ctrlRect.Height() <= 0) {
            OutputDebugStringA("OnPaint: Invalid drawing rect (width or height <= 0)\n");
            return;
        }

        // 检查图像的尺寸有效性
        int imgWidth = m_image.GetWidth();
        int imgHeight = m_image.GetHeight();
        if (imgWidth <= 0 || imgHeight <= 0) {
            OutputDebugStringA("OnPaint: Invalid image dimensions\n");
            return;
        }

        try {
            // 在图片控件的 DC 上绘制（控件客户区坐标从 0,0 开始）
            CClientDC ctrlDc(&m_pictureCtrl);
            m_image.Draw(ctrlDc.GetSafeHdc(), 0, 0, ctrlRect.Width(), ctrlRect.Height());
        } catch (CException* e) {
            e->ReportError();
            e->Delete();
            OutputDebugStringA("OnPaint: Draw operation failed with CException\n");
        } catch (...) {
            OutputDebugStringA("OnPaint: Draw operation failed with unknown exception\n");
        }
    } else if (!m_image.IsNull()) {
        // 如果控件无效，回退到在对话框上绘制
        CRect rect;
        GetClientRect(&rect);
        if (rect.Width() > 0 && rect.Height() > 0) {
            dlgDc.FillSolidRect(rect, RGB(0, 0, 0));
            int imgWidth = m_image.GetWidth();
            int imgHeight = m_image.GetHeight();
            if (imgWidth > 0 && imgHeight > 0) {
                try {
                    m_image.Draw(dlgDc.GetSafeHdc(), 0, 0, rect.Width(), rect.Height());
                } catch (CException* e) {
                    e->ReportError();
                    e->Delete();
                    OutputDebugStringA("OnPaint: Fallback draw operation failed with CException\n");
                } catch (...) {
                    OutputDebugStringA("OnPaint: Fallback draw operation failed with unknown exception\n");
                }
            }
        }
    }
}

void CScreenViewDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    
    // 确保对话框尺寸有效
    if (cx <= 0 || cy <= 0) {
        OutputDebugStringA("OnSize: Invalid dialog size\n");
        return;
    }
    
    // 重绘对话框
    if (IsWindow(m_pictureCtrl.GetSafeHwnd())) {
        try {
            // 调整图片控件位置和大小
            m_pictureCtrl.MoveWindow(0, 0, cx, cy);
        } catch (CException* e) {
            e->ReportError();
            e->Delete();
            OutputDebugStringA("OnSize: MoveWindow failed with CException\n");
        } catch (...) {
            OutputDebugStringA("OnSize: MoveWindow failed with unknown exception\n");
        }
    } else {
        // 如果控件窗口句柄无效，尝试查找并获取句柄
        CWnd* pStatic = GetDlgItem(IDC_STATIC_SCREEN);
        if (pStatic && pStatic->GetSafeHwnd()) {
            m_pictureCtrl.Attach(pStatic->GetSafeHwnd());
            try {
                m_pictureCtrl.MoveWindow(0, 0, cx, cy);
            } catch (CException* e) {
                e->ReportError();
                e->Delete();
            } catch (...) {
                OutputDebugStringA("OnSize: Second MoveWindow attempt failed\n");
            }
        }
    }
}

void CScreenViewDlg::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    if (lpDrawItemStruct == nullptr) return;

    // 只处理我们的静态控件
    if (nIDCtl != IDC_STATIC_SCREEN) {
        CDialogEx::OnDrawItem(nIDCtl, lpDrawItemStruct);
        return;
    }

    HDC hdc = lpDrawItemStruct->hDC;
    CRect rc = lpDrawItemStruct->rcItem;

    // 双缓冲：创建兼容 DC 和位图
    CDC memDC;
    memDC.CreateCompatibleDC(CDC::FromHandle(hdc));
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(CDC::FromHandle(hdc), rc.Width(), rc.Height());
    CBitmap* pOldBmp = memDC.SelectObject(&bmp);

    // 填充背景（避免未初始化像素）
    memDC.FillSolidRect(&rc, RGB(0, 0, 0));

    // 在内存 DC 上绘制图像（坐标以控件客户区为基准）
    if (!m_image.IsNull()) {
        try {
            m_image.Draw(memDC.GetSafeHdc(), 0, 0, rc.Width(), rc.Height());
        } catch (...) {
            OutputDebugStringA("OnDrawItem: failed to draw image to memDC\n");
        }
    }

    // 将内存位图 blt 到目标 DC
    BitBlt(hdc, rc.left, rc.top, rc.Width(), rc.Height(), memDC.GetSafeHdc(), 0, 0, SRCCOPY);

    // 恢复并清理
    memDC.SelectObject(pOldBmp);
    bmp.DeleteObject();
    memDC.DeleteDC();
}

HBRUSH CScreenViewDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    if (pWnd->GetDlgCtrlID() == IDC_STATIC_SCREEN) {
        pDC->SetBkMode(TRANSPARENT);
        return (HBRUSH)::GetStockObject(NULL_BRUSH);
    }
    return CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);
}

LRESULT CScreenViewDlg::OnUpdateScreen(WPARAM wParam, LPARAM lParam)
{
    size_t nSize = (size_t)wParam;
    char* pData = (char*)lParam;

    // 添加诊断信息
    CStringA debugMsg;
    debugMsg.Format("OnUpdateScreen: Received data size=%zu, ptr=0x%p\n", nSize, pData);
    OutputDebugStringA(debugMsg);

    if (pData && nSize > 0) {
        // 检查数据大小范围
        if (nSize < 8) {
            debugMsg.Format("OnUpdateScreen: Data too small (size=%zu)\n", nSize);
            OutputDebugStringA(debugMsg);
            delete[] pData;
            return 0;
        }

        // 检查是否是Cpacket格式的数据（包含0xFEFF头部）
        bool isCpacketFormat = false;
        size_t pngDataOffset = 0;
        size_t pngDataSize = nSize;
        
        // 检查前2字节是否为0xFEFF（Cpacket头部标识）
        if (nSize >= 2 && *(WORD*)pData == 0xFEFF) {
            isCpacketFormat = true;
            debugMsg.Format("OnUpdateScreen: Detected Cpacket format, extracting PNG data...\n");
            OutputDebugStringA(debugMsg);
            
            // 解析Cpacket结构提取真正的PNG数据
            // Cpacket格式: [2字节头部0xFEFF] + [4字节长度] + [2字节命令码] + [数据] + [2字节校验和]
            if (nSize >= 10) { // 至少需要头部+长度+命令码
                DWORD packetLength = *(DWORD*)(pData + 2);
                WORD commandCode = *(WORD*)(pData + 6);
                
                debugMsg.Format("OnUpdateScreen: Cpacket - Length: %u, Command: %u\n", packetLength, commandCode);
                OutputDebugStringA(debugMsg);
                
                // 健壮的边界检查：确保数据包长度合理且完整
                if (packetLength >= 6 && packetLength <= 10 * 1024 * 1024) { // 最小6字节，最大10MB
                    // 检查整个数据包是否完整接收
                    size_t expectedTotalSize = 2 + 4 + packetLength; // 头部2 + 长度4 + 数据包长度
                    if (nSize >= expectedTotalSize) {
                        // 计算PNG数据在包中的位置和大小
                        pngDataOffset = 8; // 跳过头部(2) + 长度(4) + 命令码(2) = 8字节
                        pngDataSize = packetLength - 4; // 总长度减去命令码(2)和校验和(2)
                        
                        // 验证校验和
                        WORD expectedChecksum = *(WORD*)(pData + 8 + pngDataSize);
                        WORD actualChecksum = 0;
                        for (size_t j = 0; j < pngDataSize; j++) {
                            actualChecksum += BYTE(pData[pngDataOffset + j]) & 0xFF;
                        }
                        
                        if (actualChecksum == expectedChecksum) {
                            debugMsg.Format("OnUpdateScreen: Checksum verified - Offset: %zu, Size: %zu\n", pngDataOffset, pngDataSize);
                            OutputDebugStringA(debugMsg);
                        } else {
                            debugMsg.Format("OnUpdateScreen: Checksum mismatch! Expected: %u, Actual: %u\n", expectedChecksum, actualChecksum);
                            OutputDebugStringA(debugMsg);
                            // 校验和错误，尝试使用原始数据
                            isCpacketFormat = false;
                            pngDataOffset = 0;
                            pngDataSize = nSize;
                        }
                    } else {
                        debugMsg.Format("OnUpdateScreen: Incomplete packet! Expected: %zu, Received: %zu\n", expectedTotalSize, nSize);
                        OutputDebugStringA(debugMsg);
                        // 数据包不完整，尝试使用原始数据
                        isCpacketFormat = false;
                        pngDataOffset = 0;
                        pngDataSize = nSize;
                    }
                } else {
                    debugMsg.Format("OnUpdateScreen: Invalid packet length: %u\n", packetLength);
                    OutputDebugStringA(debugMsg);
                    // 数据包长度无效，尝试使用原始数据
                    isCpacketFormat = false;
                    pngDataOffset = 0;
                    pngDataSize = nSize;
                }
            } else {
                debugMsg.Format("OnUpdateScreen: Packet too small for Cpacket format: %zu bytes\n", nSize);
                OutputDebugStringA(debugMsg);
                // 数据太小，不可能是Cpacket格式
                isCpacketFormat = false;
                pngDataOffset = 0;
                pngDataSize = nSize;
            }
        }

        // 检查 PNG 签名
        bool isPNG = false;
        
        // 首先检查提取的数据是否为PNG
        if (pngDataSize >= 8) {
            isPNG = (memcmp(pData + pngDataOffset, "\x89PNG\r\n\x1A\n", 8) == 0);
        }
        
        if (!isPNG) {
            if (isCpacketFormat) {
                debugMsg.Format("OnUpdateScreen: Extracted data is not PNG format, checking raw data...\n");
                OutputDebugStringA(debugMsg);
                
                // 如果提取的数据不是PNG，尝试使用原始数据（可能是直接PNG）
                if (nSize >= 8) {
                    isPNG = (memcmp(pData, "\x89PNG\r\n\x1A\n", 8) == 0);
                    if (isPNG) {
                        debugMsg.Format("OnUpdateScreen: Raw data is PNG format, using raw data\n");
                        OutputDebugStringA(debugMsg);
                        pngDataOffset = 0;
                        pngDataSize = nSize;
                    }
                }
            }
            
            if (!isPNG) {
                debugMsg.Format("OnUpdateScreen: Warning - Not a standard PNG signature, attempting to load anyway\n");
                OutputDebugStringA(debugMsg);
                
                // 保存数据到文件以便分析
                FILE* fp = nullptr;
                if (fopen_s(&fp, "debug_screen_data.bin", "wb") == 0) {
                    fwrite(pData, 1, nSize, fp);
                    fclose(fp);
                    OutputDebugStringA("OnUpdateScreen: Saved data to debug_screen_data.bin\n");
                }
                
                // 尝试检测其他可能的图像格式
                if (pngDataSize >= 4) {
                    // 检查JPEG格式
                    if (memcmp(pData + pngDataOffset, "\xFF\xD8\xFF", 3) == 0) {
                        debugMsg.Format("OnUpdateScreen: Detected JPEG format\n");
                        OutputDebugStringA(debugMsg);
                    }
                    // 检查BMP格式
                    else if (memcmp(pData + pngDataOffset, "BM", 2) == 0) {
                        debugMsg.Format("OnUpdateScreen: Detected BMP format\n");
                        OutputDebugStringA(debugMsg);
                    }
                }
            }
        }

        // 创建内存流并写入提取的PNG数据
        HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, pngDataSize);
        if (hGlobal != nullptr) {
            void* pGlobalData = GlobalLock(hGlobal);
            if (pGlobalData != nullptr) {
                memcpy(pGlobalData, pData + pngDataOffset, pngDataSize);
                GlobalUnlock(hGlobal);

                IStream* pStream = nullptr;
                  if (CreateStreamOnHGlobal(hGlobal, TRUE, &pStream) == S_OK) {
                      // 清理旧图像
                      if (!m_image.IsNull()) {
                          m_image.Destroy();
                          OutputDebugStringA("OnUpdateScreen: Old image destroyed\n");
                      }

                      // 将流重置到起始位置
                      LARGE_INTEGER li = { 0 };
                      ULARGE_INTEGER uli = { 0 };
                      if (pStream->Seek(li, STREAM_SEEK_SET, &uli) == S_OK) {
                          OutputDebugStringA("OnUpdateScreen: Stream position reset to start\n");
                      } else {
                          OutputDebugStringA("OnUpdateScreen: Failed to reset stream position\n");
                          // 即使流重置失败，也尝试加载图像
                      }

                      // 尝试加载图像
                      OutputDebugStringA("OnUpdateScreen: Attempting to load image...\n");
                      HRESULT hr = m_image.Load(pStream);
                    if (SUCCEEDED(hr)) {
                        // 验证图像尺寸的有效性
                        int imgWidth = m_image.GetWidth();
                        int imgHeight = m_image.GetHeight();
                        
                        if (imgWidth <= 0 || imgHeight <= 0) {
                            debugMsg.Format("OnUpdateScreen: Invalid image dimensions - Width: %d, Height: %d\n", imgWidth, imgHeight);
                            OutputDebugStringA(debugMsg);
                            
                            // 清理无效图像
                            if (!m_image.IsNull()) {
                                m_image.Destroy();
                            }
                        } else {
                            debugMsg.Format("OnUpdateScreen: Image loaded successfully - Width: %d, Height: %d\n", imgWidth, imgHeight);
                            OutputDebugStringA(debugMsg);
                            if (IsWindow(m_pictureCtrl.GetSafeHwnd())) {
                                m_pictureCtrl.RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
                            } else {
                                RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
                            }
                        }
                    } else {
                        debugMsg.Format("OnUpdateScreen: Load image failed with HRESULT=0x%X\n", hr);
                        OutputDebugStringA(debugMsg);
                        
                        // 尝试使用GDI+作为备选方案
                        try {
                            // 重置流位置，确保从头读取
                            LARGE_INTEGER li = { 0 };
                            pStream->Seek(li, STREAM_SEEK_SET, NULL);
                            
                            Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromStream(pStream);
                            if (bitmap) {
                                OutputDebugStringA("OnUpdateScreen: GDI+ Bitmap created successfully\n");
                                
                                // 检查GDI+图像的状态
                                Gdiplus::Status status = bitmap->GetLastStatus();
                                if (status == Gdiplus::Ok) {
                                    // 将GDI+ Bitmap转换为HBITMAP
                                    HBITMAP hBitmap = nullptr;
                                    if (bitmap->GetHBITMAP(Gdiplus::Color::Black, &hBitmap) == Gdiplus::Ok && hBitmap != nullptr) {
                                        if (!m_image.IsNull()) m_image.Destroy();
                                        m_image.Attach(hBitmap);
                                        if (IsWindow(m_pictureCtrl.GetSafeHwnd())) {
                                            m_pictureCtrl.RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
                                        } else {
                                            RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
                                        }
                                        OutputDebugStringA("OnUpdateScreen: GDI+ fallback succeeded\n");
                                    } else {
                                        OutputDebugStringA("OnUpdateScreen: GDI+ GetHBITMAP failed\n");
                                    }
                                } else {
                                    CStringA statusMsg;
                                    statusMsg.Format("OnUpdateScreen: GDI+ Bitmap status error - %d\n", status);
                                    OutputDebugStringA(statusMsg);
                                }
                                delete bitmap;
                            } else {
                                OutputDebugStringA("OnUpdateScreen: GDI+ Bitmap creation failed\n");
                            }
                        } catch (CException* e) {
                            e->ReportError();
                            e->Delete();
                            OutputDebugStringA("OnUpdateScreen: GDI+ fallback failed with CException\n");
                        } catch (...) {
                            OutputDebugStringA("OnUpdateScreen: GDI+ fallback failed with unknown exception\n");
                        }
                    }

                    pStream->Release();
                } else {
                    OutputDebugStringA("OnUpdateScreen: CreateStreamOnHGlobal failed\n");
                    GlobalFree(hGlobal); // 释放GlobalAlloc的内存
                }
            } else {
                OutputDebugStringA("OnUpdateScreen: GlobalLock failed\n");
                GlobalFree(hGlobal); // 释放GlobalAlloc的内存
            }
        } else {
            OutputDebugStringA("OnUpdateScreen: GlobalAlloc failed\n");
        }
    } else {
        OutputDebugStringA("OnUpdateScreen: Invalid data (null pointer or zero size)\n");
    }

    // 释放网络线程分配的内存
    if (pData) {
        delete[] pData;
    }

    return 0;
}

void CScreenViewDlg::StartReceiveLoop()
{
    if (m_pSocket == nullptr || m_receiving.load(std::memory_order_relaxed)) {
        return;
    }
    m_receiving.store(true, std::memory_order_release);
    m_receiveThread = std::thread([this]() {
        while (m_receiving.load(std::memory_order_acquire)) {
            int cmd = m_pSocket->DealCommand(100, &m_receiving);
            if (cmd <= 0) {
                break;
            }
            if (cmd != SCREEN_DATA_PACKET) {
                TRACE("Unexpected cmd %d received during screen stream, stopping\n", cmd);
                break;
            }
        }
        m_receiving.store(false, std::memory_order_release);
    });
}

void CScreenViewDlg::StopReceiveLoop()
{
    m_receiving.store(false, std::memory_order_release);
    if (m_receiveThread.joinable()) {
        m_receiveThread.join();
    }
}
