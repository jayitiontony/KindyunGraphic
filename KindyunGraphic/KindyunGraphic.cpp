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

// pixman_fini(): 释放 pixman 全局 implementation cache (sse2 / sswe3 /
// noop / fast_path / general, 每个 ~2KB). GCC/Clang 走 __attribute__((destructor))
// 自动释放; MSVC 不支持该属性, 必须由调用方在 main 末尾显式调用, 否则
// _CrtDumpMemoryLeaks 会报 5 条 {362..366} size=2064 的 leak.
#include <pixman.h>

#include <cstring>
#include <string>
#include <vector>

// 【中文注释】GBK ↔ UTF-8 转换走 libiconv 1.19 静态库路径 (libiconv_open / libiconv /
// libiconv_close), 静态嵌入到 KindyunGraphic.dll, 无运行时 DLL 依赖。
// 头文件 <iconv.h> 通过 KindyunGraphic.vcxproj 的 $(LIBICONV_INCLUDE_DIR) 解析到
// libiconv-1.19/include/iconv.h (libiconv_ 前缀 API 版本), 不会被 Windows SDK 自带的
// <iconv.h> 抢走 (linker 也找不到系统 iconv_open 等符号, 因为是 undefined)。
// 依据: GNU libiconv 1.19 的 POSIX iconv() 接口, libiconv_open 返回 libiconv_t
//       (typedef void*), 失败时返回 (libiconv_t)(-1) 并设置 errno (EINVAL / ENOENT)。
#include <iconv.h>

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

// -----------------------------------------------------------------------------
// 【中文注释】KG_DrawText 的输入字符串编码自适应 (GBK → UTF-8 自动转换)。
// -----------------------------------------------------------------------------
// 【中文注释】cairo 1.x 的 toy text API (cairo_show_text) 内部按 UTF-8 解析字节。
// 既有 KG_DrawText 调用方多为 Windows + 中文代码页 (CP936 / GBK), 传入的
// `const char* text` 经常是 GBK 字节序列, 直接送进 cairo 会渲染为乱码 ("锟斤拷")。
// 这里在 KG_DrawText 内部做启发式判别: 先严格按 RFC 3629 校验是否合法 UTF-8,
// 校验失败才假定为 GBK 并通过 Windows SDK 的 CP_ACP → UTF-8 路径转换。
// 整个逻辑只服务于 KG_DrawText, 不暴露给其它 API, 故放在 file-local 匿名
// namespace 内, 不污染头文件。
// -----------------------------------------------------------------------------

// 【中文注释】严格 RFC 3629 UTF-8 字节序列校验。
// 拒绝: overlong 编码、UTF-16 surrogate (U+D800..U+DFFF)、> U+10FFFF、
//       截断的多字节序列、孤立 10xxxxxx 续字节。
// 依据: RFC 3629 §4 "UTF-8 Definition" 与 Unicode 15.1 表 3.7
//       "Well-Formed UTF-8 Byte Sequences"。
inline bool IsValidUtf8(const char* p, size_t n) {
    const unsigned char* s = reinterpret_cast<const unsigned char*>(p);
    size_t i = 0;
    while (i < n) {
        unsigned char b0 = s[i];
        if (b0 < 0x80) {
            // 【中文注释】ASCII 单字节, 直接前进 1。
            i += 1;
        } else if ((b0 & 0xE0) == 0xC0) {
            // 【中文注释】2 字节序列 110xxxxx 10xxxxxx。b0 < 0xC2 即 overlong
            // (U+0000..U+007F 应当用单字节编码), 一律拒绝。
            if (b0 < 0xC2) return false;
            if (i + 1 >= n) return false;
            if ((s[i + 1] & 0xC0) != 0x80) return false;
            i += 2;
        } else if ((b0 & 0xF0) == 0xE0) {
            // 【中文注释】3 字节序列 1110xxxx 10xxxxxx 10xxxxxx。
            if (i + 2 >= n) return false;
            unsigned char b1 = s[i + 1];
            if ((b1 & 0xC0) != 0x80) return false;
            unsigned char b2 = s[i + 2];
            if ((b2 & 0xC0) != 0x80) return false;
            // 【中文注释】b0 == 0xE0 且 b1 < 0xA0 是 overlong (U+0800 以下用 2 字节即可);
            // b0 == 0xED 且 b1 >= 0xA0 落在 UTF-16 surrogate 区, UTF-8 中不可编码。
            if (b0 == 0xE0 && b1 < 0xA0) return false;
            if (b0 == 0xED && b1 >= 0xA0) return false;
            i += 3;
        } else if ((b0 & 0xF8) == 0xF0) {
            // 【中文注释】4 字节序列 11110xxx ... (U+10000..U+10FFFF)。
            if (b0 > 0xF4) return false;                  // > U+10FFFF
            if (i + 3 >= n) return false;
            unsigned char b1 = s[i + 1];
            if ((b1 & 0xC0) != 0x80) return false;
            if (b0 == 0xF0 && b1 < 0x90) return false;   // overlong
            if (b0 == 0xF4 && b1 >= 0x90) return false;   // > U+10FFFF
            unsigned char b2 = s[i + 2];
            if ((b2 & 0xC0) != 0x80) return false;
            unsigned char b3 = s[i + 3];
            if ((b3 & 0xC0) != 0x80) return false;
            i += 4;
        } else {
            // 【中文注释】10xxxxxx 单独出现, 或 0xF5..0xFF (永非法), 或 0xC0/0xC1 (overlong 2 字节)。
            return false;
        }
    }
    return true;
}

// 【中文注释】假定 `gbk` 是合法 GBK 字节序列, 转成 UTF-8。
// 关键: libiconv 编码名固定用 "GBK" (而不是 CP_ACP), 这样即使在英文 Windows 上
//       build / 跑, 也能按 GBK 解码中文——Windows MultiByteToWideChar(CP_ACP,...)
//       在英文系统会按 1252 (Latin-1) 解码, ° (U+00B0) 是单字节 0xB0, GBK 字节流
//       A1 E3 会被判成非法序列直接失败, ° 之类 Latin-1 Supplement 字符永远到不了
//       cairo。libiconv 的 "GBK" 编码名等价于 Windows CP936 (GBK = GB2312 + 扩展)。
// 依据:  GNU libiconv 1.19 /usr/share/libiconv/encoding.alias 里 GBK == CP936
//        (Microsoft 规范)。"//IGNORE" 后缀让 libiconv 遇到非法 GBK 字节时跳过
//        而不是返回 EILSEQ——这跟 Windows API 的 MB_ERR_INVALID_CHARS 语义相反,
//        我们的目标场景是"宁可丢字节也不要整个字符串渲染失败", //IGNORE 是合适
//        的选择。但代价是丢字节没有 caller 信号, 所以我们改用 //TRANSLIT 反而更
//        危险 (会把不能转换的字符替换成 '?' 一类乱码)。最终选 //IGNORE 后再用
//        utf8.empty() 作为整体失败的兜底 (下面注释解释)。
// 返回空串表示转换失败 (调用方将走 Latin-1 fallback)。
inline std::string GbkToUtf8(const char* gbk) {
    if (!gbk) return std::string();
    const size_t gbkLen = std::strlen(gbk);
    if (gbkLen == 0) return std::string();

    // 【中文注释】第 1 步: 打开 GBK → UTF-8 转换描述符。
    // 用 "//IGNORE" 后缀让 libiconv 在遇到非法 GBK 字节时跳过 (不抛 EILSEQ)。
    // 这样调用方的 KG_DrawText 在源串含半个汉字 / 随机二进制片段时, 仍能渲染出
    // 其余合法部分, 而不是整个串丢弃。
    // 失败返回 (libiconv_t)(-1), 实际 errno 是 EINVAL (输出编码不支持) / ENOENT
    // (输入编码不支持)。GBK + UTF-8 都是 libiconv 内置编码, 不会失败; 此分支
    // 仅作防御性兜底。
    libiconv_t cd = libiconv_open(/*tocode=*/"UTF-8//IGNORE", /*fromcode=*/"GBK");
    if (cd == reinterpret_cast<libiconv_t>(-1)) {
        return std::string();
    }

    // 【中文注释】第 2 步: 计算输出 buffer 最大可能大小, 一次性分配。
    // GBK 双字节 → UTF-8 3 字节 (BMP 汉字), 极端汉字 (扩展) 也 ≤ 3 字节 (U+FFFF 以下)。
    // 取 gbkLen * 3 + 1 (+1 给末尾 '\0' 留位), GBK 单字节 (ASCII 0x00-0x7F) 转
    // UTF-8 仍是 1 字节, 所以 * 3 永远不会欠; 实际 UTF-8 输出长度通过 libiconv
    // 调用后 outLeft 剩余 = 实际写入 = utf8.size() - outLeft 算出来。
    // 不预先写 '\0'——libiconv 不写终止符, 我们用 resize() 截断未用部分。
    const size_t utf8Cap = gbkLen * 3 + 1;
    std::string utf8(utf8Cap, '\0');

    // 【中文注释】第 3 步: 调用 libiconv() 一次性转换。
    // libiconv() 修改 *inbuf / *outbuf 推进指针, 同时 *inbytesleft / *outbytesleft
    // 减少剩余字节数。注意: libiconv 在 Windows 上 inbuf 是 const char** (iconv.h.in
    // ICONV_CONST=const), 但 libiconv() 实际不修改 *inbuf 指向的字节, 只是推进指针。
    // 这里 gbk 本来就是 const char*, 直接转成 const char* inBuf 不需要 const_cast。
    const char* inBuf = gbk;
    size_t inLeft = gbkLen;
    char* outBuf = &utf8[0];
    size_t outLeft = utf8Cap;

    const size_t res = libiconv(cd, &inBuf, &inLeft, &outBuf, &outLeft);
    libiconv_close(cd);

    if (res == static_cast<size_t>(-1)) {
        // 【中文注释】//IGNORE 后 EILSEQ 已被吞掉, 这里是 EINVAL (输入不完整, 例如
        // 末尾半个 GBK 双字节首字节)。视为失败, 让 EnsureUtf8ForCairo 走 Latin-1
        // fallback 路径。注意: GBK 字节不是合法 UTF-8, 所以 Latin-1 兜底是
        // 必走路径 (保留全部字节语义)。
        return std::string();
    }

    // 【中文注释】第 4 步: 截掉未使用的尾部, 得到精确 UTF-8 长度。
    // utf8.size() 是预分配大小 utf8Cap, outLeft 是 libiconv 没用完的空间大小。
    const size_t utf8Len = utf8Cap - outLeft;
    utf8.resize(utf8Len);
    return utf8;
}

// 【中文注释】Latin-1 (ISO-8859-1) → UTF-8 单字节映射。
// 用途: GBK 转换失败时的兜底, 处理"源字符串实际是单字节编码但被误送到这里"
// 的情况。最典型是英文 Windows 上的 Latin-1 Supplement 字符 (° © ® ¿ ¡ ñ ...),
// 它们在 CP1252 里都是单字节 (°=0xB0, ©=0xA9, ®=0xAE), GBK 转换时 MB_ERR_INVALID_CHARS
// 会直接拒绝, 必须靠这条路径救回。
// 依据: ISO-8859-1 是 256 个码位的单字节编码, U+0000..U+007F = ASCII,
// U+0080..U+00FF = Latin-1 Supplement。UTF-8 编码规则:
//   U+0080..U+07FF → 110xxxxx 10xxxxxx (2 字节)
// U+0080..U+00FF 完全落在这个范围内, 转换无歧义。
inline std::string Latin1ToUtf8(const char* input, size_t len) {
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        const unsigned char c = static_cast<unsigned char>(input[i]);
        if (c < 0x80) {
            out.push_back(static_cast<char>(c));
        } else {
            // 【中文注释】Latin-1 Supplement → 2 字节 UTF-8。
            // c 范围 0x80-0xFF (码点 U+0080-U+00FF), UTF-8 编码 = C2/C3 + 续字节。
            // (c >> 6) 在 c=0x80-0xBF 时是 0x02, c=0xC0-0xFF 时是 0x03,
            // 加上 0xC0 得到 0xC2 / 0xC3, 完全符合 RFC 3629。
            out.push_back(static_cast<char>(0xC0 | (c >> 6)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        }
    }
    return out;
}

// 【中文注释】KG_DrawText 编码自适应主入口。
// 决策树:
//   1. 空指针 → 返回空串 (KG_DrawText 已先做空指针检查, 这里仅防御性返回)。
//   2. 纯 ASCII (所有字节 < 0x80) → ASCII 是 UTF-8 的子集, 原样返回。
//   3. 含 0x80+ 字节 → 先尝试严格 UTF-8 解析; 解析通过则视为 UTF-8 原样返回。
//   4. UTF-8 校验失败 → 假定为 GBK (CP936), 调用 GbkToUtf8 转换。
//   5. GBK 转换失败 (例如字符串里有半个汉字 / CP936 不接受的字节) →
//      假定为 Latin-1 单字节编码, 直接按字节做 0x80-0xFF → U+0080-U+00FF
//      → UTF-8 映射。这条路径专门救英文 Windows / Latin-1 Supplement 字符
//      (例如 ° © ® ¿), 这类字符在 GBK 视角下是非法的, 但实际上是合法单字节字符。
//   6. (兜底) 异常路径, 实际不会到这里。
// 设计要点:
//   - 严格 UTF-8 校验放在前面, 保证中文 UTF-8 / emoji / 任意 Unicode 不会
//     被误判成 GBK (GBK 双字节首字节 0x81-0xFE 与 UTF-8 多字节首字节 0xC0-0xF7
//     有重叠, 不做校验直接靠"双字节范围"判别会产生大量假阳性)。
//   - GBK 转换失败绝对不能回退到原 GBK 字节, 因为 GBK 字节不是合法 UTF-8,
//     cairo 收到后会跳过 / 画 tofu。Latin-1 路径是更安全的回退。
inline std::string EnsureUtf8ForCairo(const char* input) {
    if (!input) return std::string();
    const size_t len = std::strlen(input);

    // 【中文注释】快速路径: 纯 ASCII 直接放行, 不进 UTF-8 严格校验。
    bool hasHighBit = false;
    for (size_t i = 0; i < len; ++i) {
        if (static_cast<unsigned char>(input[i]) >= 0x80) {
            hasHighBit = true;
            break;
        }
    }
    if (!hasHighBit) return std::string(input, len);

    // 【中文注释】含高位字节: 先尝试严格 UTF-8 解析。
    if (IsValidUtf8(input, len)) return std::string(input, len);

    // 【中文注释】UTF-8 校验失败 → 假定 GBK 转换 (固定 CP936, 跨 locale 一致)。
    std::string utf8 = GbkToUtf8(input);
    if (!utf8.empty()) return utf8;

    // 【中文注释】GBK 失败 → 假定 Latin-1 单字节编码, 处理 ° © ® 等字符。
    return Latin1ToUtf8(input, len);
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
    // 【中文注释】cairo 内部按 UTF-8 解析 text 字节。这里先用 EnsureUtf8ForCairo
    // 做编码自适应: 已是合法 UTF-8 (含纯 ASCII) 的字符串原样透传; 校验失败
    // 则视为 GBK (Windows CP_ACP / CP936) 并自动转 UTF-8。这样上游调用方
    // 不必关心源编码, 传入 GBK 字符串也能正确渲染中文。
    ToCanvas(canvas)->DrawTextUTF8(x, y, EnsureUtf8ForCairo(text));
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

// =============================================================================
// 调试: 释放 cairo 进程内所有静态缓存
// =============================================================================
KG_API void KG_CALL KG_DebugResetStaticData(void) {
    // 必须在所有 cairo_t / cairo_surface_t 都销毁后调用,
    // 否则会触发 cairo 内部 hash-table 销毁时的 "non-empty" 断言。
    cairo_debug_reset_static_data();

    // 进一步: 释放 pixman 全局 implementation cache.
    // 5 个 pixman_implementation_t (sse2 / sswe3 / noop / fast_path / general)
    // 总共约 10KB, 在 MSVC 下没有 __attribute__((destructor)) 自动释放,
    // 必须显式调用 pixman_fini() 释放. 否则 _CrtDumpMemoryLeaks 报
    // {362..366} size=2064 共 5 条 leak.
    // 顺序: cairo 静态 cache 先清, pixman cache 后清 (避免 cairo 内部仍持有
    //       pixman 实现指针时去 free pixman).
    pixman_fini();
}