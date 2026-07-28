// =============================================================================
// CairoCanvas.h
//
// 基于 Cairo 矢量绘图库的内存画布封装类。
//
// 主要功能:
//   - 在内存中创建一块 RGBA 图像作为画布;
//   - 支持直线 / 曲线 / 角度弧 / 圆 / 椭圆 / 描迹折线 / 描迹闭合多边形
//     / 矩形 / 文字 等基本绘制;
//   - 支持设置颜色 (RGBA) / 线宽 / 线帽 / 线连接 / 虚线模式 / 字体 / 字号;
//   - 支持画布的清空 / 平移 / 缩放 / 旋转 / 状态保存与恢复;
//   - 支持将画布导出为 PNG / SVG / PDF / PostScript 等格式。
//
// 实现思路:
//   内部使用 cairo 的 recording surface 记录所有绘制操作, 这样:
//     1) 导出 PNG 时, 把 recording surface 内容 replay 到一张 ARGB32 内存
//        surface, 再调用 cairo_surface_write_to_png 写出;
//     2) 导出 SVG/PDF/PS 时, 把 recording surface 内容 replay 到对应后端
//        surface, 由 cairo 生成真正的矢量描述 (不是位图嵌入)。
//
// 坐标系统:
//   Cairo 使用的是右手坐标系, Y 轴向下。画布左上角为原点 (0, 0)。
//   所有长度单位都是"用户空间单位", 默认情况下 1 单位 = 1 像素。
//
// 编译依赖:
//   cairo-1.18.4 (include + lib/cairo2d.lib 调试 / lib/cairo2.lib 发布)
//   pixman / freetype / libpng / zlib (已在 cairo 内部静态链接, 运行时
//   只需 cairo2.dll 或 cairo2d.dll 可被找到)
//
// =============================================================================

#ifndef MEMGC_CAIRO_CANVAS_H
#define MEMGC_CAIRO_CANVAS_H

#include <cstdint>
#include <string>
#include <vector>

// Cairo 是 C 库, 需要 extern "C" 包裹避免 C++ 名称修饰 (mangling)。
extern "C" {
#include <cairo.h>
}

namespace memgc {

// =============================================================================
// 基础数据结构 / 枚举
// =============================================================================

/// @brief RGBA 颜色, 各分量取值范围为 [0.0, 1.0]。
struct Color {
    double r;   ///< 红色分量
    double g;   ///< 绿色分量
    double b;   ///< 蓝色分量
    double a;   ///< 透明度, 1.0 = 完全不透明, 0.0 = 完全透明

    /// @brief 构造一个颜色, alpha 默认 1.0 (不透明)。
    Color(double red = 0.0,
          double green = 0.0,
          double blue = 0.0,
          double alpha = 1.0)
        : r(red), g(green), b(blue), a(alpha) {}

    // -------------------------------------------------------------------------
    // 常用预设颜色
    // -------------------------------------------------------------------------
    static Color Black()       { return Color(0.0, 0.0, 0.0); }
    static Color White()       { return Color(1.0, 1.0, 1.0); }
    static Color Red()         { return Color(1.0, 0.0, 0.0); }
    static Color Green()       { return Color(0.0, 1.0, 0.0); }
    static Color Blue()        { return Color(0.0, 0.0, 1.0); }
    static Color Yellow()      { return Color(1.0, 1.0, 0.0); }
    static Color Cyan()        { return Color(0.0, 1.0, 1.0); }
    static Color Magenta()     { return Color(1.0, 0.0, 1.0); }
    static Color Gray(double v){ return Color(v, v, v); }
    static Color Transparent() { return Color(0.0, 0.0, 0.0, 0.0); }

    /// @brief 通过 0-255 的字节值构造颜色, alpha 默认 255 (不透明)。
    static Color FromRGB(uint8_t red,
                         uint8_t green,
                         uint8_t blue,
                         uint8_t alpha = 255) {
        return Color(red / 255.0, green / 255.0, blue / 255.0,
                     alpha / 255.0);
    }
};

/// @brief 字体描述。
struct FontStyle {
    std::string family;   ///< 字体族名称, 例如 "Arial", "Microsoft YaHei", "sans-serif"
    double      size;     ///< 字号 (像素), cairo 中用 cairo_set_font_size 设置
    bool        bold;     ///< 是否粗体
    bool        italic;   ///< 是否斜体

    /// @brief 构造字体描述, 默认 sans-serif / 12 像素 / 不粗不斜。
    FontStyle(const std::string& fam = "sans-serif",
              double sz = 12.0,
              bool b = false,
              bool i = false)
        : family(fam), size(sz), bold(b), italic(i) {}
};

/// @brief 线帽样式, 对应 cairo_line_cap_t。
enum class LineCap {
    Butt   = CAIRO_LINE_CAP_BUTT,    ///< 无线帽, 端点处截平
    Round  = CAIRO_LINE_CAP_ROUND,   ///< 圆头
    Square = CAIRO_LINE_CAP_SQUARE   ///< 方头, 端点处延伸半个线宽
};

/// @brief 线段连接处样式, 对应 cairo_line_join_t。
enum class LineJoin {
    Miter = CAIRO_LINE_JOIN_MITER,   ///< 尖角 (默认)
    Round = CAIRO_LINE_JOIN_ROUND,   ///< 圆角
    Bevel = CAIRO_LINE_JOIN_BEVEL    ///< 斜切
};

/// @brief 填充规则, 对应 cairo_fill_rule_t。
enum class FillRule {
    NonZero = CAIRO_FILL_RULE_WINDING,  ///< 非零环绕 (默认)
    EvenOdd = CAIRO_FILL_RULE_EVEN_ODD ///< 奇偶环绕
};

/// @brief 可导出的文件格式。
enum class SaveFormat {
    Png,    ///< 位图: PNG
    Svg,    ///< 矢量: SVG
    Pdf,    ///< 矢量: PDF
    Ps      ///< 矢量: PostScript
};

/// @brief Cairo 支持的像素格式。
///        重要: CAIRO_FORMAT_ARGB32 在 Windows x86/x64 上是 **BGRA 字节序** (B 在低地址),
///        与 Win32 BITMAPINFOHEADER / Direct2D / WPF 等完全一致, 零拷贝对接无需 swap。
enum class PixelFormat {
    ARGB32    = 0,    ///< 32 位, 带 alpha;  Windows 上是 BGRA 字节序
    RGB24     = 1,    ///< 32 位, alpha 忽略; Windows 上是 BGRX
    A8        = 2,    ///< 8 位 alpha 单通道
    RGB16_565 = 3     ///< 16 位 RGB565
};

/// @brief 二维坐标点。
struct Point {
    double x;   ///< X 坐标 (向右)
    double y;   ///< Y 坐标 (向下)

    /// @brief 构造一个点, 默认 (0, 0)。
    Point(double x_ = 0.0, double y_ = 0.0) : x(x_), y(y_) {}
};

// =============================================================================
// CairoCanvas - 内存画布封装类
// =============================================================================

/// @brief 基于 cairo 的内存画布, 支持绘制基本图形和文字, 并能导出为
///        PNG / SVG / PDF / PostScript 等格式。
///
/// 典型用法:
/// @code
///     CairoCanvas canvas(800, 600);              // 创建 800x600 画布
///     canvas.SetSourceColor(Color::Red());       // 设置红色
///     canvas.SetLineWidth(2.0);                  // 线宽 2 像素
///     canvas.DrawLine(0, 0, 100, 100);           // 画线
///     canvas.SetSourceColor(Color::Black());     // 改黑色
///     canvas.SetFont(FontStyle("Microsoft YaHei", 16, true));
///     canvas.DrawText(20, 50, "你好, cairo!");   // 写文字
///     canvas.SaveAsPng("output.png");            // 保存为 PNG
///     canvas.SaveAsSvg("output.svg");            // 保存为 SVG (矢量)
/// @endcode
class CairoCanvas {
public:
    /// @brief 构造一块指定尺寸的内存画布 (内部分配 RGBA buffer, ARGB32 格式)。
    /// @param width  画布宽度 (像素), 必须 > 0
    /// @param height 画布高度 (像素), 必须 > 0
    /// @param doubleBuffered 是否启用内置双缓冲 (默认 true)。
    ///        启用后内部会分配 2 个 buffer, 每帧 Flush() 时自动 swap:
    ///        一个 buffer 暴露给外部读, 另一个 buffer 给绘制, 绘制完成后
    ///        原子地交换, 外部永远读到完整的一帧, 解决实时画面闪烁。
    /// @throw std::runtime_error 当 cairo 资源创建失败时抛出
    CairoCanvas(int width, int height, bool doubleBuffered = true);

    /// @brief 零拷贝构造: 绑定到调用方提供的 RGBA 内存, Cairo 直接在上面绘制。
    ///        适合实时渲染 / 视频帧 / 游戏画面等需要高帧率的场景。
    ///
    /// 关键点:
    ///   - 内部**不**复制 externalData, 析构时**不**释放它;
    ///     内存由调用方全权管理 (stack / heap / mmap / 显存映射等均可);
    ///   - 任何绘制立刻反映到 externalData, 调用方任何时刻都能读这块内存;
    ///   - 建议每帧绘制结束后调用 Flush() 强制把 Cairo 内部缓存写回 buffer;
    ///   - 局部更新时配合 MarkDirtyRectangle() 标脏区, 可让上层只上传脏区
    ///     (例如 Direct2D 的 ID2D1Bitmap::CopyFromMemory 只取脏区);
    ///   - 由于没有 recording surface, 矢量导出 (SaveAsSvg/Pdf/Ps) **不可用**;
    ///     SaveAsPng 仍可用, 但导出的是已经光栅化后的位图。
    ///
    /// 字节序:
    ///   - CAIRO_FORMAT_ARGB32 在 Windows x86/x64 上是 **BGRA 字节序**;
    ///   - 与 Win32 DIB / Direct2D / WPF 字节序一致, 零拷贝对接;
    ///   - 如果调用方的 buffer 严格 R,G,B,A 顺序, 需自己 swap 或选不同格式。
    ///
    /// @param width, height  画布尺寸 (像素)
    /// @param externalData  调用方提供的内存, 大小 >= stride * height 字节
    /// @param stride        每一行的字节数, 至少为 4 * width (ARGB32)
    /// @param fmt           像素格式, 默认 ARGB32
    CairoCanvas(int width, int height,
                unsigned char* externalData, int stride,
                PixelFormat fmt = PixelFormat::ARGB32);

    /// @brief 析构, 自动释放 cairo 上下文与 (内部) surface。
    ///        在零拷贝模式下**不**释放调用方提供的 externalData。
    ~CairoCanvas();

    // CairoCanvas 内部管理 cairo 资源, 关闭拷贝构造与赋值避免双重释放。
    CairoCanvas(const CairoCanvas&) = delete;
    CairoCanvas& operator=(const CairoCanvas&) = delete;

    // =========================================================================
    // 基础状态设置
    // =========================================================================

    /// @brief 设置当前绘图颜色 (实色或带 alpha)。
    void SetSourceColor(const Color& color);

    /// @brief 设置当前绘图颜色 (0-255 字节值)。
    void SetSourceRGB(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);

    /// @brief 设置描边线宽 (用户空间单位, 默认与像素 1:1)。
    void SetLineWidth(double width);

    /// @brief 设置线帽样式。
    void SetLineCap(LineCap cap);

    /// @brief 设置线段连接处样式。
    void SetLineJoin(LineJoin join);

    /// @brief 设置虚线模式。
    /// @param dashes 长度数组, 表示交替的"实线长度 / 空白长度"序列;
    ///               数组长度建议为偶数, 奇数会自动循环
    /// @param offset 起始偏移量
    void SetDash(const std::vector<double>& dashes, double offset = 0.0);

    /// @brief 取消虚线模式, 恢复为实线。
    void ClearDash();

    /// @brief 设置填充规则。
    void SetFillRule(FillRule rule);

    /// @brief 设置 cairo 合成算子 (例如 CAIRO_OPERATOR_OVER / MULTIPLY 等)。
    void SetOperator(cairo_operator_t op) { cairo_set_operator(m_cr, op); }

    // =========================================================================
    // 路径操作 (底层 API, 用户一般不需要直接调用)
    // =========================================================================

    /// @brief 移动"画笔"到指定位置, 不绘制线条。
    void MoveTo(double x, double y);

    /// @brief 从当前点画一条线段到 (x, y), 仅加入路径, 需调用 Stroke/Fill 才真正绘制。
    void LineTo(double x, double y);

    /// @brief 添加一段三阶贝塞尔曲线到当前路径, 控制点 (x1, y1) (x2, y2), 终点 (x3, y3)。
    void CurveTo(double x1, double y1,
                 double x2, double y2,
                 double x3, double y3);

    /// @brief (cairo 1.18 暂未实现 cairo_arc_to, 此接口暂时不可用)
    ///        原计划通过圆弧连接当前点和 (x2, y2), 半径由 cairo 自动确定。
    //  void ArcTo(double x1, double y1,
    //             double x2, double y2,
    //             double radius);

    /// @brief 在当前路径上添加一段圆弧 (中心 + 半径 + 起止角)。
    /// @param cx, cy    圆心
    /// @param radius    半径
    /// @param startAngle, endAngle  起止角, 单位为弧度
    void Arc(double cx, double cy,
             double radius,
             double startAngle,
             double endAngle);

    /// @brief 闭合当前路径 (回到起点)。
    void ClosePath();

    /// @brief 描边当前路径 (不填充)。
    void Stroke();

    /// @brief 填充当前路径 (不描边)。
    void Fill();

    /// @brief 先描边当前路径, 再填充 (cairo_stroke_preserve + fill)。
    void StrokeAndFill();

    /// @brief 清除当前路径 (不改变已绘制像素)。
    void NewPath();

    // =========================================================================
    // 高级图形 (一调用即绘制, 内部自动管理路径)
    // =========================================================================

    /// @brief 绘制直线段, 从 (x1, y1) 到 (x2, y2)。
    void DrawLine(double x1, double y1, double x2, double y2);

    /// @brief 绘制一条多段折线 (描迹线), 不闭合。
    /// @param points 点序列, 至少 2 个点
    void DrawPolyline(const std::vector<Point>& points);

    /// @brief 绘制闭合多边形 (描迹闭合)。
    /// @param points 点序列, 至少 3 个点
    /// @param fill   true 表示填充, false 表示仅描边
    void DrawPolygon(const std::vector<Point>& points, bool fill = false);

    /// @brief 绘制矩形 (不旋转)。
    /// @param x, y    左上角
    /// @param w, h    宽高
    /// @param fill    true 表示填充, false 表示仅描边
    void DrawRectangle(double x, double y,
                       double w, double h,
                       bool fill = false);

    /// @brief 绘制圆。
    /// @param cx, cy  圆心
    /// @param r       半径
    /// @param fill    true 表示填充, false 表示仅描边
    void DrawCircle(double cx, double cy, double r, bool fill = false);

    /// @brief 绘制椭圆。
    /// @param cx, cy      椭圆中心
    /// @param rx, ry      半长轴 / 半短轴长度
    /// @param rotation    长轴逆时针旋转角 (弧度)
    /// @param fill        true 表示填充, false 表示仅描边
    void DrawEllipse(double cx, double cy,
                     double rx, double ry,
                     double rotation = 0.0,
                     bool fill = false);

    /// @brief 绘制一段圆弧 (不闭合到圆心)。
    /// @param cx, cy    圆心
    /// @param r         半径
    /// @param startAngle, endAngle  起止角 (弧度)
    void DrawArc(double cx, double cy,
                 double r,
                 double startAngle,
                 double endAngle);

    /// @brief 绘制贝塞尔曲线 (起点为最近一次 MoveTo/LineTo)。
    /// @param cp1x, cp1y  控制点 1
    /// @param cp2x, cp2y  控制点 2
    /// @param endX, endY  终点
    void DrawCurve(double cp1x, double cp1y,
                   double cp2x, double cp2y,
                   double endX, double endY);

    /// @brief 绘制贝塞尔曲线 (指定起点)。
    /// @param sx, sy  起点
    void DrawCurveFrom(double sx, double sy,
                       double cp1x, double cp1y,
                       double cp2x, double cp2y,
                       double endX, double endY);

    // =========================================================================
    // 文字
    // =========================================================================

    /// @brief 设置字体 (族名 + 字号 + 粗体 + 斜体)。
    void SetFont(const FontStyle& font);

    /// @brief 仅修改当前字号 (字体族保持不变)。
    void SetFontSize(double size);

    /// @brief 绘制文字, (x, y) 为文字基线起点。
    void DrawText(double x, double y, const std::string& text);

    /// @brief 绘制 UTF-8 编码文字 (例如中文), (x, y) 为文字基线起点。
    ///        cairo 内部使用 toy text API 时按 UTF-8 解码, 因此中文等需要
    ///        通过此函数传入 UTF-8 字符串。
    void DrawTextUTF8(double x, double y, const std::string& text);

    /// @brief 测算文字在当前字体设置下的渲染宽度 (像素)。
    double GetTextWidth(const std::string& text) const;

    /// @brief 测算文字的完整 extents (x_bearing / y_bearing / width / height 等)。
    void GetTextExtents(const std::string& text,
                        double* x_bearing,
                        double* y_bearing,
                        double* width,
                        double* height) const;

    // =========================================================================
    // 画布整体操作
    // =========================================================================

    /// @brief 用指定颜色清空整张画布。
    void Clear(const Color& color = Color::White());

    /// @brief 坐标平移。
    void Translate(double dx, double dy);

    /// @brief 坐标缩放。
    void Scale(double sx, double sy);

    /// @brief 坐标旋转 (弧度)。
    void Rotate(double radians);

    /// @brief 保存当前绘图状态 (颜色 / 线宽 / 变换矩阵 / 路径等)。
    void Save();

    /// @brief 恢复最近一次 Save() 之前的状态。
    void Restore();

    // =========================================================================
    // 导出 / 保存
    // =========================================================================

    /// @brief 保存当前画布为 PNG 文件。
    /// @param filepath 目标文件路径 (建议后缀 .png)
    /// @return true 成功, false 失败
    bool SaveAsPng(const std::string& filepath) const;

    /// @brief 保存当前画布为 SVG 文件 (矢量, 由 cairo 生成真正的 SVG 元素)。
    /// @param filepath 目标文件路径 (建议后缀 .svg)
    bool SaveAsSvg(const std::string& filepath) const;

    /// @brief 保存当前画布为 PDF 文件 (矢量)。
    bool SaveAsPdf(const std::string& filepath) const;

    /// @brief 保存当前画布为 PostScript 文件 (矢量)。
    bool SaveAsPs(const std::string& filepath) const;

    /// @brief 根据 format 选择对应后端导出。
    bool Save(const std::string& filepath, SaveFormat format) const;

    // =========================================================================
    // 元信息 / 高级访问
    // =========================================================================

    /// @brief 获取画布宽度 (像素)。
    int GetWidth() const noexcept { return m_width; }

    /// @brief 获取画布高度 (像素)。
    int GetHeight() const noexcept { return m_height; }

    /// @brief 获取底层 cairo 上下文, 供需要直接调用 cairo API 的高级用户使用。
    cairo_t* GetCairoContext() const noexcept { return m_cr; }

    /// @brief 获取底层 recording surface, 高级用户可自行 replay 到其他 surface。
    /// @note 仅在普通 (非零拷贝) 模式下有效, 零拷贝模式下返回 nullptr。
    cairo_surface_t* GetRecordingSurface() const noexcept { return m_recordingSurface; }

    /// @brief 获取底层 image surface (零拷贝模式下就是用户 buffer 绑定的那个)。
    cairo_surface_t* GetImageSurface() const noexcept { return m_imageSurface; }

    // =========================================================================
    // 零拷贝模式专用接口 (普通模式下返回 nullptr / 0 / 无效果)
    // =========================================================================

    /// @brief 获取底层像素数据的指针 (零拷贝模式专用)。
    ///        调用方可直接读取这块内存, 或交给 GDI / Direct2D / SDL / OpenGL 上传。
    /// @return 零拷贝模式下返回用户绑定的 buffer 指针; 普通模式下返回 nullptr。
    unsigned char* GetData() const noexcept;

    /// @brief [调试] 直接读 m_surfaceRead 的 data 指针, 跳过 m_imageSurface
    unsigned char* DebugGetReadSurfaceData() const noexcept {
        if (m_surfaceRead) return cairo_image_surface_get_data(m_surfaceRead);
        return nullptr;
    }

    /// @brief [调试] 直接读 m_surfaceDraw 的 data 指针
    unsigned char* DebugGetDrawSurfaceData() const noexcept {
        if (m_surfaceDraw) return cairo_image_surface_get_data(m_surfaceDraw);
        return nullptr;
    }

    /// @brief [调试] 看 cairo 内部的 image_surface 的 data 指针
    static unsigned char* DebugCairoImageData(cairo_surface_t* s) {
        return cairo_image_surface_get_data(s);
    }

    /// @brief 获取每一行的字节数 (stride)。
    int GetStride() const noexcept { return m_stride; }

    /// @brief 获取当前像素格式。
    PixelFormat GetFormat() const noexcept { return m_format; }

    /// @brief 强制把 Cairo 内部缓存写回用户 buffer。
    ///        在每帧绘制结束后调用一次, 保证下一帧从 GPU / 显示端读到的就是最新内容。
    void Flush();

    /// @brief 把整张画布标记为"已修改"。通常在 Flush() 之后调用,
    ///        下次 cairo 读取时会跳过内部缓存, 直接走用户 buffer。
    void MarkDirty();

    /// @brief 标记一个矩形区域为"已修改", 用于脏矩形增量上传。
    /// @param x, y  矩形左上角 (像素)
    /// @param w, h  矩形宽高 (像素)
    void MarkDirtyRectangle(int x, int y, int w, int h);

    /// @brief 重新绑一块新的 buffer (零拷贝模式专用)。
    ///        适用于 resize 或 swap-chain 翻页时切换 buffer。
    ///        内部会销毁旧 surface / context, 重新创建, 因此**有少量开销**,
    ///        不建议放在每帧循环里, 仅在窗口尺寸变化 / buffer 切换时调用。
    /// @return true 成功, false 失败 (例如参数非法 / cairo 资源不足)
    bool RebindBuffer(int width, int height,
                      unsigned char* externalData, int stride);

    /// @brief 当前是否处于零拷贝模式。
    bool IsLiveMode() const noexcept { return m_isLive; }

    // =========================================================================
    // 双缓冲 (解决实时画面闪烁)
    // =========================================================================

    /// @brief 当前是否启用了内置双缓冲。
    bool IsDoubleBuffered() const noexcept { return m_isDoubleBuffered; }

    /// @brief 手动交换两个 buffer (双缓冲模式下)。
    ///
    /// 默认情况下 Flush() 会自动 swap; 如果你的循环需要更精确控制 swap 时机
    /// (例如双缓冲 + 等待垂直同步), 可以 SetAutoSwapOnFlush(false), 然后
    /// 在合适的时机手动调 SwapBuffers。
    void SwapBuffers();

    /// @brief 设置 Flush() 是否自动 swap (默认 true)。
    void SetAutoSwapOnFlush(bool enabled) noexcept { m_autoSwapOnFlush = enabled; }

private:
    int               m_width;            ///< 画布宽度
    int               m_height;           ///< 画布高度
    cairo_surface_t*  m_recordingSurface; ///< recording surface, 普通模式记录所有绘制操作
    cairo_surface_t*  m_imageSurface;     ///< image surface, 零拷贝模式下绑用户 buffer
    cairo_t*          m_cr;               ///< 与当前活动 surface 关联的绘图上下文
    int               m_stride;           ///< 零拷贝模式下的行字节数, 普通模式为 0
    PixelFormat       m_format;           ///< 零拷贝模式下的像素格式
    bool              m_isLive;           ///< true=零拷贝模式, false=普通模式

    // ----- 双缓冲成员 (普通模式启用时使用) -----
    bool              m_isDoubleBuffered = false;  ///< 是否启用内置双缓冲
    bool              m_autoSwapOnFlush  = true;   ///< Flush() 时自动 swap
    std::vector<unsigned char> m_bufA;            ///< 内部 buffer A
    std::vector<unsigned char> m_bufB;            ///< 内部 buffer B
    cairo_surface_t*  m_surfaceDraw = nullptr;     ///< 当前绘制目标
    cairo_surface_t*  m_surfaceRead = nullptr;     ///< 当前外部读的目标
};

} // namespace memgc

#endif // MEMGC_CAIRO_CANVAS_H