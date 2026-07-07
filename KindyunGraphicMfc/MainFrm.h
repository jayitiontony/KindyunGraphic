// =============================================================================
// MainFrm.h
//
// MFC 主窗口: 加载 KindyunGraphic.dll, 在客户区里用 cairo 画动画,
// 实时显示 FPS 在标题栏上。
// =============================================================================

#pragma once

#include "KindyunGraphic.h"   // KindyunGraphic.dll 的 C 接口
#include <chrono>
#include <vector>


class CMainFrame : public CFrameWnd {
public:
    CMainFrame() noexcept;
    virtual ~CMainFrame();

protected:
    DECLARE_DYNCREATE(CMainFrame)

    // 重写
public:
    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

    // 实现
public:
#ifdef _DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

    // 消息处理
protected:
    DECLARE_MESSAGE_MAP()

    afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);    // 拦截背景擦除, 避免每帧闪烁
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnDestroy();
    afx_msg void OnSize(UINT nType, int cx, int cy);

private:
    // 加载 KindyunGraphic.dll
    HMODULE               m_hDll    = nullptr;
    PFN_KG_Create         m_pfnCreate = nullptr;
    PFN_KG_Destroy        m_pfnDestroy = nullptr;
    PFN_KG_GetData        m_pfnGetData = nullptr;
    PFN_KG_GetStride      m_pfnGetStride = nullptr;
    // (其他函数指针按需拿)

    // 离屏画布 (固定 800x600, 用客户区尺寸缩放显示)
    // 使用 KG_Create (双缓冲模式), 内部已经管 2 个 buffer,
    // 我们只需要拿 read buffer 指针就能用 GDI 显示
    static constexpr int   kCanvasW = 800;
    static constexpr int   kCanvasH = 600;
    KGCanvas               m_canvas = nullptr;
    unsigned char*         m_bufPtr    = nullptr;   // read buffer 指针
    int                    m_bufStride = 0;        // 一行的字节数

    // 动画时间轴
    double                 m_t = 0.0;

    // FPS 统计
    std::chrono::steady_clock::time_point m_lastFpsTime;
    int                    m_framesSinceFps = 0;
    double                 m_currentFps    = 0.0;

    // 仅内存绘制耗时 (毫秒, 不含 BlitToClient 的 StretchDIBits 时间)
    double                 m_totalRenderMs = 0.0;   // 累计
    int                    m_renderSamples = 0;     // 帧数
    double                 m_currentRenderMs = 0.0; // 当前平均值

    // 内部函数
    void RenderOneFrame();
    void UpdateFps();
    void BlitToClient(CDC& dc);
};
