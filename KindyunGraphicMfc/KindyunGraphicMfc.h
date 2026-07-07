// =============================================================================
// KindyunGraphicMfc.h
//
// MFC App 类声明
// =============================================================================

#pragma once

#ifndef __AFXWIN_H__
    #error "在包含 pch.h 之前先包含 stdafx.h"
#endif

#include "resource.h"   // 主符号


// CKindyunGraphicMfcApp:
// 参见 KindyunGraphicMfc.cpp 实现此类
//

class CKindyunGraphicMfcApp : public CWinApp {
public:
    CKindyunGraphicMfcApp() noexcept;

    // 重写
public:
    virtual BOOL InitInstance();
    virtual int  ExitInstance();

    // 实现
    DECLARE_MESSAGE_MAP()
};

extern CKindyunGraphicMfcApp theApp;