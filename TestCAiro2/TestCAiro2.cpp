// =============================================================================
// TestCAiro2.cpp
//
// CairoCanvas 封装类的演示程序。
//
// 程序会在内存中创建一张 1000x700 的画布, 分多个区域演示 CairoCanvas
// 支持的全部功能 (直线 / 曲线 / 圆弧 / 圆 / 椭圆 / 描迹线 / 描迹闭合
// 多边形 / 矩形 / 文字 / 颜色 / 线宽 / 字体 等), 然后把结果保存为
// PNG 文件 (同时也保存一份 SVG 用于对比)。
//
// 程序运行结束后, 当前目录下会生成:
//   - demo_output.png    位图结果
//   - demo_output.svg    矢量结果 (cairo 生成的真正矢量 SVG)
//   - demo_output.pdf    矢量结果 (PDF)
//   - demo_output.ps     矢量结果 (PostScript)
// =============================================================================

#include "CairoCanvas.h"

#include <pixman.h>      // pixman_fini — 释放 pixman 全局 implementation cache

// 调试辅助: -leaktrace 模式需要 dbghelp 解析调用栈, 默认关闭.
// 启用方法: 在 Project Property C/C++ Preprocessor 加 LEAKTRACE_ENABLED=1,
//   或将下面 #if 0 改成 #if 1.
#if 0
#include <windows.h>     // GetCurrentProcess (for -leaktrace mode)
#include <dbghelp.h>     // SymInitialize / SymFromAddr / RtlCaptureStackBackTrace
#pragma comment(lib, "dbghelp.lib")
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace memgc;

namespace {

/// @brief 把弧度转角度, 仅用于演示输出。
inline double RadToDeg(double rad) {
    return rad * 180.0 / M_PI;
}

/// @brief 通用绘制辅助: 在指定位置写一行标题文字 (黑色, 大字号)。
void DrawTitle(CairoCanvas& canvas, double x, double y, const std::string& text) {
    canvas.SetSourceColor(Color::Black());
    canvas.SetFont(FontStyle("Microsoft YaHei", 18, true, false));
    canvas.DrawTextUTF8(x, y, text);

    // 标题下加一条浅灰色分隔线
    canvas.SetSourceColor(Color::Gray(0.7));
    canvas.SetLineWidth(1.0);
    canvas.DrawLine(x, y + 8, x + 300, y + 8);
}

/// @brief 通用绘制辅助: 在指定位置写一行说明文字 (深灰色, 小字号)。
void DrawCaption(CairoCanvas& canvas, double x, double y, const std::string& text) {
    canvas.SetSourceColor(Color::FromRGB(80, 80, 80));
    canvas.SetFont(FontStyle("Microsoft YaHei", 11, false, true));
    canvas.DrawTextUTF8(x, y, text);
}

/// @brief 演示 1: 直线 (颜色, 线宽, 线帽, 虚线)
void DemoLine(CairoCanvas& canvas, double x0, double y0) {
    DrawTitle(canvas, x0, y0, "[1] 直线 / 线宽 / 线帽 / 虚线");

    double y = y0 + 40;
    canvas.SetLineCap(LineCap::Butt);
    for (int i = 0; i < 6; ++i) {
        canvas.SetSourceColor(Color::FromRGB(30 * i, 80, 200 - 20 * i));
        canvas.SetLineWidth(1.0 + i * 1.5);
        canvas.DrawLine(x0, y, x0 + 220, y);
        y += 18 + i * 1.5;
    }

    y += 8;
    DrawCaption(canvas, x0, y, "不同线宽 (1 / 2.5 / 4 / 5.5 / 7 / 8.5 px)");
    y += 22;

    // 演示线帽
    canvas.SetLineWidth(10.0);
    canvas.SetSourceColor(Color::FromRGB(60, 60, 60));
    canvas.SetLineCap(LineCap::Butt);
    canvas.DrawLine(x0 + 10, y, x0 + 60, y);
    canvas.SetLineCap(LineCap::Round);
    canvas.DrawLine(x0 + 90, y, x0 + 140, y);
    canvas.SetLineCap(LineCap::Square);
    canvas.DrawLine(x0 + 170, y, x0 + 220, y);
    canvas.SetLineCap(LineCap::Butt);
    y += 22;
    DrawCaption(canvas, x0, y, "线帽: BUTT / ROUND / SQUARE");
    y += 24;

    // 演示虚线
    canvas.SetLineWidth(2.0);
    canvas.SetSourceColor(Color::Red());
    canvas.SetDash({8.0, 4.0});
    canvas.DrawLine(x0, y, x0 + 220, y);
    canvas.SetSourceColor(Color::Blue());
    canvas.SetDash({2.0, 4.0});
    canvas.DrawLine(x0, y + 10, x0 + 220, y + 10);
    canvas.SetSourceColor(Color::Green());
    canvas.SetDash({12.0, 4.0, 2.0, 4.0});
    canvas.DrawLine(x0, y + 20, x0 + 220, y + 20);
    canvas.ClearDash();
    y += 40;
    DrawCaption(canvas, x0, y, "虚线模式: 实-空 / 短虚线 / 长-短组合");
}

/// @brief 演示 2: 矩形 (填充 + 描边)
void DemoRectangle(CairoCanvas& canvas, double x0, double y0) {
    DrawTitle(canvas, x0, y0, "[2] 矩形");

    double y = y0 + 40;
    // 实心矩形 (填充)
    canvas.SetSourceColor(Color::FromRGB(255, 180, 80));
    canvas.DrawRectangle(x0, y, 80, 50, true);
    // 空心矩形
    canvas.SetSourceColor(Color::Black());
    canvas.SetLineWidth(2.0);
    canvas.DrawRectangle(x0 + 100, y, 80, 50, false);
    // 半透明填充 + 描边
    canvas.SetSourceColor(Color::FromRGB(80, 160, 240, 180));
    canvas.DrawRectangle(x0 + 200, y, 80, 50, true);
    canvas.SetSourceColor(Color::FromRGB(20, 60, 140));
    canvas.SetLineWidth(1.5);
    canvas.DrawRectangle(x0 + 200, y, 80, 50, false);

    y += 70;
    DrawCaption(canvas, x0, y, "填充 / 描边 / 半透明 + 描边");
}

/// @brief 演示 3: 圆和椭圆
void DemoCircleEllipse(CairoCanvas& canvas, double x0, double y0) {
    DrawTitle(canvas, x0, y0, "[3] 圆 / 椭圆 / 旋转");

    double y = y0 + 40;
    // 不同颜色 / 填充的圆
    canvas.SetSourceColor(Color::Red());
    canvas.DrawCircle(x0 + 30, y + 30, 30, true);
    canvas.SetSourceColor(Color::Green());
    canvas.DrawCircle(x0 + 90, y + 30, 30, true);
    canvas.SetSourceColor(Color::Blue());
    canvas.DrawCircle(x0 + 150, y + 30, 30, true);
    canvas.SetSourceColor(Color::Black());
    canvas.SetLineWidth(2.0);
    canvas.DrawCircle(x0 + 210, y + 30, 30, false);

    y += 80;
    DrawCaption(canvas, x0, y, "圆 (填充红/绿/蓝, 描边黑)");
    y += 24;

    // 椭圆 (旋转 0 / 30 / 60 度)
    canvas.SetSourceColor(Color::FromRGB(255, 120, 180));
    canvas.DrawEllipse(x0 + 30, y + 25, 35, 15, 0.0, true);
    canvas.SetSourceColor(Color::FromRGB(120, 200, 80));
    canvas.DrawEllipse(x0 + 100, y + 25, 35, 15, M_PI / 6.0, true);
    canvas.SetSourceColor(Color::FromRGB(100, 150, 240));
    canvas.DrawEllipse(x0 + 170, y + 25, 35, 15, M_PI / 3.0, true);
    canvas.SetSourceColor(Color::Black());
    canvas.SetLineWidth(2.0);
    canvas.DrawEllipse(x0 + 240, y + 25, 35, 15, M_PI / 4.0, false);

    y += 60;
    DrawCaption(canvas, x0, y, "椭圆 (旋转角 0 / 30 / 60 度, 最后一枚仅描边)");
}

/// @brief 演示 4: 圆弧 (起止角)
void DemoArc(CairoCanvas& canvas, double x0, double y0) {
    DrawTitle(canvas, x0, y0, "[4] 圆弧 (圆心 + 半径 + 起止角)");

    double cx = x0 + 50;
    double cy = y0 + 80;
    double r  = 40.0;

    // 画一个完整的圆作为参考 (浅灰)
    canvas.SetSourceColor(Color::Gray(0.85));
    canvas.SetLineWidth(1.0);
    canvas.DrawCircle(cx, cy, r, false);

    // 1/4 圆弧 (0 到 π/2)
    canvas.SetSourceColor(Color::Red());
    canvas.SetLineWidth(4.0);
    canvas.DrawArc(cx, cy, r, 0.0, M_PI / 2.0);
    // 半圆弧 (π/2 到 3π/2)
    canvas.SetSourceColor(Color::Blue());
    canvas.SetLineWidth(4.0);
    canvas.DrawArc(cx + 110, cy, r, M_PI / 2.0, 3.0 * M_PI / 2.0);
    // 3/4 圆弧
    canvas.SetSourceColor(Color::Green());
    canvas.SetLineWidth(4.0);
    canvas.DrawArc(cx + 220, cy, r, M_PI / 4.0, M_PI / 4.0 + 3.0 * M_PI / 2.0);

    double y = cy + r + 24;
    DrawCaption(canvas, x0, y, "1/4 圆弧 / 半圆弧 / 3/4 圆弧 (灰色虚线参考圆)");
}

/// @brief 演示 5: 贝塞尔曲线
void DemoCurve(CairoCanvas& canvas, double x0, double y0) {
    DrawTitle(canvas, x0, y0, "[5] 贝塞尔曲线");

    double y = y0 + 30;

    // 1) S 形曲线
    canvas.SetSourceColor(Color::Black());
    canvas.SetLineWidth(2.5);
    canvas.DrawCurveFrom(x0, y + 60,
                         x0 + 80, y,
                         x0 + 140, y + 120,
                         x0 + 220, y + 60);

    // 控制点 (用细虚线提示)
    canvas.SetSourceColor(Color::FromRGB(200, 60, 60));
    canvas.SetLineWidth(1.0);
    canvas.SetDash({2.0, 2.0});
    canvas.DrawLine(x0, y + 60, x0 + 80, y);
    canvas.DrawLine(x0 + 140, y + 120, x0 + 220, y + 60);
    canvas.ClearDash();
    // 控制点本身
    canvas.SetSourceColor(Color::Red());
    canvas.DrawCircle(x0 + 80, y, 3.0, true);
    canvas.DrawCircle(x0 + 140, y + 120, 3.0, true);

    DrawCaption(canvas, x0, y + 90, "S 形三阶贝塞尔曲线 (红点为控制点)");

    y += 110;

    // 2) 心形线 (两段对称贝塞尔)
    canvas.SetSourceColor(Color::Magenta());
    canvas.SetLineWidth(3.0);
    double cx = x0 + 110;
    double cy = y + 60;
    canvas.DrawCurveFrom(cx, cy + 30,
                         cx - 50, cy - 30,
                         cx - 110, cy + 10,
                         cx, cy + 10);
    canvas.DrawCurve(cx - 0,    cy + 10,
                     cx + 110,  cy + 10,
                     cx,        cy + 30);

    DrawCaption(canvas, x0, y + 100, "两段贝塞尔拼接的简化心形");
}

/// @brief 演示 6: 描迹线 / 描迹闭合多边形
void DemoPath(CairoCanvas& canvas, double x0, double y0) {
    DrawTitle(canvas, x0, y0, "[6] 描迹线 / 描迹闭合");

    double y = y0 + 40;
    // 折线 (不闭合)
    std::vector<Point> polyline = {
        {x0,       y + 0},
        {x0 + 40,  y + 40},
        {x0 + 80,  y + 10},
        {x0 + 120, y + 50},
        {x0 + 160, y + 20},
        {x0 + 200, y + 60}
    };
    canvas.SetSourceColor(Color::FromRGB(40, 100, 200));
    canvas.SetLineWidth(2.0);
    canvas.SetLineJoin(LineJoin::Round);
    canvas.DrawPolyline(polyline);
    DrawCaption(canvas, x0, y + 80, "描迹线 (折线, 线连接 Round)");

    y += 100;
    // 闭合多边形 (描边)
    std::vector<Point> polygon1 = {
        {x0 + 20,  y},
        {x0 + 80,  y},
        {x0 + 100, y + 40},
        {x0 + 50,  y + 80},
        {x0,       y + 40}
    };
    canvas.SetSourceColor(Color::Black());
    canvas.SetLineWidth(2.0);
    canvas.SetLineJoin(LineJoin::Miter);
    canvas.DrawPolygon(polygon1, false);

    // 闭合多边形 (填充 + 描边)
    std::vector<Point> polygon2 = {
        {x0 + 140, y},
        {x0 + 220, y},
        {x0 + 240, y + 40},
        {x0 + 180, y + 80},
        {x0 + 120, y + 40}
    };
    canvas.SetSourceColor(Color::FromRGB(255, 180, 60));
    canvas.DrawPolygon(polygon2, true);
    canvas.SetSourceColor(Color::Black());
    canvas.SetLineWidth(1.5);
    canvas.DrawPolygon(polygon2, false);

    y += 100;
    DrawCaption(canvas, x0, y, "闭合多边形: 五边形描边 / 五角星形填充+描边");
}

/// @brief 演示 7: 文字 (字体 / 字号 / 粗体 / 斜体 / 中文)
void DemoText(CairoCanvas& canvas, double x0, double y0) {
    DrawTitle(canvas, x0, y0, "[7] 文字 / 字体 / 字号");

    double y = y0 + 40;
    canvas.SetSourceColor(Color::Black());
    canvas.SetFont(FontStyle("Microsoft YaHei", 14, false, false));
    canvas.DrawTextUTF8(x0, y, "默认 14px 微软雅黑: Hello Cairo, 你好,世界!");
    y += 26;

    canvas.SetFont(FontStyle("Microsoft YaHei", 18, true, false));
    canvas.SetSourceColor(Color::FromRGB(30, 100, 200));
    canvas.DrawTextUTF8(x0, y, "粗体 18px 蓝色: 矢量绘图示例");
    y += 32;

    canvas.SetFont(FontStyle("Microsoft YaHei", 16, false, true));
    canvas.SetSourceColor(Color::FromRGB(200, 60, 60));
    canvas.DrawTextUTF8(x0, y, "斜体 16px 红色: CairoCanvas 演示");
    y += 30;

    canvas.SetFont(FontStyle("Times New Roman", 22, true, true));
    canvas.SetSourceColor(Color::FromRGB(80, 80, 80));
    canvas.DrawTextUTF8(x0, y, "Bold Italic 22px Times New Roman");
    y += 30;

    canvas.SetFont(FontStyle("Microsoft YaHei", 12, false, false));
    canvas.SetSourceColor(Color::FromRGB(120, 120, 120));
    canvas.DrawTextUTF8(x0, y, "字号测算: 'Hello World' 渲染宽度 = " +
                                std::to_string((int)canvas.GetTextWidth("Hello World")) + " px");
}

/// @brief 演示 8: 综合图形 (笑脸)
void DemoSmiley(CairoCanvas& canvas, double cx, double cy, double r) {
    // 脸 (填充黄色)
    canvas.SetSourceColor(Color::FromRGB(255, 220, 60));
    canvas.DrawCircle(cx, cy, r, true);
    canvas.SetSourceColor(Color::Black());
    canvas.SetLineWidth(2.0);
    canvas.DrawCircle(cx, cy, r, false);

    // 眼睛
    canvas.SetSourceColor(Color::Black());
    canvas.DrawCircle(cx - r * 0.35, cy - r * 0.25, r * 0.08, true);
    canvas.DrawCircle(cx + r * 0.35, cy - r * 0.25, r * 0.08, true);

    // 嘴 (弧线)
    canvas.SetLineWidth(3.0);
    canvas.SetLineCap(LineCap::Round);
    canvas.DrawArc(cx, cy,
                   r * 0.6,
                   M_PI * 0.15,
                   M_PI * 0.85);
}

/// @brief 演示 8: 颜色表 (RGBA 全色谱)
void DemoColorSwatch(CairoCanvas& canvas, double x0, double y0) {
    DrawTitle(canvas, x0, y0, "[8] 颜色 / 半透明");

    double x = x0;
    double y = y0 + 40;
    double w = 40, h = 40;
    canvas.SetLineWidth(1.0);
    canvas.SetSourceColor(Color::Black());

    auto swatch = [&](double X, const Color& c, const char* name) {
        canvas.SetSourceColor(c);
        canvas.DrawRectangle(X, y, w, h, true);
        canvas.SetSourceColor(Color::Black());
        canvas.DrawRectangle(X, y, w, h, false);
        canvas.SetFont(FontStyle("Microsoft YaHei", 10, false, false));
        canvas.DrawTextUTF8(X, y + h + 14, name);
    };

    swatch(x + 0,   Color::Red(),                            "Red");
    swatch(x + 50,  Color::Green(),                          "Green");
    swatch(x + 100, Color::Blue(),                           "Blue");
    swatch(x + 150, Color::Yellow(),                         "Yellow");
    swatch(x + 200, Color::Cyan(),                           "Cyan");
    swatch(x + 250, Color::Magenta(),                        "Magenta");

    // 半透明色叠加
    y += 90;
    canvas.SetSourceColor(Color::FromRGB(255, 100, 100));
    canvas.DrawRectangle(x0 + 0,   y, 100, 50, true);
    canvas.SetSourceColor(Color::FromRGB(100, 100, 255, 128));
    canvas.DrawRectangle(x0 + 60,  y, 100, 50, true);
    canvas.SetSourceColor(Color::FromRGB(100, 255, 100, 128));
    canvas.DrawRectangle(x0 + 30,  y + 30, 100, 50, true);
}

// =============================================================================
// 实时循环演示 (命令行参数: TestCAiro2.exe -live)
// =============================================================================
//
// 演示 CairoCanvas 的"零拷贝"用法:
//   - 调用方在堆上分配 RGBA buffer;
//   - Cairo 直接在 buffer 上绘制, 不复制像素;
//   - 每一帧结束后可以立刻 GetData() 拿这块指针, 交给 GDI / Direct2D /
//     SDL / OpenGL 上传到显示端;
//
// 本 demo 不连真实窗口, 但:
//   1) 用 std::chrono 统计 1) 单帧耗时 2) 平均帧率;
//   2) 验证 buffer 在每次画完后可以"读"到正确内容 (抽样几个像素检查);
//   3) 跑 2 秒后停止, 保留最后一帧为 PNG 文件, 方便肉眼验证图形正常。
//
int RunLiveDemo(double runSeconds, int width, int height) {
    using clk = std::chrono::high_resolution_clock;
    using ms   = std::chrono::milliseconds;
    using ns   = std::chrono::nanoseconds;

    // ------------------------------------------------------------------
    // 1) 调用方分配 RGBA buffer (堆上, 实际项目中也常用 mmap / 显存)
    // ------------------------------------------------------------------
    const int stride = width * 4;        // ARGB32 每像素 4 字节
    std::vector<unsigned char> buffer(static_cast<size_t>(stride) * height);
    std::printf("[live] buffer=%dx%d stride=%d bytes=%.1f KB\n",
                width, height, stride, buffer.size() / 1024.0);

    // ------------------------------------------------------------------
    // 2) 零拷贝构造, Cairo 直接在 buffer 上画
    // ------------------------------------------------------------------
    CairoCanvas canvas(width, height, buffer.data(), stride);

    // ------------------------------------------------------------------
    // 3) 实时循环
    // ------------------------------------------------------------------
    const auto t0 = clk::now();
    const auto deadline = t0 + std::chrono::duration_cast<clk::duration>(
        std::chrono::duration<double>(runSeconds));

    long long totalFrames   = 0;
    long long totalDrawNanos = 0;
    long long maxFrameNanos = 0;
    long long minFrameNanos = (std::numeric_limits<long long>::max)();
    long long lastFpsFrames  = 0;
    auto       lastFpsTime   = t0;
    double     t             = 0.0;

    std::printf("[live] 进入实时循环, 目标运行 %.1f 秒 ...\n", runSeconds);

    while (clk::now() < deadline) {
        const auto frameStart = clk::now();

        // 3.1 清屏 (深色背景)
        canvas.Clear(Color::FromRGB(20, 24, 36));

        // 3.2 移动的小球
        {
            const double cx = width  * 0.5 + std::cos(t)        * 220.0;
            const double cy = height * 0.5 + std::sin(t * 1.3)  * 160.0;
            canvas.SetSourceColor(Color::FromRGB(100, 200, 255));
            canvas.DrawCircle(cx, cy, 30, true);
            canvas.SetSourceColor(Color::White());
            canvas.SetLineWidth(1.5);
            canvas.DrawCircle(cx, cy, 30, false);
        }

        // 3.3 旋转的方块阵列 (5 个)
        for (int i = 0; i < 5; ++i) {
            const double a = t + i * (2.0 * M_PI / 5.0);
            const double x = width  * 0.5 + std::cos(a) * 180.0;
            const double y = height * 0.5 + std::sin(a) * 120.0;
            canvas.Save();
            canvas.Translate(x, y);
            canvas.Rotate(a);
            canvas.SetSourceColor(Color::FromRGB(255, 150, 50, 220));
            canvas.DrawRectangle(-18, -18, 36, 36, true);
            canvas.SetSourceColor(Color::Black());
            canvas.SetLineWidth(1.0);
            canvas.DrawRectangle(-18, -18, 36, 36, false);
            canvas.Restore();
        }

        // 3.4 螺旋线 (1 帧画一条, 共 50 段, 模拟描迹过程)
        canvas.SetLineWidth(2.0);
        canvas.SetSourceColor(Color::FromRGB(120, 255, 180));
        const int spiralSegments = 50;
        for (int i = 0; i < spiralSegments; ++i) {
            const double a1 = (i     ) * 0.3 + t * 0.5;
            const double a2 = (i + 1 ) * 0.3 + t * 0.5;
            const double r1 = 50.0 + i * 1.5;
            const double r2 = 50.0 + (i + 1) * 1.5;
            const double x1 = width  * 0.5 + std::cos(a1) * r1;
            const double y1 = height * 0.5 + std::sin(a1) * r1;
            const double x2 = width  * 0.5 + std::cos(a2) * r2;
            const double y2 = height * 0.5 + std::sin(a2) * r2;
            canvas.DrawLine(x1, y1, x2, y2);
        }

        // 3.5 帧率 / 帧时间文本 (用 std::min 修正首帧的 minFrameNanos)
        canvas.SetFont(FontStyle("Arial", 22, true, false));
        canvas.SetSourceColor(Color::FromRGB(255, 220, 100));
        canvas.DrawTextUTF8(20, 40, "Zero-Copy Live Demo");
        canvas.SetFont(FontStyle("Arial", 14, false, false));
        canvas.SetSourceColor(Color::FromRGB(200, 200, 200));
        canvas.DrawTextUTF8(20, 64,
            "Buffer in heap, Cairo draws directly, no copy, no PNG.");

        // 3.6 关键: 把 Cairo 内部缓存写回用户 buffer
        canvas.Flush();

        // 3.7 抽样验证: 读 buffer 中几个像素, 确认 Cairo 真的写入了
        // (用第一帧 4 个角点 + 小球中心点, 角点期望是深色背景, 球心期望是蓝色)
        if (totalFrames == 0) {
            const unsigned char* p = canvas.GetData();
            // t=0 时小球中心 = (cx, cy) = (400+220, 300) = (620, 300)
            const int ballX = static_cast<int>(width  * 0.5 + 220.0);
            const int ballY = static_cast<int>(height * 0.5);
            std::printf("[live] 抽样像素 (期望: 球心 B=100 G=200 R=255, 4 角是深色):\n");
            auto sample = [&](const char* name, int x, int y) {
                const unsigned char* px = p + y * canvas.GetStride() + x * 4;
                std::printf("  %-10s 0x%02X%02X%02X%02X  B=%3d G=%3d R=%3d A=%3d\n",
                            name, px[2], px[1], px[0], px[3],
                            px[0], px[1], px[2], px[3]);
            };
            sample("ball-center", ballX, ballY);
            sample("topleft",       1,   1);
            sample("topright",width-2,   1);
            sample("botleft",       1, height-2);
        }

        // 3.8 统计
        const auto frameEnd = clk::now();
        const long long frameNs =
            std::chrono::duration_cast<ns>(frameEnd - frameStart).count();
        totalDrawNanos += frameNs;
        if (frameNs > maxFrameNanos) maxFrameNanos = frameNs;
        if (frameNs < minFrameNanos) minFrameNanos = frameNs;
        ++totalFrames;
        ++lastFpsFrames;
        t += 0.05;

        // 每 0.5 秒打印一次瞬时 FPS
        const auto now = clk::now();
        const double fpsInterval =
            std::chrono::duration<double>(now - lastFpsTime).count();
        if (fpsInterval >= 0.5) {
            const double instFps = lastFpsFrames / fpsInterval;
            std::printf("[live] %.2fs 瞬时 FPS = %6.1f  (本帧 %5.2f ms)\n",
                        std::chrono::duration<double>(now - t0).count(),
                        instFps, frameNs / 1e6);
            lastFpsFrames = 0;
            lastFpsTime   = now;
        }
    }

    // ------------------------------------------------------------------
    // 4) 统计 + 保存最后一帧
    // ------------------------------------------------------------------
    const double totalSec = totalDrawNanos / 1e9;
    const double avgFps   = totalFrames / totalSec;
    const double avgMs    = totalDrawNanos / 1e6 / totalFrames;
    std::printf("\n[live] === 总结 ===\n");
    std::printf("[live] 总帧数       : %lld\n",  totalFrames);
    std::printf("[live] 总耗时       : %.2f s\n", totalSec);
    std::printf("[live] 平均帧率     : %.1f FPS\n", avgFps);
    std::printf("[live] 平均帧耗时   : %.2f ms\n", avgMs);
    std::printf("[live] 最小帧耗时   : %.2f ms\n", minFrameNanos / 1e6);
    std::printf("[live] 最大帧耗时   : %.2f ms\n", maxFrameNanos / 1e6);

    // 提示: 集成到 GUI 时, 这里把 canvas.GetData() 传给 GDI/D2D/SDL 即可。
    // 例如 Win32 GDI:
    //   BITMAPINFO bmi{}; bmi.bmiHeader.biSize = sizeof(bmi);
    //   bmi.bmiHeader.biWidth = canvas.GetWidth();
    //   bmi.bmiHeader.biHeight = -canvas.GetHeight();   // 顶向下 DIB
    //   bmi.bmiHeader.biPlanes = 1; bmi.bmiHeader.biBitCount = 32;
    //   bmi.bmiHeader.biCompression = BI_RGB;
    //   StretchDIBits(hdc, 0, 0, w, h, 0, 0, w, h,
    //                 canvas.GetData(), &bmi, DIB_RGB_COLORS, SRCCOPY);

    // 保留最后一帧为 PNG (方便肉眼验证)
    if (!canvas.SaveAsPng("live_last_frame.png")) {
        std::fprintf(stderr, "[live] 保存最后一帧失败\n");
        return 1;
    }
    std::printf("[live] 最后一帧已保存到 live_last_frame.png\n");
    return 0;
}



// =============================================================================
// -leaktrace 调试: 分配 hook, 把每次 alloc 的 PC 通过 dbghelp.SymFromAddr 解析
// 默认 #if 0 关闭; 死磕 0 leak 时改成 #if 1 + 上方 windows.h/dbghelp.h 也打开.
// =============================================================================
#if 0
static int LeakAllocHook(int allocType, void* /*userData*/, size_t size,
                        int /*blockType*/, long reqNum,
                        const unsigned char* /*file*/, int /*line*/) {
    if (allocType != _HOOK_ALLOC) return 1;
    if (size != 264 && size != 432 && size != 2064 && size != 96) return 1;

    void* callers[6] = { nullptr };
    USHORT got = ::RtlCaptureStackBackTrace(2, 6, callers, nullptr);

    std::FILE* out = nullptr;
    fopen_s(&out, "D:\\Develop\\MemoryGraphic\\leak_trace.txt", "a");
    if (!out) return 1;
    std::fprintf(out, "alloc #%ld size=%zu |", reqNum, size);
    HANDLE hp = ::GetCurrentProcess();
    for (int i = 0; i < got; ++i) {
        SYMBOL_INFO* sym = (SYMBOL_INFO*)std::calloc(sizeof(SYMBOL_INFO) + 256, 1);
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen   = 255;
        DWORD64 disp = 0;
        if (::SymFromAddr(hp, (DWORD64)callers[i], &disp, sym)) {
            std::fprintf(out, " %p:%s+0x%llx", callers[i], sym->Name, disp);
        } else {
            std::fprintf(out, " %p:?", callers[i]);
        }
        std::free(sym);
    }
    std::fprintf(out, "\n");
    std::fclose(out);
    return 1;
}

#endif // #if 0 (leaktrace debug)

} // namespace (anonymous, 包围 line 47-583 的辅助函数 + 可选 LeakAllocHook)

int main(int argc, char* argv[]) {
    // 打开 CRT 内存泄漏 / 越界检测 (Debug 版本)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF
                   | _CRTDBG_CHECK_ALWAYS_DF);

#if 0
    // -leaktrace 模式, 把每次 alloc 的 PC 用 dbghelp 解析, 写文件
    if (argc >= 2 && std::string(argv[1]) == "-leaktrace") {
        std::FILE* f = nullptr;
        fopen_s(&f, "D:\\Develop\\MemoryGraphic\\leak_trace.txt", "w");
        if (f) { std::fprintf(f, "# alloc trace (size, reqNum, caller-symbol+offset)\n"); std::fclose(f); }

        // 用 dbghelp 解析 caller
        ::SymInitialize(::GetCurrentProcess(), "D:\\Develop\\MemoryGraphic\\cairo-1.18.4\\lib\\x64-v143", TRUE);
        ::SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);

        _CrtSetAllocHook(LeakAllocHook);
    }
#endif

    // ---------------------------------------------------------------------
    // 命令行分发:
    //   TestCAiro2.exe           跑静态演示, 输出 PNG / SVG / PDF / PS
    //   TestCAiro2.exe -live     跑实时循环演示, 测帧率
    //   TestCAiro2.exe -live 5   指定运行秒数 (默认 2.0)
    //   TestCAiro2.exe -dbuf     验证双缓冲 (KG_Create 模式, 不走 KindyunGraphic)
    //   TestCAiro2.exe -ellip    单测 DrawEllipse fill=true vs fill=false
    // ---------------------------------------------------------------------
    if (argc >= 2 && std::string(argv[1]) == "-ellip") {
        // 单测 DrawEllipse: 4 枚椭圆, fill=true,true,true,false
        // 期望: 前 3 个填充, 第 4 个仅描边
        // 必须用单缓冲模式 (false), 双缓冲模式下 SaveAsPng 直接 dump read surface,
        //     read surface 里只有 swap 之前的内容, 无法用于导出。
        CairoCanvas c(600, 200, /*doubleBuffered=*/false);
        c.Clear(Color::White());
        c.SetLineWidth(3.0);

        c.SetSourceColor(Color::Red());
        c.DrawEllipse(100, 100, 60, 30, 0.0, true);

        c.SetSourceColor(Color(0.0, 0.6, 0.0, 1.0));
        c.DrawEllipse(220, 100, 60, 30, 0.0, true);

        c.SetSourceColor(Color::Blue());
        c.DrawEllipse(340, 100, 60, 30, 0.0, true);

        c.SetSourceColor(Color::Black());
        c.DrawEllipse(460, 100, 60, 30, 0.0, false);  // 只描边

        c.SaveAsPng("ellipse_test.png");
        std::printf("[ellip] 写出 ellipse_test.png (期望: 红/绿/蓝填充椭圆 + 第4个仅描边)\n");
        return 0;
    }
    if (argc >= 2 && std::string(argv[1]) == "-dbuf") {
        std::printf("[dbuf] 启动\n");
        std::fflush(stdout);
        try {
            using namespace memgc;
            CairoCanvas canvas(100, 100, true);  // 开双缓冲
            std::printf("[dbuf] 构造完成\n");
            std::fflush(stdout);

            // 第 1 次画 + Flush
            canvas.Clear(Color(0.0, 0.0, 1.0, 1.0));  // 蓝
            canvas.DrawRectangle(10, 10, 80, 80, true);
            canvas.Flush();
            std::printf("[dbuf] 第 1 次 Flush 完成\n");
            std::fflush(stdout);

            // 跳过 GetData, 直接用 DebugGetReadSurfaceData
            unsigned char* probe = canvas.DebugGetReadSurfaceData();
            std::fprintf(stderr, "[dbuf] DebugGetReadSurfaceData = %p\n", (void*)probe);
            std::fflush(stderr);
            unsigned char* probe2 = canvas.DebugGetDrawSurfaceData();
            std::fprintf(stderr, "[dbuf] DebugGetDrawSurfaceData = %p\n", (void*)probe2);
            std::fflush(stderr);

            unsigned char* p1 = canvas.GetData();
            std::fprintf(stderr, "[dbuf] GetData = %p\n", (void*)p1);
            std::fflush(stderr);
            std::fflush(stdout);
            if (p1) {
                std::printf("[1] 像素 = 0x%02X%02X%02X%02X (期望蓝色)\n",
                            p1[0], p1[1], p1[2], p1[3]);
                std::fflush(stdout);
            }

            // 第 2 次画红色
            canvas.Clear(Color(1.0, 0.0, 0.0, 1.0));
            canvas.Flush();
            unsigned char* p2 = canvas.GetData();
            std::printf("[2] GetData = %p (p1==p2? %d)\n", (void*)p2, p1 == p2);
            std::fflush(stdout);
            if (p2) {
                std::printf("[2] 像素 = 0x%02X%02X%02X%02X (期望红色)\n",
                            p2[0], p2[1], p2[2], p2[3]);
                std::fflush(stdout);
            }

            // 第 3 次画绿色
            canvas.Clear(Color(0.0, 1.0, 0.0, 1.0));
            canvas.Flush();
            unsigned char* p3 = canvas.GetData();
            std::printf("[3] GetData = %p (p2==p3? %d)\n", (void*)p3, p2 == p3);
            std::fflush(stdout);
            if (p3) {
                std::printf("[3] 像素 = 0x%02X%02X%02X%02X (期望绿色)\n",
                            p3[0], p3[1], p3[2], p3[3]);
                std::fflush(stdout);
            }
        } catch (const std::exception& e) {
            std::printf("[dbuf] 异常: %s\n", e.what());
            std::fflush(stdout);
        }
        return 0;
    }
    if (argc >= 2 && std::string(argv[1]) == "-live") {
        // 用法: -live [seconds] [width] [height]
        const double seconds = (argc >= 3) ? std::atof(argv[2]) : 2.0;
        const int    width   = (argc >= 4) ? std::atoi(argv[3]) : 800;
        const int    height  = (argc >= 5) ? std::atoi(argv[4]) : 600;
        return RunLiveDemo(seconds, width, height);
    }


    try {
        // 1) 创建 1100x900 的内存画布 (调整大小让所有演示都有足够空间)
        CairoCanvas canvas(1100, 900);

        // 2) 用白色清空 (默认就是白色, 但显式调用一下展示 Clear 用法)
        canvas.Clear(Color::White());

        // ---------------------------------------------------------------------
        // 顶部标题
        // ---------------------------------------------------------------------
        canvas.SetSourceColor(Color::FromRGB(30, 60, 130));
        canvas.SetFont(FontStyle("Microsoft YaHei", 28, true, false));
        canvas.DrawTextUTF8(30, 50, "CairoCanvas 封装类演示");

        canvas.SetSourceColor(Color::FromRGB(120, 120, 120));
        canvas.SetFont(FontStyle("Microsoft YaHei", 12, false, true));
        canvas.DrawTextUTF8(30, 75,
            "直线 / 曲线 / 圆 / 椭圆 / 描迹 / 文字 / 颜色 / 线宽 / 字体");

        // 标题下的横线
        canvas.SetSourceColor(Color::Gray(0.6));
        canvas.SetLineWidth(1.0);
        canvas.DrawLine(30, 90, 1070, 90);

        // ---------------------------------------------------------------------
        // 各种演示 (左侧列, x = 30)
        // ---------------------------------------------------------------------
        DemoLine(canvas,         30, 120);  // 1. 直线 / 线宽 / 线帽 / 虚线
        DemoRectangle(canvas,    30, 380);  // 2. 矩形
        DemoCircleEllipse(canvas, 30, 510);  // 3. 圆 / 椭圆 / 旋转

        // ---------------------------------------------------------------------
        // 各种演示 (右侧列, x = 530)
        // ---------------------------------------------------------------------
        DemoColorSwatch(canvas, 530, 120);  // 4. 颜色 / 半透明
        DemoArc(canvas,         530, 310);  // 5. 圆弧
        DemoCurve(canvas,       530, 480);  // 6. 贝塞尔曲线
        DemoPath(canvas,        530, 670);  // 7. 描迹线 / 描迹闭合

        // ---------------------------------------------------------------------
        // 底部: 文字 / 笑脸
        // ---------------------------------------------------------------------
        DemoText(canvas,    30, 760);       // 8. 文字
        DemoSmiley(canvas, 950, 870, 40);   // 笑脸 (右下角)

        // ---------------------------------------------------------------------
        // 保存结果
        // ---------------------------------------------------------------------
        const std::string pngPath = "demo_output.png";
        const std::string svgPath = "demo_output.svg";
        const std::string pdfPath = "demo_output.pdf";
        const std::string psPath  = "demo_output.ps";
        canvas.Flush();
        std::printf("正在导出 PNG ...");
        if (!canvas.SaveAsPng(pngPath)) {
            std::fprintf(stderr, "\n保存 PNG 失败: %s\n", pngPath.c_str());
            return 1;
        }
        std::printf(" OK -> %s\n", pngPath.c_str());

        std::printf("正在导出 SVG ...");
        if (!canvas.SaveAsSvg(svgPath)) {
            std::fprintf(stderr, "\n保存 SVG 失败: %s\n", svgPath.c_str());
            return 1;
        }
        std::printf(" OK -> %s\n", svgPath.c_str());

        std::printf("正在导出 PDF ...");
        if (!canvas.SaveAsPdf(pdfPath)) {
            std::fprintf(stderr, "\n保存 PDF 失败: %s\n", pdfPath.c_str());
            return 1;
        }
        std::printf(" OK -> %s\n", pdfPath.c_str());

        std::printf("正在导出 PS ...");
        if (!canvas.SaveAsPs(psPath)) {
            std::fprintf(stderr, "\n保存 PS 失败: %s\n", psPath.c_str());
            return 1;
        }
        std::printf(" OK -> %s\n", psPath.c_str());

        std::printf("\n全部完成!\n");
    } catch (const std::exception& e) {
        std::fprintf(stderr, "异常: %s\n", e.what());
        return 1;
    }

    // 关键: 主动清理 cairo 进程级 cache, 避免 cairo 静态 cache 大量 leak
    // (valgrind / crtdbg 默认会报 cairo 的 scaled_font / font_face / pattern 等
    //  静态 cache 为 leak, 但这些是 lazy 的, 实际进程退出前 cairo 不主动 free)
    // 文档: https://www.cairographics.org/manual/cairo-cairo-debug.html
    // 注意: 必须在所有 active cairo 对象都销毁之后才调用, 否则 cairo 内部
    //       hash 表正在用时清空会导致 crash.
    // 我们的 CairoCanvas 在 try 块结束时就析构, 此时调这个函数是安全的.
    cairo_debug_reset_static_data();

    // 进一步: 释放 pixman 全局 implementation cache (cairo_debug_reset_static_data 不管 pixman)
    // 5 个 pixman_implementation_t (sse2/ssse3/noop/fast_path/general) 总共 ~10KB.
    // 默认 pixman 用 __attribute__((destructor)) 自动释放, 但 MSVC 不支持这个属性.
    pixman_fini();

#ifdef _DEBUG
    // Debug 版: 接 stderr 的 leak dump, 让 CLI 死磕 0 leak.
    // 无 leak 时打印 "[OK] 0 leak"; 有 leak 时由 crtdbg 详细列 caller.
    _CrtSetReportMode(_CRT_WARN,   _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN,   _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR,  _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR,  _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    int leakedBytes = _CrtDumpMemoryLeaks();
    std::fprintf(stderr, leakedBytes > 0
        ? "\n[LEAK] crtdbg reported leaked bytes = %d\n"
        : "\n[OK] 0 leak\n", leakedBytes);
    std::fflush(stderr);
#endif

    return 0;
}