#include "pch.h"
#include "RemoteControl_Client.h"
#include "framework.h"
#include "include/Infra/ThreadPool.hpp"
#include "mvc/controller/MainController.h"
#include "mvc/model/FileSystemModel.h"
#include "mvc/model/IOModel.h"
#include "mvc/model/MoniterModel.h"
#include "mvc/model/NetworkModel.h"
#include "mvc/view/RemoteControlViewDlg.h"



#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CRemoteControlClientApp

BEGIN_MESSAGE_MAP(CRemoteControlClientApp, CWinApp)
ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()

// CRemoteControlClientApp 构造

CRemoteControlClientApp::CRemoteControlClientApp() {
  // 支持重新启动管理器
  m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

  // TODO: 在此处添加构造代码，
  // 将所有重要的初始化放置在 InitInstance 中
}

// 唯一的 CRemoteControlClientApp 对象

CRemoteControlClientApp theApp;

// CRemoteControlClientApp 初始化

BOOL CRemoteControlClientApp::InitInstance() {
  // 如果应用程序存在以下情况，Windows XP 上需要 InitCommonControlsEx()
  // 使用 ComCtl32.dll 版本 6 或更高版本来启用可视化方式，
  // 则需要 InitCommonControlsEx()。  否则，将无法创建窗口。
  INITCOMMONCONTROLSEX InitCtrls;
  InitCtrls.dwSize = sizeof(InitCtrls);
  // 将它设置为包括所有要在应用程序中使用的
  // 公共控件类。
  InitCtrls.dwICC = ICC_WIN95_CLASSES;
  InitCommonControlsEx(&InitCtrls);

  CWinApp::InitInstance();

  AfxEnableControlContainer();

  // 创建 shell 管理器，以防对话框包含
  // 任何 shell 树视图控件或 shell 列表视图控件。
  CShellManager *pShellManager = new CShellManager;

  // 激活“Windows Native”视觉管理器，以便在 MFC 控件中启用主题
  CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

  // 标准初始化
  // 如果未使用这些功能并希望减小
  // 最终可执行文件的大小，则应移除下列
  // 不需要的特定初始化例程
  // 更改用于存储设置的注册表项
  // TODO: 应适当修改该字符串，
  // 例如修改为公司或组织名
  SetRegistryKey(_T("应用程序向导生成的本地应用程序"));

  // --- MVC Initialization ---
  // 1. Create ThreadPool
  auto threadPool = std::make_shared<ThreadPool>(4);

  // 2. Create NetworkModel
  auto network = std::make_shared<NetworkModel>();

  // 3. Create other Models
  auto fileSystem = std::make_shared<FileSystemModel>(network, threadPool);
  auto io = std::make_shared<IOModel>(network);
  auto monitor = std::make_shared<MonitorModel>(threadPool);
  monitor->setNetworkModel(network);
  monitor->setIoModel(io);

  // 4. Create View
  RemoteControlViewDlg dlg;
  m_pMainWnd = &dlg;

  // 5. Create Controller
  auto controller =
      std::make_shared<MainController>(network, fileSystem, monitor, io, &dlg);

  // 6. Inject Controller into View
  dlg.SetController(controller);

  INT_PTR nResponse = dlg.DoModal();
  if (nResponse == IDOK) {
    // TODO: 在此放置处理何时用
    //  “确定”来关闭对话框的代码
  } else if (nResponse == IDCANCEL) {
    // TODO: 在此放置处理何时用
    //  “取消”来关闭对话框的代码
  } else if (nResponse == -1) {
    TRACE(traceAppMsg, 0, "警告: 对话框创建失败，应用程序将意外终止。\n");
    TRACE(traceAppMsg, 0,
          "警告: 如果您在对话框上使用 MFC 控件，则无法 #define "
          "_AFX_NO_MFC_CONTROLS_IN_DIALOGS。\n");
  }

  // 删除上面创建的 shell 管理器。
  if (pShellManager != nullptr) {
    delete pShellManager;
  }

#if !defined(_AFXDLL) && !defined(_AFX_NO_MFC_CONTROLS_IN_DIALOGS)
  ControlBarCleanUp();
#endif

  // 由于对话框已关闭，所以将返回 FALSE 以便退出应用程序，
  //  而不是启动应用程序的消息泵。
  return FALSE;
}
