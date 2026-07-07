// =============================================================================
// KindyunGraphic.cpp
//
// 把 memgc::CairoCanvas 封装成纯 C 风格 API 给 MFC / 其他语言调用。
// cairo2 / libpng / pixman / freetype / zlib 全部静态链入本 DLL, 运行时
// 只需 KindyunGraphic.dll 一个文件。
// =============================================================================

// KINDYUN_GRAPHIC_BUILD 在 vcxproj 的 PreprocessorDefinitions 里定义了,
// 这里不要再 #define, 否则会出 C4005 宏重定义警告。

#include "KindyunGraphic.h"

// 把 CairoCanvas 头文件 include 进来; 实现文件 (CairoCanvas.cpp) 在
// KindyunGraphic.vcxproj 里作为独立源文件添加, 这样 CairoCanvas 类只编译一次。
#include "../TestCAiro2/CairoCanvas.h"

#include <cstring>
#include <string>
#include <vector>

using memgc::CairoCanvas;
using memgc::Color;
using memgc::FontStyle;
using memgc::LineCap;
using memgc::LineJoin;
using memgc::PixelFormat;
using memgc::Point;
using memgc::SaveFormat;

// =============================================================================
// 内部辅助: opaque 句柄 <-> CairoCanvas*
// =============================================================================

namespace {

inline CairoCanvas* ToCanvas(KGCanvas h) {
    return reinterpret_cast<CairoCanvas*>(h);
}

inline KGCanvas ToHandle(CairoCanvas* p) {
    return reinterpret_cast<KGCanvas>(p);
}

PixelFormat ToPixelFormat(int fmt) {
    switch (fmt) {
        case KG_FMT_ARGB32:    return PixelFormat::ARGB32;
        case KG_FMT_RGB24:     return PixelFormat::RGB24;
        case KG_FMT_A8:        return PixelFormat::A8;
        case KG_FMT_RGB16_565: return PixelFormat::RGB16_565;
        default:               return PixelFormat::ARGB32;
    }
}

SaveFormat ToSaveFormat(int fmt) {
    switch (fmt) {
        case KG_SAVE_PNG: return SaveFormat::Png;
        case KG_SAVE_SVG: return SaveFormat::Svg;
        case KG_SAVE_PDF: return SaveFormat::Pdf;
        case KG_SAVE_PS:  return SaveFormat::Ps;
        default:          return SaveFormat::Png;
    }
}

} // namespace

// =============================================================================
// 画布生命周期
// =============================================================================

KG_API KGCanvas KG_CALL KG_Create(int width, int height) {
    try {
        CairoCanvas* c = new CairoCanvas(width, height);
        return ToHandle(c);
    } catch (...) {
        return nullptr;
    }
}

KG_API KGCanvas KG_CALL KG_CreateFromBuffer(int width, int height,
                                             unsigned char* buffer, int stride,
                                             int fmt) {
    if (!buffer) return nullptr;
    try {
        CairoCanvas* c = new CairoCanvas(width, height, buffer, stride,
                                          ToPixelFormat(fmt));
        return ToHandle(c);
    } catch (...) {
        return nullptr;
    }
}

KG_API void KG_CALL KG_Destroy(KGCanvas canvas) {
    if (!canvas) return;
    delete ToCanvas(canvas);
}

KG_API void KG_CALL KG_SwapBuffers(KGCanvas canvas) {
    if (canvas) ToCanvas(canvas)->SwapBuffers();
}

KG_API void KG_CALL KG_SetAutoSwapOnFlush(KGCanvas canvas, int enabled) {
    if (canvas) ToCanvas(canvas)->SetAutoSwapOnFlush(enabled != 0);
}

KG_API int KG_CALL KG_IsDoubleBuffered(KGCanvas canvas) {
    return canvas ? (ToCanvas(canvas)->IsDoubleBuffered() ? 1 : 0) : 0;
}

// =============================================================================
// 元信息
// =============================================================================

KG_API int KG_CALL KG_GetWidth(KGCanvas canvas) {
    return canvas ? ToCanvas(canvas)->GetWidth() : 0;
}

KG_API int KG_CALL KG_GetHeight(KGCanvas canvas) {
    return canvas ? ToCanvas(canvas)->GetHeight() : 0;
}

KG_API int KG_CALL KG_GetStride(KGCanvas canvas) {
    return canvas ? ToCanvas(canvas)->GetStride() : 0;
}

KG_API int KG_CALL KG_IsLiveMode(KGCanvas canvas) {
    return canvas ? (ToCanvas(canvas)->IsLiveMode() ? 1 : 0) : 0;
}

KG_API unsigned char* KG_CALL KG_GetData(KGCanvas canvas) {
    return canvas ? ToCanvas(canvas)->GetData() : nullptr;
}

KG_API void KG_CALL KG_Flush(KGCanvas canvas) {
    if (canvas) ToCanvas(canvas)->Flush();
}

KG_API void KG_CALL KG_MarkDirty(KGCanvas canvas) {
    if (canvas) ToCanvas(canvas)->MarkDirty();
}

KG_API void KG_CALL KG_MarkDirtyRect(KGCanvas canvas, int x, int y, int w, int h) {
    if (canvas) ToCanvas(canvas)->MarkDirtyRectangle(x, y, w, h);
}

// =============================================================================
// 状态
// =============================================================================

KG_API void KG_CALL KG_SetSourceColor(KGCanvas canvas,
                                       double r, double g, double b, double a) {
    if (canvas) ToCanvas(canvas)->SetSourceColor(Color(r, g, b, a));
}

KG_API void KG_CALL KG_SetSourceRGB(KGCanvas canvas,
                                     uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (canvas) ToCanvas(canvas)->SetSourceRGB(r, g, b, a);
}

KG_API void KG_CALL KG_SetLineWidth(KGCanvas canvas, double width) {
    if (canvas) ToCanvas(canvas)->SetLineWidth(width);
}

KG_API void KG_CALL KG_SetLineCap(KGCanvas canvas, int cap) {
    if (canvas) ToCanvas(canvas)->SetLineCap(static_cast<LineCap>(cap));
}

KG_API void KG_CALL KG_SetLineJoin(KGCanvas canvas, int join) {
    if (canvas) ToCanvas(canvas)->SetLineJoin(static_cast<LineJoin>(join));
}

KG_API void KG_CALL KG_SetDash(KGCanvas canvas,
                                const double* dashes, int count, double offset) {
    if (!canvas) return;
    if (!dashes || count <= 0) {
        ToCanvas(canvas)->ClearDash();
        return;
    }
    ToCanvas(canvas)->SetDash(
        std::vector<double>(dashes, dashes + count), offset);
}

KG_API void KG_CALL KG_ClearDash(KGCanvas canvas) {
    if (canvas) ToCanvas(canvas)->ClearDash();
}

// =============================================================================
// 画布操作
// =============================================================================

KG_API void KG_CALL KG_Clear(KGCanvas canvas,
                             double r, double g, double b, double a) {
    if (canvas) ToCanvas(canvas)->Clear(Color(r, g, b, a));
}

KG_API void KG_CALL KG_Translate(KGCanvas canvas, double dx, double dy) {
    if (canvas) ToCanvas(canvas)->Translate(dx, dy);
}

KG_API void KG_CALL KG_Scale(KGCanvas canvas, double sx, double sy) {
    if (canvas) ToCanvas(canvas)->Scale(sx, sy);
}

KG_API void KG_CALL KG_Rotate(KGCanvas canvas, double radians) {
    if (canvas) ToCanvas(canvas)->Rotate(radians);
}

KG_API void KG_CALL KG_Save(KGCanvas canvas) {
    if (canvas) ToCanvas(canvas)->Save();
}

KG_API void KG_CALL KG_Restore(KGCanvas canvas) {
    if (canvas) ToCanvas(canvas)->Restore();
}

// =============================================================================
// 路径
// =============================================================================

KG_API void KG_CALL KG_MoveTo(KGCanvas canvas, double x, double y) {
    if (canvas) ToCanvas(canvas)->MoveTo(x, y);
}

KG_API void KG_CALL KG_LineTo(KGCanvas canvas, double x, double y) {
    if (canvas) ToCanvas(canvas)->LineTo(x, y);
}

KG_API void KG_CALL KG_CurveTo(KGCanvas canvas,
                                double x1, double y1, double x2, double y2,
                                double x3, double y3) {
    if (canvas) ToCanvas(canvas)->CurveTo(x1, y1, x2, y2, x3, y3);
}

KG_API void KG_CALL KG_Arc(KGCanvas canvas, double cx, double cy, double r,
                           double startAngle, double endAngle) {
    if (canvas) ToCanvas(canvas)->Arc(cx, cy, r, startAngle, endAngle);
}

KG_API void KG_CALL KG_ClosePath(KGCanvas canvas) {
    if (canvas) ToCanvas(canvas)->ClosePath();
}

KG_API void KG_CALL KG_Stroke(KGCanvas canvas) {
    if (canvas) ToCanvas(canvas)->Stroke();
}

KG_API void KG_CALL KG_Fill(KGCanvas canvas) {
    if (canvas) ToCanvas(canvas)->Fill();
}

KG_API void KG_CALL KG_NewPath(KGCanvas canvas) {
    if (canvas) ToCanvas(canvas)->NewPath();
}

// =============================================================================
// 高级图形
// =============================================================================

KG_API void KG_CALL KG_DrawLine(KGCanvas canvas,
                                double x1, double y1, double x2, double y2) {
    if (canvas) ToCanvas(canvas)->DrawLine(x1, y1, x2, y2);
}

KG_API void KG_CALL KG_DrawRectangle(KGCanvas canvas,
                                     double x, double y, double w, double h,
                                     int fill) {
    if (canvas) ToCanvas(canvas)->DrawRectangle(x, y, w, h, fill != 0);
}

KG_API void KG_CALL KG_DrawCircle(KGCanvas canvas, double cx, double cy,
                                  double r, int fill) {
    if (canvas) ToCanvas(canvas)->DrawCircle(cx, cy, r, fill != 0);
}

KG_API void KG_CALL KG_DrawEllipse(KGCanvas canvas, double cx, double cy,
                                   double rx, double ry,
                                   double rotation, int fill) {
    if (canvas) ToCanvas(canvas)->DrawEllipse(cx, cy, rx, ry, rotation, fill != 0);
}

KG_API void KG_CALL KG_DrawArc(KGCanvas canvas, double cx, double cy,
                               double r, double startAngle, double endAngle) {
    if (canvas) ToCanvas(canvas)->DrawArc(cx, cy, r, startAngle, endAngle);
}

KG_API void KG_CALL KG_DrawPolyline(KGCanvas canvas,
                                     const double* points, int pointCount) {
    if (!canvas || !points || pointCount < 2) return;
    std::vector<Point> pts;
    pts.reserve(pointCount);
    for (int i = 0; i < pointCount; ++i) {
        pts.emplace_back(points[i * 2], points[i * 2 + 1]);
    }
    ToCanvas(canvas)->DrawPolyline(pts);
}

KG_API void KG_CALL KG_DrawPolygon(KGCanvas canvas,
                                    const double* points, int pointCount,
                                    int fill) {
    if (!canvas || !points || pointCount < 3) return;
    std::vector<Point> pts;
    pts.reserve(pointCount);
    for (int i = 0; i < pointCount; ++i) {
        pts.emplace_back(points[i * 2], points[i * 2 + 1]);
    }
    ToCanvas(canvas)->DrawPolygon(pts, fill != 0);
}

// =============================================================================
// 文字
// =============================================================================

KG_API void KG_CALL KG_SetFont(KGCanvas canvas, const char* family,
                               double size, int bold, int italic) {
    if (!canvas) return;
    const std::string fam = family ? family : "sans-serif";
    ToCanvas(canvas)->SetFont(FontStyle(fam, size, bold != 0, italic != 0));
}

KG_API void KG_CALL KG_SetFontSize(KGCanvas canvas, double size) {
    if (canvas) ToCanvas(canvas)->SetFontSize(size);
}

KG_API void KG_CALL KG_DrawText(KGCanvas canvas, double x, double y,
                                const char* text) {
    if (!canvas || !text) return;
    ToCanvas(canvas)->DrawTextUTF8(x, y, std::string(text));
}

KG_API double KG_CALL KG_GetTextWidth(KGCanvas canvas, const char* text) {
    if (!canvas || !text) return 0.0;
    return ToCanvas(canvas)->GetTextWidth(std::string(text));
}

// =============================================================================
// 保存
// =============================================================================

KG_API int KG_CALL KG_SaveToFile(KGCanvas canvas, const char* filepath, int format) {
    if (!canvas || !filepath) return 0;
    return ToCanvas(canvas)->Save(std::string(filepath), ToSaveFormat(format)) ? 1 : 0;
}