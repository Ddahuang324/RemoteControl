// 远控.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include "framework.h"
#include "RemoteControl_server.h"
#include "ServerSocket.h"
#include "direct.h"
#include "io.h"
#include <list>


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
                int nCmd = 1;
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
