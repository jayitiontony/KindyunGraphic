// =============================================================================
// KindyunGraphicMfc.cpp
//
// MFC App 实现
// =============================================================================

#include "stdafx.h"
#include "KindyunGraphicMfc.h"
#include "MainFrm.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CKindyunGraphicMfcApp

BEGIN_MESSAGE_MAP(CKindyunGraphicMfcApp, CWinApp)
END_MESSAGE_MAP()


// CKindyunGraphicMfcApp 构造

CKindyunGraphicMfcApp::CKindyunGraphicMfcApp() noexcept {
    // 在这里放置所有初始化代码
}


// 唯一的 CKindyunGraphicMfcApp 对象

CKindyunGraphicMfcApp theApp;


// CKindyunGraphicMfcApp 初始化

BOOL CKindyunGraphicMfcApp::InitInstance() {
    CWinApp::InitInstance();

    // 创建主窗口
    CMainFrame* pFrame = new CMainFrame;
    if (!pFrame) return FALSE;
    m_pMainWnd = pFrame;
    pFrame->LoadFrame(IDR_MAINFRAME);
    pFrame->ShowWindow(SW_SHOW);
    pFrame->UpdateWindow();
    return TRUE;
}

int CKindyunGraphicMfcApp::ExitInstance() {
    return CWinApp::ExitInstance();
}