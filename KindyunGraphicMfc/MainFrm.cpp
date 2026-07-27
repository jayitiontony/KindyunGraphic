// =============================================================================
// MainFrm.cpp
// =============================================================================

#include "stdafx.h"
#include "KindyunGraphicMfc.h"
#include "MainFrm.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef _DEBUG
// ==========================================================================
// 调试用: 抓"谁在分配 2064 字节"的分配钩子
// ==========================================================================
// 用法:
//   1. 编译 Debug|x64 后 F5 启动 (必须命中断点才能看到调用栈)
//   2. 程序在 _cairo_malloc(2064) 处 __debugbreak, 看 Call Stack
//      就能定位是 cairo / mfc140ud / 第三方 dll 哪个分配的
//   3. 找到了就注释掉这段, 或把目标 allocNumber 写进 g_breakOnAllocRequest
//      (用 _CrtSetBreakAlloc, 走的是更标准的 hook 流程)
//
// 配套环境变量 / 宏:
//   - 如果只想断"crtdbg dump 报告的某个 alloc 编号", 把 g_breakOnAllocSize
//     改成 0 并设置 g_breakOnAllocRequest = N (从 dump 的 {NNN} 抄过来)
//   - 5 个泄漏块的编号是 362, 363, 364, 365, 366 (按 dump 顺序)
static LONG g_breakOnAllocSize      = 2064;  // 按 size 匹配; 0 = 关闭
static LONG g_breakOnAllocRequest   = 0;     // 按 _CrtMemBlockHeader.lRequest 匹配; 0 = 关闭
static LONG g_breakOnAllocHitsLeft  = 1;     // 命中几次后就不再断 (避免一次启动触发 N 次)
static LONG g_breakOnAllocHitsDone  = 0;     // 已命中次数

static int MyLeakAllocHook(int allocType, void* /*userData*/,
                            size_t size, int /*blockType*/,
                            long request,
                            const unsigned char* /*filename*/,
                            int /*lineNumber*/)
{
    if (allocType != _HOOK_ALLOC)
        return 1;  // 1 = 让 CRT 继续正常分配

  /*  if (g_breakOnAllocSize && size == (size_t)g_breakOnAllocSize)
        goto hit;
    if (g_breakOnAllocRequest && request == g_breakOnAllocRequest)
        goto hit;*/

    return 1;

//hit:
//    if (g_breakOnAllocHitsDone < g_breakOnAllocHitsLeft) {
//        g_breakOnAllocHitsDone++;
//        // __debugbreak 会在 debug build 里触发 IDE 断点; 不会卡死 CRT
//        // (在 cairo 内部 alloc 钩子里断相对安全, cairo 这边没有继续持有
//        //  critical section 的逻辑, 断在分配前相当于让 malloc 还没返回)
//        __debugbreak();
//    }
    return 1;
}
#endif // _DEBUG


#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CMainFrame

IMPLEMENT_DYNCREATE(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
    ON_WM_CREATE()
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_TIMER()
    ON_WM_DESTROY()
    ON_WM_SIZE()
END_MESSAGE_MAP()


CMainFrame::CMainFrame() noexcept {
}

CMainFrame::~CMainFrame() {
}


BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs) {
    if (!CFrameWnd::PreCreateWindow(cs)) return FALSE;

    // 800x600 客户端尺寸 + 标准窗口边框/标题
    cs.style &= ~FWS_ADDTOTITLE;
    cs.lpszName = _T("KindyunGraphic - MFC 动画演示");
    cs.cx = 820;
    cs.cy = 640;
    return TRUE;
}


// =============================================================================
// 生命周期
// =============================================================================

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) {
    if (CFrameWnd::OnCreate(lpCreateStruct) == -1) return -1;

    // 1) 加载 KindyunGraphic.dll
    m_hDll = ::LoadLibrary(_T("KindyunGraphic.dll"));
    if (!m_hDll) {
        ::MessageBox(m_hWnd, _T("找不到 KindyunGraphic.dll\n请把它放到 exe 同目录"),
                     _T("错误"), MB_ICONERROR);
        return -1;
    }
    m_pfnCreate     = (PFN_KG_Create)    ::GetProcAddress(m_hDll, "KG_Create");
    m_pfnDestroy    = (PFN_KG_Destroy)   ::GetProcAddress(m_hDll, "KG_Destroy");
    m_pfnGetData    = (PFN_KG_GetData)   ::GetProcAddress(m_hDll, "KG_GetData");
    m_pfnGetStride  = (PFN_KG_GetStride) ::GetProcAddress(m_hDll, "KG_GetStride");
    // [调试] 可选; 老版本 DLL 没这个符号也不报错
    m_pfnDebugReset = (PFN_KG_DebugResetStaticData)
                      ::GetProcAddress(m_hDll, "KG_DebugResetStaticData");
    if (!m_pfnCreate || !m_pfnDestroy || !m_pfnGetData || !m_pfnGetStride) {
        ::MessageBox(m_hWnd, _T("KindyunGraphic.dll 缺符号 (KG_Create/KG_Destroy/KG_GetData/KG_GetStride)"),
                     _T("错误"), MB_ICONERROR);
        return -1;
    }

    // 2) 用 KG_Create (默认开双缓冲, 内部分配 2 个 buffer, 无需自己管理)
    m_canvas = m_pfnCreate(kCanvasW, kCanvasH);
    if (!m_canvas) {
        ::MessageBox(m_hWnd, _T("KG_Create 失败"), _T("错误"), MB_ICONERROR);
        return -1;
    }
    m_bufStride = m_pfnGetStride(m_canvas);

    // 3) 启动 60Hz 定时器 (~16ms 一帧)
    m_lastFpsTime = std::chrono::steady_clock::now();
    ::SetTimer(m_hWnd, 1, 16, nullptr);

#ifdef _DEBUG
    // 注册"分配 2064 字节就断"hook, 用于定位剩余的 5 个泄漏
    // (F5 启动会在 cairo 内部 _cairo_* 分配 2064 字节时断, 看 Call Stack)
    _CrtSetAllocHook(MyLeakAllocHook);
#endif

    return 0;
}

void CMainFrame::OnDestroy() {
    ::KillTimer(m_hWnd, 1);

#ifdef _DEBUG
    // 卸载分配 hook, 避免影响之后的内存 dump
    _CrtSetAllocHook(nullptr);
#endif

    if (m_canvas && m_pfnDestroy) {
        m_pfnDestroy(m_canvas);
        m_canvas = nullptr;
    }
    if (m_hDll) {
        // ★ 在 FreeLibrary 之前先调一次 KG_DebugResetStaticData:
        //   1) 把 cairo 进程内的静态缓存 (toy_font_face / scaled_font_map /
        //      unscaled_font_map / win32_device / pixman global_glyph_cache 等)
        //      全部释放, 避免 _CrtDumpMemoryLeaks 报告一堆"漏掉"的 cairo 内部对象
        //   2) 必须在所有 cairo_t / surface 都销毁之后 (上面 m_pfnDestroy 已调)
        //   3) FreeLibrary 之后再调就晚了, 因为 cairo 静态状态所在的那块代码
        //      已经被卸载, 函数指针会指向已释放的内存
        if (m_pfnDebugReset) {
            m_pfnDebugReset();
        }

        ::FreeLibrary(m_hDll);
        m_hDll = nullptr;
    }
    CFrameWnd::OnDestroy();
}


// =============================================================================
// 渲染
// =============================================================================

void CMainFrame::OnPaint() {
    CPaintDC dc(this);
    BlitToClient(dc);
}

// 关键: 拦截背景擦除, 阻止 MFC 默认在 OnPaint 前用白/灰背景刷一次客户区
// 不拦截的话, 每帧都会看到 "擦白 → 画黑底 → StretchDIBits" 三步切换, 视觉上闪烁
BOOL CMainFrame::OnEraseBkgnd(CDC* /*pDC*/) {
    return TRUE;   // 告诉 Windows 我们自己处理背景, 不要默认擦除
}

void CMainFrame::OnSize(UINT nType, int cx, int cy) {
    CFrameWnd::OnSize(nType, cx, cy);
    Invalidate();
}

void CMainFrame::OnTimer(UINT_PTR nIDEvent) {
    if (nIDEvent == 1) {
        RenderOneFrame();
        UpdateFps();
        Invalidate();   // 触发 OnPaint 把 buffer 画到窗口
    } else {
        CFrameWnd::OnTimer(nIDEvent);
    }
}


// 绘制一帧到 m_buf (KindyunGraphic.dll / cairo 渲染)
void CMainFrame::RenderOneFrame() {
    KGCanvas c = m_canvas;
    if (!c) return;

    // ★计时起点: 从这里到 KG_Flush 结束, 都是"内存绘制"时间,
    //   不含 BlitToClient 把 buffer 拷到窗口 DC 的时间。
    const auto renderStart = std::chrono::steady_clock::now();

    // 1) 清屏 (深蓝灰)
    KG_Clear(c, 0.10, 0.12, 0.18, 1.0);

    // 2) 移动的小球 (沿 sin/cos 椭圆轨道)
    {
        double cx = kCanvasW * 0.5 + std::cos(m_t)        * 240.0;
        double cy = kCanvasH * 0.5 + std::sin(m_t * 1.3)  * 160.0;
        KG_SetSourceColor(c, 0.40, 0.80, 1.00, 1.0);
        KG_DrawCircle(c, cx, cy, 30.0, 1);
        KG_SetSourceColor(c, 1.0, 1.0, 1.0, 1.0);
        KG_SetLineWidth(c, 1.5);
        KG_DrawCircle(c, cx, cy, 30.0, 0);
    }

    // 3) 6 个旋转的方块
    KG_SetLineWidth(c, 1.0);
    for (int i = 0; i < 6; ++i) {
        double a = m_t + i * (2.0 * M_PI / 6.0);
        double x = kCanvasW * 0.5 + std::cos(a) * 180.0;
        double y = kCanvasH * 0.5 + std::sin(a) * 120.0;
        KG_Save(c);
        KG_Translate(c, x, y);
        KG_Rotate(c, a);
        KG_SetSourceColor(c, 1.0, 0.60, 0.20, 0.85);
        KG_DrawRectangle(c, -20, -20, 40, 40, 1);
        KG_SetSourceColor(c, 0.0, 0.0, 0.0, 1.0);
        KG_DrawRectangle(c, -20, -20, 40, 40, 0);
        KG_Restore(c);
    }

    // 4) 螺旋线 (60 段)
    KG_SetLineWidth(c, 2.0);
    KG_SetSourceColor(c, 0.50, 1.0, 0.70, 1.0);
    const int spiralSegments = 60;
    for (int i = 0; i < spiralSegments; ++i) {
        const double a1 = (i     ) * 0.3 + m_t * 0.5;
        const double a2 = (i + 1 ) * 0.3 + m_t * 0.5;
        const double r1 = 50.0 + i * 1.5;
        const double r2 = 50.0 + (i + 1) * 1.5;
        const double x1 = kCanvasW * 0.5 + std::cos(a1) * r1;
        const double y1 = kCanvasH * 0.5 + std::sin(a1) * r1;
        const double x2 = kCanvasW * 0.5 + std::cos(a2) * r2;
        const double y2 = kCanvasH * 0.5 + std::sin(a2) * r2;
        KG_DrawLine(c, x1, y1, x2, y2);
    }

    // 5) 文字 (在 buffer 上)
    KG_SetFont(c, "Microsoft YaHei", 22.0, 1, 0);
    KG_SetSourceColor(c, 1.0, 0.85, 0.40, 1.0);
    KG_DrawText(c, 20, 42, "KindyunGraphic + MFC");
    KG_SetFont(c, "Microsoft YaHei", 13.0, 0, 0);
    KG_SetSourceColor(c, 0.85, 0.85, 0.85, 1.0);
    KG_DrawText(c, 20, 64, "Zero-Copy 渲染, 实时显示 FPS");

    // 6) 关键: 写回用户 buffer
    KG_Flush(c);

    // ★计时终点: 累加本次"纯内存绘制"耗时到 m_totalRenderMs, UpdateFps 计算平均值
    const auto renderEnd = std::chrono::steady_clock::now();
    m_totalRenderMs +=
        std::chrono::duration<double, std::milli>(renderEnd - renderStart).count();
    ++m_renderSamples;

    // 7) 时间步进
    m_t += 0.05;
}


// 把 buffer 画到客户区 (BGRA DIB)
void CMainFrame::BlitToClient(CDC& dc) {
    if (!m_canvas) return;

    // 每次绘制都重新拿 read buffer 指针 (双缓冲 swap 后指针指向变了)
    unsigned char* bufPtr = m_pfnGetData(m_canvas);
    if (!bufPtr) return;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = kCanvasW;
    bmi.bmiHeader.biHeight      = -kCanvasH;   // 顶向下 DIB
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage   = 0;

    CRect rc;
    GetClientRect(&rc);
    const int dstW = rc.Width();
    const int dstH = rc.Height();
    if (dstW <= 0 || dstH <= 0) return;

    // 等比例缩放居中 (黑边留给系统 / 父窗口背景)
    const double srcAspect = static_cast<double>(kCanvasW) / kCanvasH;
    const double dstAspect = static_cast<double>(dstW) / dstH;
    int drawW, drawH, drawX, drawY;
    if (srcAspect > dstAspect) {
        drawW = dstW; drawH = static_cast<int>(dstW / srcAspect);
        drawX = 0;    drawY = (dstH - drawH) / 2;
    } else {
        drawH = dstH; drawW = static_cast<int>(dstH * srcAspect);
        drawY = 0;    drawX = (dstW - drawW) / 2;
    }

    // 不再 FillSolidRect 画黑底 (避免与 StretchDIBits 一起造成"擦一次画一次"的闪烁)
    // 等比例居中后留下的黑边由 OnEraseBkgnd 返回 TRUE 后的系统层处理 (维持上次内容)
    ::StretchDIBits(dc.GetSafeHdc(),
                    drawX, drawY, drawW, drawH,
                    0, 0, kCanvasW, kCanvasH,
                    bufPtr,
                    &bmi, DIB_RGB_COLORS, SRCCOPY);
}


// =============================================================================
// FPS 统计
// =============================================================================

void CMainFrame::UpdateFps() {
    ++m_framesSinceFps;
    const auto now = std::chrono::steady_clock::now();
    const double elapsed =
        std::chrono::duration<double>(now - m_lastFpsTime).count();
    if (elapsed >= 0.5) {
        m_currentFps   = m_framesSinceFps / elapsed;
        m_framesSinceFps = 0;
        m_lastFpsTime  = now;

        // 平均每帧"纯内存绘制"耗时 (毫秒, 不含 BlitToClient 往窗口上绘制的时间)
        m_currentRenderMs = (m_renderSamples > 0)
                            ? (m_totalRenderMs / m_renderSamples)
                            : 0.0;
        m_totalRenderMs = 0.0;
        m_renderSamples = 0;

        // 在标题栏显示 FPS + 渲染耗时 (ms)
        CString title;
        title.Format(_T("KindyunGraphic MFC Demo   |   FPS: %.1f   |   Render: %.2f ms"),
                     m_currentFps, m_currentRenderMs);
        SetWindowText(title);
    }
}


#ifdef _DEBUG
void CMainFrame::AssertValid() const {
    CFrameWnd::AssertValid();
}
void CMainFrame::Dump(CDumpContext& dc) const {
    CFrameWnd::Dump(dc);
}
#endif