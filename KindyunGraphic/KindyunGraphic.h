// =============================================================================
// KindyunGraphic.h
//
// KindyunGraphic.dll 公开 C 接口 (C 风格, extern "C", 方便 MFC / C / 其他语言调用)
//
// 概述:
//   本 DLL 把 memgc::CairoCanvas (C++ 矢量绘图封装) 的核心能力以纯 C API 形式
//   导出, 调用方不需要懂 C++, 只需要:
//     1) KG_Create / KG_CreateFromBuffer  建一个画布 (或绑用户内存);
//     2) 一系列 KG_DrawXxx 绘制;
//     3) KG_GetData + KG_Flush 拿像素 + 刷新到显示设备 (Win32 GDI 用 SetDIBitsToDevice);
//     4) KG_Destroy 释放资源;
//
// 内存布局:
//   Cairo ARGB32 在 Windows x86/x64 上是 BGRA 字节序 (B 在低地址), 与
//   Win32 BITMAPINFOHEADER / Direct2D / WPF 完全一致, 零拷贝对接。
//
// 编译:
//   cairo2d.lib (debug) / cairo2.lib (release) 作为静态库被链入本 DLL, 运行时
//   只需 KindyunGraphic.dll 一个文件, 不再依赖 cairo2.dll / libpng16.dll 等。
//
// =============================================================================

#ifndef KINDYUN_GRAPHIC_H
#define KINDYUN_GRAPHIC_H

#include <stdint.h>

// 编译器无关导出宏
#if defined(_WIN32)
    #if defined(KINDYUN_GRAPHIC_BUILD)
        #define KG_API __declspec(dllexport)
    #else
        #define KG_API __declspec(dllimport)
    #endif
    #define KG_CALL __stdcall
#else
    #define KG_API    __attribute__((visibility("default")))
    #define KG_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// 画布管理
// =============================================================================

/// @brief 不透明画布句柄, 由 KG_Create / KG_CreateFromBuffer 返回。
typedef void* KGCanvas;

/// @brief 像素格式。
typedef enum KGPixelFormat {
    KG_FMT_ARGB32 = 0,    ///< 32 位带 alpha; Windows 上是 BGRA 字节序
    KG_FMT_RGB24  = 1,    ///< 32 位, alpha 忽略
    KG_FMT_A8     = 2,    ///< 8 位 alpha 单通道
    KG_FMT_RGB16_565 = 3  ///< 16 位
} KGPixelFormat;

/// @brief 线帽样式。
typedef enum KGLineCap {
    KG_CAP_BUTT   = 0,
    KG_CAP_ROUND  = 1,
    KG_CAP_SQUARE = 2
} KGLineCap;

/// @brief 线段连接样式。
typedef enum KGLineJoin {
    KG_JOIN_MITER = 0,
    KG_JOIN_ROUND = 1,
    KG_JOIN_BEVEL = 2
} KGLineJoin;

/// @brief 文件保存格式。
typedef enum KGSaveFormat {
    KG_SAVE_PNG = 0,
    KG_SAVE_SVG = 1,
    KG_SAVE_PDF = 2,
    KG_SAVE_PS  = 3
} KGSaveFormat;

// ---------------------------------------------------------------------------
// 画布生命周期
// ---------------------------------------------------------------------------

/// @brief 创建一个内部分配 buffer 的画布 (ARGB32 格式, 默认开双缓冲)。
///        **推荐用法**: 内部开双缓冲, 每帧绘制后调 KG_Flush 即自动 swap,
///        外部分别通过 KG_GetData 取完整一帧, 不会有撕裂 / 闪烁。
/// @param width, height 画布尺寸 (像素)
/// @return 画布句柄, 失败返回 NULL
KG_API KGCanvas KG_CALL KG_Create(int width, int height);

/// @brief 零拷贝创建: 绑定到调用方提供的 RGBA 内存, Cairo 直接在上面绘制。
///        内存由调用方管理, 析构时**不**释放。**默认不**开双缓冲
///        (用户自己管理 buffer 同步)。
/// @param width, height 画布尺寸 (像素)
/// @param buffer 调用方提供的内存, 大小至少 stride * height 字节
/// @param stride 每一行的字节数 (>= 4*width, ARGB32)
/// @param fmt 像素格式
/// @return 画布句柄, 失败返回 NULL
KG_API KGCanvas KG_CALL KG_CreateFromBuffer(int width, int height,
                                             unsigned char* buffer, int stride,
                                             int fmt);

/// @brief 销毁画布 (不释放调用方提供的 buffer)。
KG_API void KG_CALL KG_Destroy(KGCanvas canvas);

/// @brief 手动触发 swap (双缓冲模式下)。
///        默认情况下 KG_Flush 会自动 swap; 如果想更精确控制 swap 时机,
///        可先调 KG_SetAutoSwapOnFlush(0) 关闭自动 swap, 然后在合适的时机
///        (例如等待垂直同步信号) 调本函数。
KG_API void KG_CALL KG_SwapBuffers(KGCanvas canvas);

/// @brief 设置 KG_Flush 是否自动 swap (默认 1)。
KG_API void KG_CALL KG_SetAutoSwapOnFlush(KGCanvas canvas, int enabled);

/// @brief 查询画布是否启用了双缓冲。返回 1/0。
KG_API int KG_CALL KG_IsDoubleBuffered(KGCanvas canvas);

// ---------------------------------------------------------------------------
// 元信息
// ---------------------------------------------------------------------------

KG_API int    KG_CALL KG_GetWidth(KGCanvas canvas);
KG_API int    KG_CALL KG_GetHeight(KGCanvas canvas);
KG_API int    KG_CALL KG_GetStride(KGCanvas canvas);
KG_API int    KG_CALL KG_IsLiveMode(KGCanvas canvas);   ///< 0=普通, 1=零拷贝

/// @brief 拿到底层像素指针 (零拷贝模式下就是用户 buffer, 普通模式下是内部 buffer)。
KG_API unsigned char* KG_CALL KG_GetData(KGCanvas canvas);

/// @brief 强制把 Cairo 内部缓存写回底层 buffer。
KG_API void KG_CALL KG_Flush(KGCanvas canvas);

/// @brief 把整张画布标记为"已修改"。
KG_API void KG_CALL KG_MarkDirty(KGCanvas canvas);

/// @brief 标脏一个矩形区域。
KG_API void KG_CALL KG_MarkDirtyRect(KGCanvas canvas, int x, int y, int w, int h);

// ---------------------------------------------------------------------------
// 状态设置
// ---------------------------------------------------------------------------

KG_API void KG_CALL KG_SetSourceColor(KGCanvas canvas,
                                       double r, double g, double b, double a);
KG_API void KG_CALL KG_SetSourceRGB(KGCanvas canvas,
                                     uint8_t r, uint8_t g, uint8_t b, uint8_t a);
KG_API void KG_CALL KG_SetLineWidth(KGCanvas canvas, double width);
KG_API void KG_CALL KG_SetLineCap(KGCanvas canvas, int cap);
KG_API void KG_CALL KG_SetLineJoin(KGCanvas canvas, int join);

/// @brief 设置虚线模式。dashes 是 double 数组; dashes==NULL 或 count==0 表示实线。
KG_API void KG_CALL KG_SetDash(KGCanvas canvas,
                                const double* dashes, int count, double offset);
KG_API void KG_CALL KG_ClearDash(KGCanvas canvas);

// ---------------------------------------------------------------------------
// 画布整体操作
// ---------------------------------------------------------------------------

/// @brief 用指定 RGBA 颜色清空整张画布。
KG_API void KG_CALL KG_Clear(KGCanvas canvas,
                             double r, double g, double b, double a);

KG_API void KG_CALL KG_Translate(KGCanvas canvas, double dx, double dy);
KG_API void KG_CALL KG_Scale(KGCanvas canvas, double sx, double sy);
KG_API void KG_CALL KG_Rotate(KGCanvas canvas, double radians);
KG_API void KG_CALL KG_Save(KGCanvas canvas);
KG_API void KG_CALL KG_Restore(KGCanvas canvas);

// ---------------------------------------------------------------------------
// 基础路径
// ---------------------------------------------------------------------------

KG_API void KG_CALL KG_MoveTo(KGCanvas canvas, double x, double y);
KG_API void KG_CALL KG_LineTo(KGCanvas canvas, double x, double y);
KG_API void KG_CALL KG_CurveTo(KGCanvas canvas,
                                double x1, double y1, double x2, double y2,
                                double x3, double y3);
KG_API void KG_CALL KG_Arc(KGCanvas canvas, double cx, double cy, double r,
                           double startAngle, double endAngle);
KG_API void KG_CALL KG_ClosePath(KGCanvas canvas);
KG_API void KG_CALL KG_Stroke(KGCanvas canvas);
KG_API void KG_CALL KG_Fill(KGCanvas canvas);
KG_API void KG_CALL KG_NewPath(KGCanvas canvas);

// ---------------------------------------------------------------------------
// 高级图形
// ---------------------------------------------------------------------------

KG_API void KG_CALL KG_DrawLine(KGCanvas canvas,
                                double x1, double y1, double x2, double y2);
KG_API void KG_CALL KG_DrawRectangle(KGCanvas canvas,
                                     double x, double y, double w, double h,
                                     int fill);
KG_API void KG_CALL KG_DrawCircle(KGCanvas canvas, double cx, double cy,
                                  double r, int fill);
KG_API void KG_CALL KG_DrawEllipse(KGCanvas canvas, double cx, double cy,
                                   double rx, double ry,
                                   double rotation, int fill);
KG_API void KG_CALL KG_DrawArc(KGCanvas canvas, double cx, double cy,
                               double r, double startAngle, double endAngle);
KG_API void KG_CALL KG_DrawPolyline(KGCanvas canvas,
                                     const double* points, int pointCount);
KG_API void KG_CALL KG_DrawPolygon(KGCanvas canvas,
                                    const double* points, int pointCount,
                                    int fill);

// ---------------------------------------------------------------------------
// 文字
// ---------------------------------------------------------------------------

/// @brief 设置字体 (族名 + 字号 + 粗/斜)。
KG_API void KG_CALL KG_SetFont(KGCanvas canvas, const char* family,
                               double size, int bold, int italic);
KG_API void KG_CALL KG_SetFontSize(KGCanvas canvas, double size);

/// @brief 绘制文字 (UTF-8 编码, 支持中文)。
KG_API void KG_CALL KG_DrawText(KGCanvas canvas, double x, double y,
                                const char* text);
KG_API double KG_CALL KG_GetTextWidth(KGCanvas canvas, const char* text);

// ---------------------------------------------------------------------------
// 保存
// ---------------------------------------------------------------------------

/// @brief 保存为文件。format: 0=PNG, 1=SVG, 2=PDF, 3=PS
KG_API int KG_CALL KG_SaveToFile(KGCanvas canvas, const char* filepath, int format);

#ifdef __cplusplus
} // extern "C"
#endif


// =============================================================================
// 函数指针 typedef (方便 GetProcAddress 动态加载)
// =============================================================================
// 注: 这些 typedef 必须放在 KGCanvas / 函数声明之后, 因为它们引用了
//     KGCanvas / 函数原型。

typedef KGCanvas        (KG_CALL *PFN_KG_Create)(int width, int height);
typedef KGCanvas        (KG_CALL *PFN_KG_CreateFromBuffer)(int width, int height,
                                                           unsigned char* buffer, int stride,
                                                           int fmt);
typedef void            (KG_CALL *PFN_KG_Destroy)(KGCanvas canvas);
typedef void            (KG_CALL *PFN_KG_SwapBuffers)(KGCanvas canvas);
typedef void            (KG_CALL *PFN_KG_SetAutoSwapOnFlush)(KGCanvas canvas, int enabled);
typedef int             (KG_CALL *PFN_KG_IsDoubleBuffered)(KGCanvas canvas);
typedef int             (KG_CALL *PFN_KG_GetWidth)(KGCanvas canvas);
typedef int             (KG_CALL *PFN_KG_GetHeight)(KGCanvas canvas);
typedef int             (KG_CALL *PFN_KG_GetStride)(KGCanvas canvas);
typedef int             (KG_CALL *PFN_KG_IsLiveMode)(KGCanvas canvas);
typedef unsigned char*  (KG_CALL *PFN_KG_GetData)(KGCanvas canvas);
typedef void            (KG_CALL *PFN_KG_Flush)(KGCanvas canvas);
typedef void            (KG_CALL *PFN_KG_MarkDirty)(KGCanvas canvas);
typedef void            (KG_CALL *PFN_KG_MarkDirtyRect)(KGCanvas canvas, int x, int y, int w, int h);
typedef void            (KG_CALL *PFN_KG_SetSourceColor)(KGCanvas canvas, double r, double g, double b, double a);
typedef void            (KG_CALL *PFN_KG_SetSourceRGB)(KGCanvas canvas, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
typedef void            (KG_CALL *PFN_KG_SetLineWidth)(KGCanvas canvas, double width);
typedef void            (KG_CALL *PFN_KG_SetLineCap)(KGCanvas canvas, int cap);
typedef void            (KG_CALL *PFN_KG_SetLineJoin)(KGCanvas canvas, int join);
typedef void            (KG_CALL *PFN_KG_SetDash)(KGCanvas canvas, const double* dashes, int count, double offset);
typedef void            (KG_CALL *PFN_KG_ClearDash)(KGCanvas canvas);
typedef void            (KG_CALL *PFN_KG_Clear)(KGCanvas canvas, double r, double g, double b, double a);
typedef void            (KG_CALL *PFN_KG_Translate)(KGCanvas canvas, double dx, double dy);
typedef void            (KG_CALL *PFN_KG_Scale)(KGCanvas canvas, double sx, double sy);
typedef void            (KG_CALL *PFN_KG_Rotate)(KGCanvas canvas, double radians);
typedef void            (KG_CALL *PFN_KG_Save)(KGCanvas canvas);
typedef void            (KG_CALL *PFN_KG_Restore)(KGCanvas canvas);
typedef void            (KG_CALL *PFN_KG_MoveTo)(KGCanvas canvas, double x, double y);
typedef void            (KG_CALL *PFN_KG_LineTo)(KGCanvas canvas, double x, double y);
typedef void            (KG_CALL *PFN_KG_CurveTo)(KGCanvas canvas, double x1, double y1, double x2, double y2, double x3, double y3);
typedef void            (KG_CALL *PFN_KG_Arc)(KGCanvas canvas, double cx, double cy, double r, double startAngle, double endAngle);
typedef void            (KG_CALL *PFN_KG_ClosePath)(KGCanvas canvas);
typedef void            (KG_CALL *PFN_KG_Stroke)(KGCanvas canvas);
typedef void            (KG_CALL *PFN_KG_Fill)(KGCanvas canvas);
typedef void            (KG_CALL *PFN_KG_NewPath)(KGCanvas canvas);
typedef void            (KG_CALL *PFN_KG_DrawLine)(KGCanvas canvas, double x1, double y1, double x2, double y2);
typedef void            (KG_CALL *PFN_KG_DrawRectangle)(KGCanvas canvas, double x, double y, double w, double h, int fill);
typedef void            (KG_CALL *PFN_KG_DrawCircle)(KGCanvas canvas, double cx, double cy, double r, int fill);
typedef void            (KG_CALL *PFN_KG_DrawEllipse)(KGCanvas canvas, double cx, double cy, double rx, double ry, double rotation, int fill);
typedef void            (KG_CALL *PFN_KG_DrawArc)(KGCanvas canvas, double cx, double cy, double r, double startAngle, double endAngle);
typedef void            (KG_CALL *PFN_KG_DrawPolyline)(KGCanvas canvas, const double* points, int pointCount);
typedef void            (KG_CALL *PFN_KG_DrawPolygon)(KGCanvas canvas, const double* points, int pointCount, int fill);
typedef void            (KG_CALL *PFN_KG_SetFont)(KGCanvas canvas, const char* family, double size, int bold, int italic);
typedef void            (KG_CALL *PFN_KG_SetFontSize)(KGCanvas canvas, double size);
typedef void            (KG_CALL *PFN_KG_DrawText)(KGCanvas canvas, double x, double y, const char* text);
typedef double          (KG_CALL *PFN_KG_GetTextWidth)(KGCanvas canvas, const char* text);
typedef int             (KG_CALL *PFN_KG_SaveToFile)(KGCanvas canvas, const char* filepath, int format);

#endif // KINDYUN_GRAPHIC_H