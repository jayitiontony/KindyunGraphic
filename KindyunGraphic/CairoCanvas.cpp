// =============================================================================
// CairoCanvas.cpp
//
// CairoCanvas 头文件对应的实现, 详细接口说明请参见 CairoCanvas.h。
//
// =============================================================================

#include "CairoCanvas.h"

// Cairo 的 SVG / PDF / PS 后端 API 在独立的头文件中, 需要单独包含。
extern "C" {
#include <cairo-svg.h>
#include <cairo-pdf.h>
#include <cairo-ps.h>
}


#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

// MSVC / CRT 中一般没有 M_PI, 这里手动定义一下, 仅在本地编译单元生效。
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace memgc {

namespace {

/// @brief 把 cairo 状态码格式化成可读字符串, 用于异常信息。
std::string FormatCairoStatus(cairo_status_t status) {
    return std::string("cairo status: ") + cairo_status_to_string(status);
}

/// @brief 把自定义的 PixelFormat 映射到 cairo_format_t。
cairo_format_t ToCairoFormat(PixelFormat fmt) {
    switch (fmt) {
        case PixelFormat::ARGB32:    return CAIRO_FORMAT_ARGB32;
        case PixelFormat::RGB24:     return CAIRO_FORMAT_RGB24;
        case PixelFormat::A8:        return CAIRO_FORMAT_A8;
        case PixelFormat::RGB16_565: return CAIRO_FORMAT_RGB16_565;
    }
    return CAIRO_FORMAT_ARGB32;
}

} // namespace

// =============================================================================
// 构造 / 析构
// =============================================================================

CairoCanvas::CairoCanvas(int width, int height, bool doubleBuffered)
    : m_width(width),
      m_height(height),
      m_recordingSurface(nullptr),
      m_imageSurface(nullptr),
      m_cr(nullptr),
      m_stride(0),
      m_format(PixelFormat::ARGB32),
      m_isLive(false),
      m_isDoubleBuffered(doubleBuffered),
      m_autoSwapOnFlush(true),
      m_surfaceDraw(nullptr),
      m_surfaceRead(nullptr)
{
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("CairoCanvas: width/height 必须为正整数");
    }

    if (doubleBuffered) {
        // --------------------------------------------------------------
        // 双缓冲模式: 内部分配 2 个 buffer, 走 image surface 路径
        // --------------------------------------------------------------
        m_stride = m_width * 4;
        m_format = PixelFormat::ARGB32;
        m_bufA.assign(static_cast<size_t>(m_stride) * m_height, 0);
        m_bufB.assign(static_cast<size_t>(m_stride) * m_height, 0);
        unsigned char* bufA = m_bufA.data();
        unsigned char* bufB = m_bufB.data();

        m_surfaceDraw = cairo_image_surface_create_for_data(
            bufA, CAIRO_FORMAT_ARGB32, m_width, m_height, m_stride);
        m_surfaceRead = cairo_image_surface_create_for_data(
            bufB, CAIRO_FORMAT_ARGB32, m_width, m_height, m_stride);

        if (cairo_surface_status(m_surfaceDraw) != CAIRO_STATUS_SUCCESS
            || cairo_surface_status(m_surfaceRead) != CAIRO_STATUS_SUCCESS) {
            std::string msg = "cairo_image_surface_create_for_data failed";
            if (m_surfaceDraw) cairo_surface_destroy(m_surfaceDraw);
            if (m_surfaceRead) cairo_surface_destroy(m_surfaceRead);
            throw std::runtime_error(msg);
        }

        m_imageSurface = m_surfaceRead;        // 兼容旧 API: GetData / SaveAsPng 走 read
        m_cr = cairo_create(m_surfaceDraw);
        if (cairo_status(m_cr) != CAIRO_STATUS_SUCCESS) {
            std::string msg = "cairo_create failed: "
                            + FormatCairoStatus(cairo_status(m_cr));
            cairo_destroy(m_cr); m_cr = nullptr;
            cairo_surface_destroy(m_surfaceDraw); m_surfaceDraw = nullptr;
            cairo_surface_destroy(m_surfaceRead); m_surfaceRead = nullptr;
            throw std::runtime_error(msg);
        }
    } else {
        // --------------------------------------------------------------
        // 单缓冲 + recording surface 模式: 支持真矢量导出 (SVG/PDF/PS)
        // --------------------------------------------------------------
        cairo_rectangle_t extents;
        extents.x = 0.0;
        extents.y = 0.0;
        extents.width  = static_cast<double>(m_width);
        extents.height = static_cast<double>(m_height);

        m_recordingSurface = cairo_recording_surface_create(
            CAIRO_CONTENT_COLOR_ALPHA, &extents);
        if (cairo_surface_status(m_recordingSurface) != CAIRO_STATUS_SUCCESS) {
            std::string msg = "cairo_recording_surface_create failed: "
                            + FormatCairoStatus(
                                  cairo_surface_status(m_recordingSurface));
            cairo_surface_destroy(m_recordingSurface);
            m_recordingSurface = nullptr;
            throw std::runtime_error(msg);
        }

        m_cr = cairo_create(m_recordingSurface);
        if (cairo_status(m_cr) != CAIRO_STATUS_SUCCESS) {
            std::string msg = "cairo_create failed: "
                            + FormatCairoStatus(cairo_status(m_cr));
            cairo_destroy(m_cr);
            cairo_surface_destroy(m_recordingSurface);
            m_cr = nullptr;
            m_recordingSurface = nullptr;
            throw std::runtime_error(msg);
        }
    }

    // 启用默认抗锯齿, 让线条 / 曲线 / 圆弧边缘更平滑。
    cairo_set_antialias(m_cr, CAIRO_ANTIALIAS_DEFAULT);

    // 默认字体 / 线宽, 避免初次绘制时属性未定义。
    cairo_set_line_width(m_cr, 1.0);
    cairo_select_font_face(m_cr, "sans-serif",
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(m_cr, 12.0);
}

CairoCanvas::CairoCanvas(int width, int height,
                         unsigned char* externalData, int stride,
                         PixelFormat fmt)
    : m_width(width),
      m_height(height),
      m_recordingSurface(nullptr),
      m_imageSurface(nullptr),
      m_cr(nullptr),
      m_stride(stride),
      m_format(fmt),
      m_isLive(true)
{
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("CairoCanvas: width/height 必须为正整数");
    }
    if (!externalData) {
        throw std::runtime_error("CairoCanvas: externalData 不能为空");
    }
    if (stride <= 0) {
        throw std::runtime_error("CairoCanvas: stride 必须为正整数");
    }

    // ------------------------------------------------------------------
    // 零拷贝模式: 直接在用户提供的 buffer 上创建 image surface。
    // Cairo 不会复制 buffer, 不会在析构时 free, 内存由调用方全权管理。
    // ------------------------------------------------------------------
    m_imageSurface = cairo_image_surface_create_for_data(
        externalData, ToCairoFormat(fmt), m_width, m_height, m_stride);
    if (cairo_surface_status(m_imageSurface) != CAIRO_STATUS_SUCCESS) {
        std::string msg = "cairo_image_surface_create_for_data failed: "
                        + FormatCairoStatus(
                              cairo_surface_status(m_imageSurface));
        cairo_surface_destroy(m_imageSurface);
        m_imageSurface = nullptr;
        throw std::runtime_error(msg);
    }

    m_cr = cairo_create(m_imageSurface);
    if (cairo_status(m_cr) != CAIRO_STATUS_SUCCESS) {
        std::string msg = "cairo_create failed: "
                        + FormatCairoStatus(cairo_status(m_cr));
        cairo_destroy(m_cr);
        cairo_surface_destroy(m_imageSurface);
        m_cr = nullptr;
        m_imageSurface = nullptr;
        throw std::runtime_error(msg);
    }

    cairo_set_antialias(m_cr, CAIRO_ANTIALIAS_DEFAULT);
    cairo_set_line_width(m_cr, 1.0);
    cairo_select_font_face(m_cr, "sans-serif",
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(m_cr, 12.0);
}

CairoCanvas::~CairoCanvas() {
    if (m_cr) {
        cairo_destroy(m_cr);
        m_cr = nullptr;
    }
    // 释放双缓冲的 2 个 image surface (m_imageSurface 只是其中一个的指针,
    // 不要重复 destroy)
    if (m_surfaceDraw) {
        cairo_surface_destroy(m_surfaceDraw);
        m_surfaceDraw = nullptr;
    }
    if (m_surfaceRead) {
        cairo_surface_destroy(m_surfaceRead);
        m_surfaceRead = nullptr;
    }
    m_imageSurface = nullptr;
    if (m_recordingSurface) {
        cairo_surface_destroy(m_recordingSurface);
        m_recordingSurface = nullptr;
    }
}

// =============================================================================
// 基础状态设置
// =============================================================================

void CairoCanvas::SetSourceColor(const Color& color) {
    if (color.a >= 1.0) {
        // 完全不透明, 用效率更高的 rgb 接口
        cairo_set_source_rgb(m_cr, color.r, color.g, color.b);
    } else {
        cairo_set_source_rgba(m_cr, color.r, color.g, color.b, color.a);
    }
}

void CairoCanvas::SetSourceRGB(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    SetSourceColor(Color::FromRGB(r, g, b, a));
}

void CairoCanvas::SetLineWidth(double width) {
    cairo_set_line_width(m_cr, width);
}

void CairoCanvas::SetLineCap(LineCap cap) {
    cairo_set_line_cap(m_cr, static_cast<cairo_line_cap_t>(cap));
}

void CairoCanvas::SetLineJoin(LineJoin join) {
    cairo_set_line_join(m_cr, static_cast<cairo_line_join_t>(join));
}

void CairoCanvas::SetDash(const std::vector<double>& dashes, double offset) {
    if (dashes.empty()) {
        cairo_set_dash(m_cr, nullptr, 0, offset);
        return;
    }
    cairo_set_dash(m_cr, dashes.data(),
                   static_cast<int>(dashes.size()), offset);
}

void CairoCanvas::ClearDash() {
    cairo_set_dash(m_cr, nullptr, 0, 0.0);
}

void CairoCanvas::SetFillRule(FillRule rule) {
    cairo_set_fill_rule(m_cr, static_cast<cairo_fill_rule_t>(rule));
}

// =============================================================================
// 路径操作
// =============================================================================

void CairoCanvas::MoveTo(double x, double y) {
    cairo_move_to(m_cr, x, y);
}

void CairoCanvas::LineTo(double x, double y) {
    cairo_line_to(m_cr, x, y);
}

void CairoCanvas::CurveTo(double x1, double y1,
                          double x2, double y2,
                          double x3, double y3) {
    cairo_curve_to(m_cr, x1, y1, x2, y2, x3, y3);
}

void CairoCanvas::Arc(double cx, double cy,
                      double radius,
                      double startAngle,
                      double endAngle) {
    cairo_arc(m_cr, cx, cy, radius, startAngle, endAngle);
}

void CairoCanvas::ClosePath() {
    cairo_close_path(m_cr);
}

void CairoCanvas::Stroke() {
    cairo_stroke(m_cr);
}

void CairoCanvas::Fill() {
    cairo_fill(m_cr);
}

void CairoCanvas::StrokeAndFill() {
    // cairo_stroke_preserve 描边后保留路径, 再 cairo_fill 填充, 实现
    // "描边 + 填充" 一次性效果。
    cairo_stroke_preserve(m_cr);
    cairo_fill(m_cr);
}

void CairoCanvas::NewPath() {
    cairo_new_path(m_cr);
}

// =============================================================================
// 高级图形
// =============================================================================

void CairoCanvas::DrawLine(double x1, double y1, double x2, double y2) {
    cairo_new_path(m_cr);
    cairo_move_to(m_cr, x1, y1);
    cairo_line_to(m_cr, x2, y2);
    cairo_stroke(m_cr);
}

void CairoCanvas::DrawPolyline(const std::vector<Point>& points) {
    if (points.size() < 2) return;       // 至少需要两个点才画得出线

    cairo_new_path(m_cr);
    cairo_move_to(m_cr, points[0].x, points[0].y);
    for (std::size_t i = 1; i < points.size(); ++i) {
        cairo_line_to(m_cr, points[i].x, points[i].y);
    }
    cairo_stroke(m_cr);
}

void CairoCanvas::DrawPolygon(const std::vector<Point>& points, bool fill) {
    if (points.size() < 3) return;       // 至少三个点才构成多边形

    cairo_new_path(m_cr);
    cairo_move_to(m_cr, points[0].x, points[0].y);
    for (std::size_t i = 1; i < points.size(); ++i) {
        cairo_line_to(m_cr, points[i].x, points[i].y);
    }
    cairo_close_path(m_cr);              // 回到起点, 形成闭合路径

    if (fill) {
        cairo_fill(m_cr);
    } else {
        cairo_stroke(m_cr);
    }
}

void CairoCanvas::DrawRectangle(double x, double y,
                                double w, double h,
                                bool fill) {
    cairo_new_path(m_cr);
    cairo_rectangle(m_cr, x, y, w, h);
    if (fill) {
        cairo_fill(m_cr);
    } else {
        cairo_stroke(m_cr);
    }
}

void CairoCanvas::DrawCircle(double cx, double cy, double r, bool fill) {
    cairo_new_path(m_cr);
    // 完整圆: 从 0 到 2π 的弧
    cairo_arc(m_cr, cx, cy, r, 0.0, 2.0 * M_PI);
    if (fill) {
        cairo_fill(m_cr);
    } else {
        cairo_stroke(m_cr);
    }
}

void CairoCanvas::DrawEllipse(double cx, double cy,
                              double rx, double ry,
                              double rotation,
                              bool fill) {
    // 椭圆不是 cairo 的内置原语, 通过"先缩放成单位圆, 再反向变换"实现。
    // 步骤:
    //   1. 保存当前状态, 平移到椭圆中心, 旋转角度;
    //   2. 用非均匀缩放 (rx, ry) 把单位圆"拉"成椭圆;
    //   3. 画一个单位圆 (半径 1);
    //   4. 恢复状态。
    //
    // ★重要: cairo_set_line_width 设的线宽也会被 cairo_scale 放大!
    //    设 line=2, cairo_scale(35, 15) 后实际描边 = (70, 30), 比 rx=35 还厚,
    //    椭圆看起来像填充。要让描边看起来 "跟原 user space 设的宽度一致",
    //    必须把 line width 按 "user width × (1/单位半径在 user space 的尺度)" 缩小。
    //    这里单位圆半径=1 在 user space 里最大尺度 = max(rx, ry),
    //    所以 line_width_userSpace = currentLineWidth / max(rx, ry)。
    cairo_save(m_cr);
    cairo_translate(m_cr, cx, cy);
    if (rotation != 0.0) {
        cairo_rotate(m_cr, rotation);
    }
    cairo_scale(m_cr, rx, ry);

    if (!fill) {
        // 把 user space 的 line width 转回 unit circle 坐标系: lw / max(rx,ry)
        const double curLineWidth = cairo_get_line_width(m_cr);
        const double unitScale    = (rx > ry ? rx : ry);   // max
        cairo_set_line_width(m_cr, curLineWidth / unitScale);
    }

    cairo_new_path(m_cr);
    cairo_arc(m_cr, 0.0, 0.0, 1.0, 0.0, 2.0 * M_PI);

    if (fill) {
        cairo_fill(m_cr);
    } else {
        cairo_stroke(m_cr);
    }
    cairo_restore(m_cr);
}

void CairoCanvas::DrawArc(double cx, double cy,
                          double r,
                          double startAngle,
                          double endAngle) {
    cairo_new_path(m_cr);
    cairo_arc(m_cr, cx, cy, r, startAngle, endAngle);
    cairo_stroke(m_cr);
}

void CairoCanvas::DrawCurve(double cp1x, double cp1y,
                            double cp2x, double cp2y,
                            double endX, double endY) {
    // 起点为最近一次 MoveTo / LineTo 的位置, 由调用方保证
    cairo_curve_to(m_cr, cp1x, cp1y, cp2x, cp2y, endX, endY);
    cairo_stroke(m_cr);
}

void CairoCanvas::DrawCurveFrom(double sx, double sy,
                                double cp1x, double cp1y,
                                double cp2x, double cp2y,
                                double endX, double endY) {
    cairo_new_path(m_cr);
    cairo_move_to(m_cr, sx, sy);
    cairo_curve_to(m_cr, cp1x, cp1y, cp2x, cp2y, endX, endY);
    cairo_stroke(m_cr);
}

// =============================================================================
// 文字
// =============================================================================

void CairoCanvas::SetFont(const FontStyle& font) {
    cairo_font_weight_t weight = font.bold
        ? CAIRO_FONT_WEIGHT_BOLD
        : CAIRO_FONT_WEIGHT_NORMAL;
    cairo_font_slant_t slant = font.italic
        ? CAIRO_FONT_SLANT_ITALIC
        : CAIRO_FONT_SLANT_NORMAL;
    cairo_select_font_face(m_cr, font.family.c_str(), slant, weight);
    cairo_set_font_size(m_cr, font.size);
}

void CairoCanvas::SetFontSize(double size) {
    cairo_set_font_size(m_cr, size);
}

void CairoCanvas::DrawText(double x, double y, const std::string& text) {
    cairo_move_to(m_cr, x, y);
    cairo_show_text(m_cr, text.c_str());
}

void CairoCanvas::DrawTextUTF8(double x, double y, const std::string& text) {
    // cairo 1.x 的 toy text API (cairo_show_text) 内部按 UTF-8 解析字节,
    // 所以这里与 DrawText 实现一致; 但语义上明确表示"UTF-8 输入"方便阅读。
    cairo_move_to(m_cr, x, y);
    cairo_show_text(m_cr, text.c_str());
}

double CairoCanvas::GetTextWidth(const std::string& text) const {
    cairo_text_extents_t extents;
    cairo_text_extents(m_cr, text.c_str(), &extents);
    return extents.x_advance;
}

void CairoCanvas::GetTextExtents(const std::string& text,
                                 double* x_bearing,
                                 double* y_bearing,
                                 double* width,
                                 double* height) const {
    cairo_text_extents_t extents;
    cairo_text_extents(m_cr, text.c_str(), &extents);
    if (x_bearing) *x_bearing = extents.x_bearing;
    if (y_bearing) *y_bearing = extents.y_bearing;
    if (width)     *width     = extents.width;
    if (height)    *height    = extents.height;
}

// =============================================================================
// 画布整体操作
// =============================================================================

void CairoCanvas::Clear(const Color& color) {
    // 临时切换合成算子为 SOURCE, 使 paint 不与已有内容做混合,
    // 这样可以真正把整张画布替换为指定颜色 (包括 alpha 通道)。
    cairo_save(m_cr);
    cairo_set_operator(m_cr, CAIRO_OPERATOR_SOURCE);
    SetSourceColor(color);
    cairo_paint(m_cr);
    cairo_restore(m_cr);
}

void CairoCanvas::Translate(double dx, double dy) {
    cairo_translate(m_cr, dx, dy);
}

void CairoCanvas::Scale(double sx, double sy) {
    cairo_scale(m_cr, sx, sy);
}

void CairoCanvas::Rotate(double radians) {
    cairo_rotate(m_cr, radians);
}

void CairoCanvas::Save() {
    cairo_save(m_cr);
}

void CairoCanvas::Restore() {
    cairo_restore(m_cr);
}

// =============================================================================
// 导出 / 保存
// =============================================================================

bool CairoCanvas::SaveAsPng(const std::string& filepath) const {
    if ((m_isLive || m_isDoubleBuffered) && m_imageSurface) {
        // 零拷贝 / 双缓冲模式: 直接对绑了 read buffer 的 image surface 写 PNG。
        // 这时导出的是已经光栅化后的位图, 仍然完整正确, 只是不能继续
        // 缩放无损。
        cairo_surface_flush(m_imageSurface);
        return cairo_surface_write_to_png(m_imageSurface, filepath.c_str())
               == CAIRO_STATUS_SUCCESS;
    }

    // 普通模式: 创建一张 ARGB32 内存 surface, 把 recording surface 作为源
    //       paint 上去, cairo 会把 recording surface 记录的所有操作
    //       replay 到 image surface 上, 然后用 cairo_surface_write_to_png
    //       写出 PNG。
    cairo_surface_t* image = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, m_width, m_height);
    if (cairo_surface_status(image) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(image);
        return false;
    }

    cairo_t* cr = cairo_create(image);
    if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
        cairo_destroy(cr);
        cairo_surface_destroy(image);
        return false;
    }

    cairo_set_source_surface(cr, m_recordingSurface, 0.0, 0.0);
    cairo_paint(cr);

    cairo_status_t writeStatus =
        cairo_surface_write_to_png(image, filepath.c_str());

    cairo_destroy(cr);
    cairo_surface_destroy(image);
    return writeStatus == CAIRO_STATUS_SUCCESS;
}

bool CairoCanvas::SaveAsSvg(const std::string& filepath) const {
    if ((m_isLive || m_isDoubleBuffered) && m_imageSurface) {
        // 零拷贝 / 双缓冲模式没有 recording surface, 只能 paint 已栅格化的位图。
        cairo_surface_flush(m_imageSurface);
        cairo_surface_t* svg =
            cairo_svg_surface_create(filepath.c_str(), m_width, m_height);
        if (cairo_surface_status(svg) != CAIRO_STATUS_SUCCESS) {
            cairo_surface_destroy(svg);
            return false;
        }
        cairo_t* cr = cairo_create(svg);
        if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
            cairo_destroy(cr);
            cairo_surface_destroy(svg);
            return false;
        }
        cairo_set_source_surface(cr, m_imageSurface, 0.0, 0.0);
        cairo_paint(cr);
        cairo_surface_flush(svg);
        cairo_surface_destroy(svg);
        cairo_destroy(cr);
        return true;
    }

    // 创建 SVG surface, 再把 recording surface 内容 paint 上去。
    // cairo 在 SVG 后端看到源是 recording surface 时, 会 replay 所有
    // 记录的操作, 输出真正的矢量 SVG 元素 (<path>, <circle>, <line> 等),
    // 而不是把位图嵌入 SVG。
    cairo_surface_t* svg =
        cairo_svg_surface_create(filepath.c_str(), m_width, m_height);
    if (cairo_surface_status(svg) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(svg);
        return false;
    }

    cairo_t* cr = cairo_create(svg);
    if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
        cairo_destroy(cr);
        cairo_surface_destroy(svg);
        return false;
    }

    cairo_set_source_surface(cr, m_recordingSurface, 0.0, 0.0);
    cairo_paint(cr);

    // cairo_surface_flush / finish 触发 SVG 文件实际写入磁盘
    cairo_surface_flush(svg);
    cairo_surface_destroy(svg);
    cairo_destroy(cr);
    return true;
}

bool CairoCanvas::SaveAsPdf(const std::string& filepath) const {
    if ((m_isLive || m_isDoubleBuffered) && m_imageSurface) {
        cairo_surface_flush(m_imageSurface);
        cairo_surface_t* pdf =
            cairo_pdf_surface_create(filepath.c_str(), m_width, m_height);
        if (cairo_surface_status(pdf) != CAIRO_STATUS_SUCCESS) {
            cairo_surface_destroy(pdf);
            return false;
        }
        cairo_t* cr = cairo_create(pdf);
        if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
            cairo_destroy(cr);
            cairo_surface_destroy(pdf);
            return false;
        }
        cairo_set_source_surface(cr, m_imageSurface, 0.0, 0.0);
        cairo_paint(cr);
        cairo_show_page(cr);
        cairo_surface_flush(pdf);
        cairo_surface_destroy(pdf);
        cairo_destroy(cr);
        return true;
    }

    cairo_surface_t* pdf =
        cairo_pdf_surface_create(filepath.c_str(), m_width, m_height);
    if (cairo_surface_status(pdf) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(pdf);
        return false;
    }

    cairo_t* cr = cairo_create(pdf);
    if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
        cairo_destroy(cr);
        cairo_surface_destroy(pdf);
        return false;
    }

    cairo_set_source_surface(cr, m_recordingSurface, 0.0, 0.0);
    cairo_paint(cr);
    cairo_show_page(cr);                // PDF 必须显式换页

    cairo_surface_flush(pdf);
    cairo_surface_destroy(pdf);
    cairo_destroy(cr);
    return true;
}

bool CairoCanvas::SaveAsPs(const std::string& filepath) const {
    if ((m_isLive || m_isDoubleBuffered) && m_imageSurface) {
        cairo_surface_flush(m_imageSurface);
        cairo_surface_t* ps =
            cairo_ps_surface_create(filepath.c_str(), m_width, m_height);
        if (cairo_surface_status(ps) != CAIRO_STATUS_SUCCESS) {
            cairo_surface_destroy(ps);
            return false;
        }
        cairo_t* cr = cairo_create(ps);
        if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
            cairo_destroy(cr);
            cairo_surface_destroy(ps);
            return false;
        }
        cairo_set_source_surface(cr, m_imageSurface, 0.0, 0.0);
        cairo_paint(cr);
        cairo_show_page(cr);
        cairo_surface_flush(ps);
        cairo_surface_destroy(ps);
        cairo_destroy(cr);
        return true;
    }

    cairo_surface_t* ps =
        cairo_ps_surface_create(filepath.c_str(), m_width, m_height);
    if (cairo_surface_status(ps) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(ps);
        return false;
    }

    cairo_t* cr = cairo_create(ps);
    if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
        cairo_destroy(cr);
        cairo_surface_destroy(ps);
        return false;
    }

    cairo_set_source_surface(cr, m_recordingSurface, 0.0, 0.0);
    cairo_paint(cr);
    cairo_show_page(cr);

    cairo_surface_flush(ps);
    cairo_surface_destroy(ps);
    cairo_destroy(cr);
    return true;
}

bool CairoCanvas::Save(const std::string& filepath, SaveFormat format) const {
    switch (format) {
        case SaveFormat::Png: return SaveAsPng(filepath);
        case SaveFormat::Svg: return SaveAsSvg(filepath);
        case SaveFormat::Pdf: return SaveAsPdf(filepath);
        case SaveFormat::Ps:  return SaveAsPs(filepath);
        default: return false;
    }
}

// =============================================================================
// 零拷贝模式专用方法
// =============================================================================

unsigned char* CairoCanvas::GetData() const noexcept {
    if ((m_isLive || m_isDoubleBuffered) && m_imageSurface) {
        // 零拷贝: m_imageSurface 绑用户 buffer;
        // 双缓冲: m_imageSurface 指向 m_surfaceRead (外部读目标)。
        return cairo_image_surface_get_data(m_imageSurface);
    }
    return nullptr;
}

void CairoCanvas::Flush() {
    if (m_isDoubleBuffered) {
        // 1) 把当前 draw 表面写回其绑定的 buffer
        if (m_surfaceDraw) cairo_surface_flush(m_surfaceDraw);
        // 2) 双缓冲: 交换 draw / read buffer, 下一帧绘制到原 read buffer
        if (m_autoSwapOnFlush) {
            SwapBuffers();
        }
        return;
    }
    if (m_isLive && m_imageSurface) {
        // 零拷贝: 把 Cairo 内部缓存写回用户 buffer
        cairo_surface_flush(m_imageSurface);
    }
}

void CairoCanvas::MarkDirty() {
    if ((m_isLive || m_isDoubleBuffered) && m_imageSurface) {
        cairo_surface_mark_dirty(m_imageSurface);
    }
}

void CairoCanvas::MarkDirtyRectangle(int x, int y, int w, int h) {
    if ((m_isLive || m_isDoubleBuffered) && m_imageSurface) {
        cairo_surface_mark_dirty_rectangle(m_imageSurface,
                                           static_cast<double>(x),
                                           static_cast<double>(y),
                                           static_cast<double>(w),
                                           static_cast<double>(h));
    }
}

void CairoCanvas::SwapBuffers() {
    if (!m_isDoubleBuffered || !m_surfaceDraw || !m_surfaceRead) return;

    // ----------------------------------------------------------------------
    // 状态 (swap 前):
    //   m_surfaceDraw  绑 m_bufA (刚画完, 像素就绪)
    //   m_surfaceRead  绑 m_bufB (空, 上一帧被外部读)
    // 状态 (swap 后):
    //   m_surfaceDraw  绑 m_bufB (新空白, 下一帧画这里)
    //   m_surfaceRead  绑 m_bufA (刚画完, 外部下一帧读这个)
    //   m_imageSurface = m_surfaceRead
    //
    // ★关键: swap 只是指针重命名, **不 destroy 任何 surface**!
    //   旧 draw surface 变成新 read (要保留给外部读);
    //   旧 read surface 变成新 draw (要保留给下一帧画)。
    //   需要销毁重建的只是 cairo_t (它绑的 surface 角色变了)。
    // ----------------------------------------------------------------------

    // 1) flush 当前 draw, 把 cairo 内部缓存写回绑定的 buffer (m_bufA)
    cairo_surface_flush(m_surfaceDraw);

    // 2) swap 指针: m_surfaceDraw ↔ m_surfaceRead
    std::swap(m_surfaceDraw, m_surfaceRead);
    // 现在 m_surfaceDraw = 绑 m_bufB (旧 read, 现在是新 draw)
    //     m_surfaceRead = 绑 m_bufA (旧 draw, 现在是新 read)

    // 3) 外部读目标 = 新 read
    m_imageSurface = m_surfaceRead;

    // 4) 销毁旧 cairo_t (它绑的是 cairo 对象的视角, swap 不影响 cairo_t 内部指针)
    //    cairo_t 现在绑的是绑 m_bufA 的 surface (已变成 read), 不能用于绘制
    if (m_cr) {
        cairo_destroy(m_cr);
        m_cr = nullptr;
    }

    // 5) 重建 cairo_t 绑到新 draw surface (绑 m_bufB)
    m_cr = cairo_create(m_surfaceDraw);
    if (cairo_status(m_cr) != CAIRO_STATUS_SUCCESS) {
        return;  // 失败不抛异常
    }

    // 6) 重建默认状态 (cairo_create 不会继承上次的 gstate)
    cairo_set_antialias(m_cr, CAIRO_ANTIALIAS_DEFAULT);
    cairo_set_line_width(m_cr, 1.0);
    cairo_select_font_face(m_cr, "sans-serif",
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(m_cr, 12.0);
}
bool CairoCanvas::RebindBuffer(int width, int height,
                               unsigned char* externalData, int stride) {
    if (!m_isLive) {
        return false;                   // 仅在零拷贝模式下可用
    }
    if (width <= 0 || height <= 0 || !externalData || stride <= 0) {
        return false;
    }

    // 销毁旧 surface / context (Cairo 1.18 没有 cairo_image_surface_set_data,
    // 只能重建)。这一步有少量开销, 建议仅在 resize / 换 buffer 时调用。
    if (m_cr) {
        cairo_destroy(m_cr);
        m_cr = nullptr;
    }
    if (m_imageSurface) {
        cairo_surface_destroy(m_imageSurface);
        m_imageSurface = nullptr;
    }

    m_width  = width;
    m_height = height;
    m_stride = stride;

    m_imageSurface = cairo_image_surface_create_for_data(
        externalData, ToCairoFormat(m_format), m_width, m_height, m_stride);
    if (cairo_surface_status(m_imageSurface) != CAIRO_STATUS_SUCCESS) {
        return false;
    }

    m_cr = cairo_create(m_imageSurface);
    if (cairo_status(m_cr) != CAIRO_STATUS_SUCCESS) {
        cairo_destroy(m_cr);
        cairo_surface_destroy(m_imageSurface);
        m_cr = nullptr;
        m_imageSurface = nullptr;
        return false;
    }

    // 重新设置默认属性 (cairo_create 不会继承上次的 state)
    cairo_set_antialias(m_cr, CAIRO_ANTIALIAS_DEFAULT);
    cairo_set_line_width(m_cr, 1.0);
    cairo_select_font_face(m_cr, "sans-serif",
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(m_cr, 12.0);
    return true;
}



} // namespace memgc