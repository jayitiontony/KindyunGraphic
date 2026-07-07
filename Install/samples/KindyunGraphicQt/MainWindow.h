// =============================================================================
// MainWindow.h - KindyunGraphicQt 主窗口
//
// 与 KindyunGraphicMfc 1:1 对应, 同样的功能:
//   - 16ms 定时器触发渲染 (≈60 FPS)
//   - 标题栏实时显示 FPS + 内存绘制平均耗时 (ms)
//   - 0.5s 统计一次 FPS
//   - 双缓冲由 KG_Create 默认开启, 不需要自己管理 buffer
// =============================================================================

#pragma once

#include <QMainWindow>
#include <QImage>
#include <chrono>

// 前向声明, 避免把 KindyunGraphic.h 漏出来 (它定义 KGCanvas / PFN_* 类型)
typedef struct KGCanvas_t* KGCanvas;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    // Qt 消息处理 (MFC 对应 ON_WM_TIMER / ON_WM_PAINT)
    void timerEvent(QTimerEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    // ===== 离屏画布参数 =====
    static constexpr int kCanvasW   = 800;
    static constexpr int kCanvasH   = 600;
    static constexpr int kTimerMs   = 16;   // ≈60 FPS

    // ===== 渲染状态 =====
    void RenderOneFrame();    // 用 cairo 画一帧 (含计时)
    void UpdateFps();         // 0.5s 一次, 计算 FPS + 平均 render ms

    // ===== KindyunGraphic.dll 加载 =====
    bool LoadKindyunGraphic();          // 加载 + GetProcAddress
    void UnloadKindyunGraphic();        // 释放

    // ===== 成员变量 =====
    void*       m_hLib           = nullptr;   // HMODULE 跨平台用 void*

    // C 接口函数指针 (PFN_KG_* 类型由 KindyunGraphic.h 定义)
    KGCanvas  (*m_pfnCreate)     (int w, int h)                                          = nullptr;
    void      (*m_pfnDestroy)    (KGCanvas)                                              = nullptr;
    unsigned char* (*m_pfnGetData)(KGCanvas)                                             = nullptr;
    int       (*m_pfnGetStride)  (KGCanvas)                                              = nullptr;

    KGCanvas   m_canvas          = nullptr;
    int        m_bufStride       = 0;

    // 用 QImage wrap KindyunGraphic 的 read buffer (零拷贝)
    // QImage 不持有数据所有权, 因此不能随便 detach, paintEvent 结束就失效
    QImage     m_canvasImage;        // 每次 paintEvent 重建, wrap 当前 read buffer

    // 动画
    double     m_t               = 0.0;

    // FPS 统计
    std::chrono::steady_clock::time_point m_lastFpsTime;
    int        m_framesSinceFps  = 0;
    double     m_currentFps      = 0.0;

    // 仅内存绘制耗时 (毫秒, 不含 paintEvent 把 buffer 画到 widget 的时间)
    double     m_totalRenderMs   = 0.0;
    int        m_renderSamples   = 0;
    double     m_currentRenderMs = 0.0;
};