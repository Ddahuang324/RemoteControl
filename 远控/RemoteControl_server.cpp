// 远控.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "framework.h"
#include "RemoteControl_server.h"
#include "ServerSocket.h"
#include "direct.h"
#include "io.h"
#include <list>
#include <atlimage.h>


typedef struct file_info {
    file_info() {
        isValid = 0;
		isDir = 0;
		hasNext = true;
		memset(szFileName, 0, sizeof(szFileName));
    }

	bool isValid; //是否有效
    bool isDir; //是目录还是文件
	char szFileName[256];//文件名
	bool hasNext;//是否有下一个
    
}FILEINFO, * PFILEINFO;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
#pragma comment (linker , "/subsystem:windows /ENTRY:mainCRTStartup")
#pragma comment (linker , "/subsystem:windows /ENTRY:WinMainCRTStartup")
#pragma comment (linker , "/subsystem:console /ENTRY:mainCRTStartup")
#pragma comment (linker , "/subsystem:console /ENTRY:WinMainCRTStartup")

// 唯一的应用程序对象

CWinApp theApp;

using namespace std;

void Dump(const BYTE* pData, size_t nSize) {
    std::string strOut;
    for (size_t i = 0; i < nSize; i++) {
        char buf[8] = "";
        if (i > 0 && i % 16 == 0) {
            strOut += "\n";
        }
        snprintf(buf, sizeof(buf), "%02X ", pData[i] & 0xFF);
        strOut += buf;
    }
    strOut += "\n";
    OutputDebugStringA(strOut.c_str());
}

int MakeDriverInfo() {
    std::string result;

    // 使用位掩码一次性枚举 A:~Z:
    DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (mask & (1u << i)) {
            if (!result.empty()) result += ",";
            result += char('A' + i);
        }
    }

    Cpacket pack(1, reinterpret_cast<const BYTE*>(result.data()), result.size());
    Dump(reinterpret_cast<const BYTE*>(pack.Data()), pack.Size());
    //CServerSocket::GetInstance().Send(pack); 
    return 0;
}

int MakeDirectoryInfo() {
    std::string path;
    std::list<FILEINFO>listFileInfo;
    if (CServerSocket::GetInstance().GetFilePath(path) == false) {
        OutputDebugString(_T("当前的命令不是文件获取列表，命令解析错误"));
        return -1;
    }
    if (_chdir(path.c_str()) != 0) {
        FILEINFO finfo;
        finfo.isValid = true;
        finfo.isDir = true;
        finfo.hasNext = false;
        memcpy(finfo.szFileName, path.c_str(), path.size());
        //listFileInfo.push_back(finfo);
        Cpacket pack(2, (BYTE*) & finfo, sizeof(finfo));
        CServerSocket::GetInstance().Send(pack);
        OutputDebugString(_T("没有权限，访问目录"));
        return -2;
    }

    _finddata_t fdata;
    int hfind = 0;
    if ((hfind =_findfirst("*", &fdata)) == -1) {
        OutputDebugString(_T("没有找到任何文件"));
        return -3;
    }


    do {
        FILEINFO finfo; 
		finfo.isDir = (fdata.attrib & _A_SUBDIR) != 0;
        memcpy(finfo.szFileName, fdata.name, strlen(fdata.name));
        //listFileInfo.push_back(finfo);
        Cpacket pack(2, (BYTE*)&finfo, sizeof(finfo));
        CServerSocket::GetInstance().Send(pack);
        //返回值处理？
    } while (!_findnext(hfind, &fdata));

	FILEINFO finfo;
	finfo.hasNext = false;

    Cpacket pack(2, (BYTE*)&finfo, sizeof(finfo));
    CServerSocket::GetInstance().Send(pack);

    return 0;
}

int RunFile() {
	std::string path;
	CServerSocket::GetInstance().GetFilePath(path);
	ShellExecuteA(NULL, NULL, path.c_str(), NULL, NULL, SW_SHOWNORMAL);
    Cpacket pack(3, NULL, 0);
    CServerSocket::GetInstance().Send(pack);
    return 0;
}

int DownLoadFile(){

    std::string path;
    CServerSocket::GetInstance().GetFilePath(path);
    FILE* fp = nullptr;
    long long data = 0;
    if (fopen_s(&fp, path.c_str(), "rb") != 0 ) {
        Cpacket pack(4, (BYTE*)data, 8);
        OutputDebugString(_T("打开文件失败"));
        CServerSocket::GetInstance().Send(pack);
        return -1;
    }
    if (fp != NULL) {
        fseek(fp, 0, SEEK_END);
        data = _ftelli64(fp);
        Cpacket head(4, (BYTE*)data, 8);
        fseek(fp, 0, SEEK_SET);

        char Buffer[1024] = "";
        size_t nRead = 0;
        do {
            nRead = fread(Buffer, 1, sizeof(Buffer), fp);
            Cpacket pack(4, (BYTE*)Buffer, nRead);
            CServerSocket::GetInstance().Send(pack);

        } while (nRead >= 1024 && CServerSocket::GetInstance().Send((BYTE*)Buffer, nRead));
        fclose(fp);
    }

    Cpacket pack(4, NULL, 0);
    CServerSocket::GetInstance().Send(pack);
    return 0;
}

int MouseEvent() {
    MOUSEEVENT mouse;
    if (CServerSocket::GetInstance().GetMouseEvent(mouse)) {
        OutputDebugString(_T("获取鼠标事件成功"));
      
        DWORD nFlags = 0; //鼠标标志位， 1 = 左键 2 = 右键 4 = 中键
       
    switch (mouse.nButton)
        {
        case 0: //左键
            nFlags =  1 ;
            break;
        case 1: //右键
            nFlags = 2 ;
            break;
        case 2: //中键
            nFlags = 4 ;
            break;
        case 3 ://没有按键，移动
            nFlags = 8 ;
            break;
        default:
            break;
        }
        return 0;
    
    if(nFlags != 8)SetCursorPos(mouse.ptXY.x, mouse.ptXY.y);
    
    switch (mouse.nAction)
    {
    case 0: //单击
        nFlags |= 0x10;
        break;
    case 1: //双击
        nFlags |= 0x20;
        break;
    case 2: //按下
        nFlags |= 0x40;
        break;
    case 3: //放开
        nFlags |= 0x80;
        break;
    default:
        break;
    }

    switch (nFlags)
    {
    case 0x11: //左键单击
        mouse_event(MOUSEEVENTF_LEFTDOWN,0,0,0,GetMessageExtraInfo());
        mouse_event(MOUSEEVENTF_LEFTUP,0,0,0,GetMessageExtraInfo());
        break; 
    case 0x21: //左键双击
        mouse_event(MOUSEEVENTF_LEFTDOWN,0,0,0,GetMessageExtraInfo());
        mouse_event(MOUSEEVENTF_LEFTUP,0,0,0,GetMessageExtraInfo());
        mouse_event(MOUSEEVENTF_LEFTDOWN,0,0,0,GetMessageExtraInfo());
        mouse_event(MOUSEEVENTF_LEFTUP,0,0,0,GetMessageExtraInfo());
        break;
    case 0x41: //左键按下  
        mouse_event(MOUSEEVENTF_LEFTDOWN,0,0,0,GetMessageExtraInfo());
        break;
    case 0x81: //左键放开
        mouse_event(MOUSEEVENTF_LEFTUP,0,0,0,GetMessageExtraInfo());
        break;
    case 0x12: //右键单击
        mouse_event(MOUSEEVENTF_RIGHTDOWN,0,0,0,GetMessageExtraInfo());
        mouse_event(MOUSEEVENTF_RIGHTUP,0,0,0,GetMessageExtraInfo());
        break;
    case 0x22: //右键双击
        mouse_event(MOUSEEVENTF_RIGHTDOWN,0,0,0,GetMessageExtraInfo());
        mouse_event(MOUSEEVENTF_RIGHTUP,0,0,0,GetMessageExtraInfo());
        mouse_event(MOUSEEVENTF_RIGHTDOWN,0,0,0,GetMessageExtraInfo());
        mouse_event(MOUSEEVENTF_RIGHTUP,0,0,0,GetMessageExtraInfo());
        break;  
    case 0x42: //右键按下
        mouse_event(MOUSEEVENTF_RIGHTDOWN,0,0,0,GetMessageExtraInfo()); 
        break;
    case 0x82: //右键放开
        mouse_event(MOUSEEVENTF_RIGHTUP,0,0,0,GetMessageExtraInfo());
        break;
    case 0x14: //中键单击
        mouse_event(MOUSEEVENTF_MIDDLEDOWN,0,0,0,GetMessageExtraInfo());
        mouse_event(MOUSEEVENTF_MIDDLEUP,0,0,0,GetMessageExtraInfo());
        break;
    case 0x24: //中键双击
        mouse_event(MOUSEEVENTF_MIDDLEDOWN,0,0,0,GetMessageExtraInfo());
        mouse_event(MOUSEEVENTF_MIDDLEUP,0,0,0,GetMessageExtraInfo());
        mouse_event(MOUSEEVENTF_MIDDLEDOWN,0,0,0,GetMessageExtraInfo());
        mouse_event(MOUSEEVENTF_MIDDLEUP,0,0,0,GetMessageExtraInfo());
        break;  
    case 0x44: //中键按下
        mouse_event(MOUSEEVENTF_MIDDLEDOWN,0,0,0,GetMessageExtraInfo());
        break;
    case 0x84: //中键放开
        mouse_event(MOUSEEVENTF_MIDDLEUP,0,0,0,GetMessageExtraInfo());
        break;
    case 0x08: //鼠标移动
        mouse_event(MOUSEEVENTF_MOVE,mouse.ptXY.x,mouse.ptXY.y,0,GetMessageExtraInfo());
        break;
    default:
        break;
    }
 
    Cpacket pack(5, (BYTE*)&mouse, sizeof(mouse));
    CServerSocket::GetInstance().Send(pack); //发送回执
    }else {
        OutputDebugString(_T("获取鼠标事件失败"));
        return -1;
    }
}
#include "LockDialog.h"
CLockDialog dlg;
unsigned threadId = 0;



unsigned __stdcall threadLockDlg(void* arg) {
     dlg.Create(IDD_DIALOG_INFO);
    dlg.ShowWindow(SW_SHOW);
    //窗口置顶
    dlg.SetWindowPos(&dlg.wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    //限制鼠标功能
    ShowCursor(false);
    CRect rect;
    //限制鼠标位置
    dlg.GetWindowRect(&rect);
    ClipCursor(&rect);  // 限制鼠标在对话框范围内

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if(msg.message == WM_KEYDOWN){
            break;
        }
    }
    ClipCursor(NULL);  // 释放鼠标限制
    dlg.DestroyWindow();

    ShowCursor(true);
    _endthread();
    _endthreadex(0);
    return 0;
}

int LockMachine() {
 //非模态
 if((dlg.m_hWnd == NULL) || (dlg.m_hWnd == INVALID_HANDLE_VALUE)){//判断窗口是否存在
     //_beginthread(threadLockDlg, 0, NULL);//如果不存在则创建线程显示窗口
     _beginthreadex(NULL, 0, threadLockDlg, NULL, 0, &threadId);
 }


    Cpacket pack(7, NULL, 0);//发送锁定命令
    CServerSocket::GetInstance().Send(pack); //发送回执
   
    return 0;
}

int UnlockMachine() {
    PostThreadMessage(threadId, WM_KEYDOWN, 0, 0); //向锁定线程发送消息，结束锁定对话框
    Cpacket pack(8, NULL, 0); //发送回执
    CServerSocket::GetInstance().Send(pack);
    return 0; 
}

int sendScreen() {
    CImage screen;//适合GDI编程的 库 atlimage.h ,GDI:Graphics Device Interface
    HDC hScreen = ::GetDC(NULL);
    int nBitPerPixel = GetDeviceCaps(hScreen, BITSPIXEL);//涉及到了位图的概念 (计算机色彩)
    int nWidth = GetDeviceCaps(hScreen, HORZRES);
    int nHeight = GetDeviceCaps(hScreen, VERTRES);
    screen.Create(nWidth, nHeight, nBitPerPixel);
    BitBlt(screen.GetDC(), 0, 0, nWidth, nHeight, hScreen, 0, 0, SRCCOPY);
    ReleaseDC(NULL, hScreen);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, 0); //申请内存
    if(hMem  == NULL){
        OutputDebugString(_T("申请内存失败"));
        return -1;
    }
    IStream* pStream = NULL;
    HRESULT ret = CreateStreamOnHGlobal(NULL, TRUE, &pStream);
    if(ret == S_OK){
        OutputDebugString(_T("创建内存流成功"));
        LARGE_INTEGER bg = { 0 };

        screen.Save(pStream, Gdiplus::ImageFormatPNG); //保存到内存流中
        pStream->Seek(bg, STREAM_SEEK_SET, NULL); //将指针指向流的开始
        PBYTE pData =(PBYTE) GlobalLock(hMem);
        size_t nsize = GlobalSize(hMem);
        Cpacket pack(6, pData, nsize);
        GlobalUnlock(hMem);
       
    }
   
    /*
    
     for (int i = 0; i < 10 ; i++){
        DWORD tick = GetTickCount64();
        screen.Save(_T("test.png"), Gdiplus::ImageFormatPNG);
        TRACE(_T("保存png屏幕截图耗时： %d ms\n"), GetTickCount64() - tick);
        tick = GetTickCount64();
        screen.Save(_T("test.jpg"), Gdiplus::ImageFormatJPEG);
        TRACE(_T("保存jpg屏幕截图耗时： %d ms\n"), GetTickCount64() - tick);
    }

    */
    pStream->Release();
    screen.ReleaseDC();
    GlobalFree(hMem);
    
    return 0;
}
int main()
    {
        int nRetCode = 0;

        HMODULE hModule = ::GetModuleHandle(nullptr);

        if (hModule != nullptr)
        {
            // 初始化 MFC 并在失败时显示错误
            if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0))
            {
                // TODO: 在此处为应用程序的行为编写代码。
                wprintf(L"错误: MFC 初始化失败\n");
                nRetCode = 1;
            }
            else
            {
                // TODO: 在此处为应用程序的行为编写代码。
                //server;
                // TODO: 在此处为应用程序的行为编写代码。
            /*
            *
            */
               
                int nCmd = 7;

                switch (nCmd) {
                case 1:
                    MakeDriverInfo();
                    break;
                case 2:
                    MakeDirectoryInfo();
                    break;
                case 3:
                    RunFile();
                    break;
                case 4:
                    DownLoadFile(); //下载文件
					break;
                case 5:
                    MouseEvent();
                    break; 
                case 6://发送屏幕内容(发送屏幕截图)
                    sendScreen();
                    break;
                case 7: //锁定机器
                    LockMachine();
                    //Sleep(500);
                    //LockMachine();
                    break;
                case 8: //解锁机器
                    UnlockMachine();
                    break;
                default:
                    break;
                }
                Sleep(5000);
                UnlockMachine();

                while(dlg.m_hWnd != NULL || dlg.m_hWnd != INVALID_HANDLE_VALUE){
                   Sleep(100);
                }
              
            }
        }
        else
        {
            // TODO: 更改错误代码以符合需要
            wprintf(L"错误: GetModuleHandle 失败\n");
            nRetCode = 1;
        }

        return nRetCode;
        }
