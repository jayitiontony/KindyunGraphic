// =============================================================================
// MainWindow.cpp - KindyunGraphicQt 主窗口实现
// =============================================================================

#include "MainWindow.h"

#include "KindyunGraphic.h"   // KindyunGraphic.dll 的 C 接口

#include <QPainter>
#include <QTimerEvent>
#include <QPaintEvent>

#include <cmath>
#include <cstdio>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


// -----------------------------------------------------------------------------
// 平台相关: 加载 DLL / .so / .dylib
// -----------------------------------------------------------------------------

#if defined(_WIN32)
    #include <windows.h>
    #define KG_LIB_NAME   "KindyunGraphic.dll"
    #define KG_LIB_ENV    "KINDYUNGRAPHIC_PATH"   // 用户可指定自定义路径
    using LibHandle = HMODULE;

    static LibHandle LoadLib() {
        // 1) 优先用环境变量 (用户可指定 DLL 位置)
        if (qEnvironmentVariableIsSet(KG_LIB_ENV)) {
            QString p = qEnvironmentVariable(KG_LIB_ENV);
            return ::LoadLibraryW(reinterpret_cast<LPCWSTR>(p.utf16()));
        }
        // 2) 默认: 让 Windows 在 PATH 里找, 或与 exe 同目录
        return ::LoadLibraryA(KG_LIB_NAME);
    }
    static void* GetSym(LibHandle h, const char* name) {
        return reinterpret_cast<void*>(::GetProcAddress(h, name));
    }
    static void CloseLib(LibHandle h) { if (h) ::FreeLibrary(h); }
#else
    #include <dlfcn.h>
    #define KG_LIB_NAME   "libKindyunGraphic.so"
    #define KG_LIB_ENV    "KINDYUNGRAPHIC_PATH"
    using LibHandle = void*;

    static LibHandle LoadLib() {
        if (qEnvironmentVariableIsSet(KG_LIB_ENV)) {
            return ::dlopen(qEnvironmentVariable(KG_LIB_ENV).toUtf8().constData(), RTLD_NOW);
        }
        return ::dlopen(KG_LIB_NAME, RTLD_NOW);
    }
    static void* GetSym(LibHandle h, const char* name) {
        return ::dlsym(h, name);
    }
    static void CloseLib(LibHandle h) { if (h) ::dlclose(h); }
#endif


// =============================================================================
// 构造 / 析构
// =============================================================================

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {

    setWindowTitle("KindyunGraphic Qt Demo");
    resize(820, 660);   // 留点空间给标题栏

    if (!LoadKindyunGraphic()) {
        setWindowTitle("KindyunGraphic Qt Demo   |   [ERROR: load failed]");
        return;
    }

    // 创建离屏画布 (默认双缓冲, 内部分配 2 个 buffer)
    m_canvas = m_pfnCreate(kCanvasW, kCanvasH);
    if (!m_canvas) {
        setWindowTitle("KindyunGraphic Qt Demo   |   [ERROR: KG_Create failed]");
        UnloadKindyunGraphic();
        return;
    }
    m_bufStride = m_pfnGetStride(m_canvas);

    // 启动 16ms 定时器驱动渲染
    startTimer(kTimerMs);

    // FPS 统计起点
    m_lastFpsTime = std::chrono::steady_clock::now();
}

MainWindow::~MainWindow() {
    if (m_pfnDestroy && m_canvas) {
        m_pfnDestroy(m_canvas);
        m_canvas = nullptr;
    }
    UnloadKindyunGraphic();
}


// =============================================================================
// 加载 / 卸载 KindyunGraphic 动态库
// =============================================================================

bool MainWindow::LoadKindyunGraphic() {
    m_hLib = LoadLib();
    if (!m_hLib) {
        qWarning("Failed to load %s (set KINDYUNGRAPHIC_PATH if not in PATH)",
                 KG_LIB_NAME);
        return false;
    }

    // 拿 4 个核心符号 (其他按需追加)
    m_pfnCreate    = reinterpret_cast<KGCanvas(*)(int, int)>           (GetSym(m_hLib, "KG_Create"));
    m_pfnDestroy   = reinterpret_cast<void(*)(KGCanvas)>              (GetSym(m_hLib, "KG_Destroy"));
    m_pfnGetData   = reinterpret_cast<unsigned char*(*)(KGCanvas)>     (GetSym(m_hLib, "KG_GetData"));
    m_pfnGetStride = reinterpret_cast<int(*)(KGCanvas)>                (GetSym(m_hLib, "KG_GetStride"));

    if (!m_pfnCreate || !m_pfnDestroy || !m_pfnGetData || !m_pfnGetStride) {
        qWarning("Missing one of KG_Create/KG_Destroy/KG_GetData/KG_GetStride");
        CloseLib(m_hLib);
        m_hLib = nullptr;
        return false;
    }
    return true;
}

void MainWindow::UnloadKindyunGraphic() {
    if (m_hLib) {
        CloseLib(m_hLib);
        m_hLib = nullptr;
    }
    m_pfnCreate = nullptr;
    m_pfnDestroy = nullptr;
    m_pfnGetData = nullptr;
    m_pfnGetStride = nullptr;
}


// =============================================================================
// 定时器: 每 16ms 触发, 调 RenderOneFrame + UpdateFps + 触发 paintEvent
// =============================================================================

void MainWindow::timerEvent(QTimerEvent* /*event*/) {
    RenderOneFrame();     // 内存绘制 (cairo, 含计时)
    UpdateFps();          // 统计 FPS + 平均 render ms, 写标题栏
    update();             // 请求 Qt 在下一帧调 paintEvent (把 buffer 画到 widget)
}


// =============================================================================
// 渲染一帧 (与 KindyunGraphicMfc/MainFrm.cpp 的 RenderOneFrame 1:1)
// =============================================================================

void MainWindow::RenderOneFrame() {
    if (!m_canvas) return;
    KGCanvas c = m_canvas;

    // ★计时起点: 从这里到 KG_Flush 结束, 都是"内存绘制"时间,
    //   不含 paintEvent 把 buffer 画到 widget 的时间。
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

    // 5) 文字
    KG_SetFont(c, "Microsoft YaHei", 22.0, 1, 0);
    KG_SetSourceColor(c, 1.0, 0.85, 0.40, 1.0);
    KG_DrawText(c, 20, 42, "KindyunGraphic + Qt");
    KG_SetFont(c, "Microsoft YaHei", 13.0, 0, 0);
    KG_SetSourceColor(c, 0.85, 0.85, 0.85, 1.0);
    KG_DrawText(c, 20, 64, "Zero-Copy 渲染, 实时显示 FPS");

    // 6) 关键: 写回用户 buffer (触发 swap)
    KG_Flush(c);

    // ★计时终点
    const auto renderEnd = std::chrono::steady_clock::now();
    m_totalRenderMs +=
        std::chrono::duration<double, std::milli>(renderEnd - renderStart).count();
    ++m_renderSamples;

    // 7) 时间步进
    m_t += 0.05;
}


// =============================================================================
// FPS 统计 + 标题栏 (与 MFC 一致)
// =============================================================================

void MainWindow::UpdateFps() {
    ++m_framesSinceFps;
    const auto now = std::chrono::steady_clock::now();
    const double elapsed =
        std::chrono::duration<double>(now - m_lastFpsTime).count();
    if (elapsed >= 0.5) {
        m_currentFps   = m_framesSinceFps / elapsed;
        m_framesSinceFps = 0;
        m_lastFpsTime  = now;

        m_currentRenderMs = (m_renderSamples > 0)
                            ? (m_totalRenderMs / m_renderSamples)
                            : 0.0;
        m_totalRenderMs = 0.0;
        m_renderSamples = 0;

        setWindowTitle(QString::asprintf(
            "KindyunGraphic Qt Demo   |   FPS: %.1f   |   Render: %.2f ms",
            m_currentFps, m_currentRenderMs));
    }
}


// =============================================================================
// 绘制: 把 KindyunGraphic 的 read buffer 用 QPainter 画到 widget
//
// 关键: 用 QImage wrap read buffer (零拷贝), QPainter::drawImage 内部
//       会按 QImage 的 format + bytesPerLine 解析, 不需要复制像素。
//       Format_ARGB32 在 Windows (little-endian) 上 = 内存布局 BGRA,
//       跟 cairo CAIRO_FORMAT_ARGB32 一致, 不需要做字节序转换。
// =============================================================================

void MainWindow::paintEvent(QPaintEvent* /*event*/) {
    if (!m_canvas || !m_pfnGetData) return;

    unsigned char* bufPtr = m_pfnGetData(m_canvas);
    if (!bufPtr) return;

    // 重新 wrap read buffer (双缓冲 swap 后指针变了, 每次 paint 都要重 wrap)
    m_canvasImage = QImage(bufPtr, kCanvasW, kCanvasH, m_bufStride,
                           QImage::Format_ARGB32);

    QPainter p(this);
    p.fillRect(rect(), QColor(0, 0, 0));   // 等比例居中留下的黑边

    // 等比例缩放居中
    const QSize dst = size();
    const double srcAspect = double(kCanvasW) / kCanvasH;
    const double dstAspect = double(dst.width()) / dst.height();
    int drawW, drawH, drawX, drawY;
    if (srcAspect > dstAspect) {
        drawW = dst.width();
        drawH = int(dst.width() / srcAspect);
        drawX = 0;
        drawY = (dst.height() - drawH) / 2;
    } else {
        drawH = dst.height();
        drawW = int(dst.height() * srcAspect);
        drawY = 0;
        drawX = (dst.width() - drawW) / 2;
    }
    p.drawImage(QRect(drawX, drawY, drawW, drawH), m_canvasImage);
}