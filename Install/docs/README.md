# KindyunGraphic 调用说明书

**Version 1.0** | 基于 Cairo 1.18.4 | 纯 C 接口 DLL

---

## 1. 简介

`KindyunGraphic.dll` 是基于 [Cairo 1.18.4](https://www.cairographics.org/) 矢量绘图库封装的纯 C 接口 DLL。

- **零外部依赖**：cairo / libpng / pixman / freetype / zlib 全部静态链入 DLL，运行时只需 `KindyunGraphic.dll` 一个文件（外加 Windows 系统 DLL 与 MSVC 运行时）
- **内置双缓冲**：默认开启，每帧绘制后自动 swap，外部读到的永远是一帧完整画面，**不会闪烁 / 撕裂**
- **像素格式**：`CAIRO_FORMAT_ARGB32`，Windows x86/x64 上是 **BGRA 字节序**（与 Win32 GDI / Direct2D / WPF 完全一致，零拷贝对接）
- **C 接口**：`extern "C"` 风格，无 C++ 依赖，**可被 C / C++ / MFC / Qt / .NET / Rust / Go 等任意语言调用**
- **两种使用模式**：
  - **托管模式**：`KG_Create()` → DLL 内部分配两个 buffer，最简单
  - **零拷贝模式**：`KG_CreateFromBuffer()` → 绑定调用方自己的 RGBA buffer，**真正零内存拷贝**

---

## 2. 目录结构（编译完成后）

```
Install/
├── README.md                         # 本文件
├── include/
│   └── KindyunGraphic.h              # 唯一需要 include 的头文件
├── lib/
│   ├── Debug/
│   │   └── KindyunGraphic.lib        # Debug 版导入库
│   └── Release/
│       └── KindyunGraphic.lib        # Release 版导入库
├── bin/
│   ├── Debug/
│   │   ├── KindyunGraphic.dll        # Debug 版 DLL (含调试符号)
│   │   └── KindyunGraphicMfc.exe     # MFC 示例程序 (Debug)
│   └── Release/
│       ├── KindyunGraphic.dll        # Release 版 DLL
│       └── KindyunGraphicMfc.exe     # MFC 示例程序 (Release)
├── docs/
│   └── README.md                     # 调用说明 (本文件)
└── samples/
    └── KindyunGraphicMfc/            # MFC 示例源码
        ├── KindyunGraphicMfc.cpp
        ├── MainFrm.cpp
        ├── MainFrm.h
        ├── stdafx.cpp
        ├── stdafx.h
        ├── targetver.h
        ├── resource.h
        ├── KindyunGraphicMfc.rc
        ├── KindyunGraphicMfc.ico
        ├── KindyunGraphicMfc.vcxproj
        └── KindyunGraphicMfc.vcxproj.filters
```

> 编译方案之后，所有 DLL / LIB / EXE / 头文件 / 示例源码 / 文档都会自动部署到 `Install/` 下。第三方项目只需要把 `Install\include`、`Install\lib\Release`、`Install\bin\Release` 三个目录加入工程即可。

---

## 3. 部署到自己的项目

### 3.1 在自己的项目里加入链接

把下面三个目录加到你的 Visual Studio 项目：

| 类型 | 路径 |
|------|------|
| **头文件** | `Install\include` |
| **导入库** | `Install\lib\Release`（或 `Install\lib\Debug`） |
| **运行时 DLL** | `Install\bin\Release`（或 `Install\bin\Debug`） |

在 vcxproj 里配置（也可用 Visual Studio 图形界面）：

```xml
<PropertyGroup>
  <IncludePath>$(IncludePath);Install\include</IncludePath>
</PropertyGroup>

<ItemDefinitionGroup>
  <Link>
    <AdditionalLibraryDirectories>Install\lib\Release;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>
    <AdditionalDependencies>KindyunGraphic.lib;%(AdditionalDependencies)</AdditionalDependencies>
  </Link>
</ItemDefinitionGroup>
```

运行时把 `KindyunGraphic.dll` 放到 `.exe` 同目录（或者加入 `PATH`）。

### 3.2 你的代码

```cpp
#include "KindyunGraphic.h"   // 唯一需要 include 的头

int main() {
    // 1. 创建画布 (800x600, 默认开双缓冲, DLL 内部分配 2 个 buffer)
    KGCanvas canvas = KG_Create(800, 600);
    if (!canvas) { /* 错误处理 */ }

    // 2. 绘制一帧
    KG_Clear(canvas, 0.1, 0.1, 0.2, 1.0);                     // 深色背景
    KG_SetSourceColor(canvas, 1.0, 0.6, 0.2, 1.0);            // 橙色
    KG_SetLineWidth(canvas, 3.0);
    KG_DrawCircle(canvas, 400, 300, 80, 1);                   // 填充圆

    KG_SetSourceColor(canvas, 1.0, 1.0, 1.0, 1.0);            // 白色文字
    KG_SetFont(canvas, "Microsoft YaHei", 24, 1, 0);
    KG_DrawText(canvas, 30, 50, "Hello, KindyunGraphic!");

    // 3. 提交本帧 (内部自动 swap, 外部 GetData 即可拿到完整画面)
    KG_Flush(canvas);

    // 4. 取本帧数据 (例如给 GDI / Direct2D / OpenGL 上传)
    unsigned char* pixels = KG_GetData(canvas);
    int stride = KG_GetStride(canvas);
    // ... 把 pixels + stride 喂给 GDI StretchDIBits / D2D / SDL ...

    // 5. 销毁
    KG_Destroy(canvas);
    return 0;
}
```

---

## 4. 关键 API 速查

### 4.1 画布生命周期

| 函数 | 用途 |
|------|------|
| `KGCanvas KG_Create(int w, int h)` | 托管模式创建，**默认开双缓冲**。DLL 内部 2 个 buffer，最简用法 |
| `KGCanvas KG_CreateFromBuffer(int w, int h, unsigned char* buf, int stride, int fmt)` | **零拷贝模式**，绑定调用方自己的 RGBA buffer。适合你已经管好内存（Direct2D bitmap / SDL texture / 自定义内存池） |
| `void KG_Destroy(KGCanvas)` | 销毁画布（不释放调用方提供的 buffer） |
| `int KG_GetWidth(KGCanvas)` / `KG_GetHeight` / `KG_GetStride` | 元信息 |
| `int KG_IsLiveMode(KGCanvas)` | 1 = 零拷贝模式，0 = 托管模式 |
| `int KG_IsDoubleBuffered(KGCanvas)` | 1 = 当前启用了双缓冲 |

### 4.2 取图（关键！）

| 函数 | 用途 |
|------|------|
| `unsigned char* KG_GetData(KGCanvas)` | **拿 read buffer 指针**。BGRA 字节序，与 Win32 DIB / D2D / WPF 兼容 |
| `void KG_Flush(KGCanvas)` | **每帧结束必须调用**。把内部 Cairo 缓存写回 buffer + **自动 swap**（双缓冲模式）|
| `void KG_SwapBuffers(KGCanvas)` | 手动 swap（关闭 auto-swap 后才有意义）|
| `void KG_SetAutoSwapOnFlush(KGCanvas, int enabled)` | 设置 KG_Flush 是否自动 swap（默认 1）|
| `void KG_MarkDirty(KGCanvas)` / `KG_MarkDirtyRect(KGCanvas, x, y, w, h)` | 标脏（让 Cairo 跳过内部缓存）|

### 4.3 状态

| 函数 | 用途 |
|------|------|
| `KG_SetSourceColor(canvas, r, g, b, a)` | 设置当前颜色（实色或带 alpha），r/g/b/a ∈ [0,1] |
| `KG_SetSourceRGB(canvas, r, g, b, a)` | 同上，r/g/b/a ∈ [0,255] |
| `KG_SetLineWidth(canvas, width)` | 线宽（用户空间单位） |
| `KG_SetLineCap(canvas, cap)` | 0=Butt, 1=Round, 2=Square |
| `KG_SetLineJoin(canvas, join)` | 0=Miter, 1=Round, 2=Bevel |
| `KG_SetDash(canvas, dashes, count, offset)` | 虚线模式，dashes 是 {实, 虚, 实, 虚...} 长度 |
| `KG_ClearDash(canvas)` | 取消虚线 |
| `KG_Clear(canvas, r, g, b, a)` | 用指定颜色清空整张画布 |
| `KG_Translate/Scale/Rotate/Save/Restore` | 坐标变换和 gstate |

### 4.4 高级图形

| 函数 | 用途 |
|------|------|
| `KG_DrawLine(x1, y1, x2, y2)` | 直线 |
| `KG_DrawRectangle(x, y, w, h, fill)` | 矩形（fill: 0=描边, 1=填充）|
| `KG_DrawCircle(cx, cy, r, fill)` | 圆 |
| `KG_DrawEllipse(cx, cy, rx, ry, rotation, fill)` | 椭圆（带旋转） |
| `KG_DrawArc(cx, cy, r, startAngle, endAngle)` | 圆弧（起止角，弧度） |
| `KG_DrawPolyline(points, pointCount)` | 描迹线（多段连续折线），points={x0,y0,x1,y1,...} |
| `KG_DrawPolygon(points, pointCount, fill)` | 描迹闭合多边形 |

### 4.5 文字（支持中文）

```cpp
KG_SetFont(canvas, "Microsoft YaHei", 24, /*bold*/1, /*italic*/0);
KG_SetSourceColor(canvas, 1.0, 1.0, 1.0, 1.0);
KG_DrawText(canvas, x, y, "你好, cairo! 矢量绘图 ✓");
```

| 函数 | 用途 |
|------|------|
| `KG_SetFont(canvas, family, size, bold, italic)` | 设置字体。family 传系统字体名 |
| `KG_DrawText(canvas, x, y, text)` | 绘制文字，**(x, y) 是基线起点** |
| `KG_GetTextWidth(canvas, text)` | 文字渲染宽度 |

字体名用 Windows 上已安装的字体（运行 `KindyunGraphicMfc.exe` 试一下效果）。常用：

```
Microsoft YaHei        微软雅黑（中文）
SimSun                  宋体
SimHei                  黑体
Arial                   西文
Consolas                等宽
Times New Roman         衬线
```

### 4.6 保存文件

| 函数 | 用途 |
|------|------|
| `int KG_SaveToFile(canvas, "out.png", 0)` | 0=PNG, 1=SVG, 2=PDF, 3=PS。返回 1=成功 0=失败 |

> **注意**：托管双缓冲模式下，SVG/PDF/PS 导出是**位图嵌入**（不是真矢量）。
> 如要真矢量，请用 `KG_CreateFromBuffer` 零拷贝模式（不启用双缓冲）。

---

## 5. 性能与帧率参考

测试环境：Debug 编译，纯 CPU 渲染，800×600 画布，负载 = 移动圆 + 6 个旋转方块 + 60 段螺旋 + 两行文字。

| 分辨率 | 平均帧率 | 平均帧耗时 |
|---|---|---|
| 800×600 | ~190 FPS | 5.4 ms |
| 1920×1080 | ~140 FPS | 7.2 ms |
| 2560×1440 | ~115 FPS | 8.6 ms |

**Release 编译通常比 Debug 再快 30-50%。** 2K 也能稳 100+ FPS，**远超 60 / 120 / 144 Hz 显示器刷新率**。

每帧 swap 的开销：rebuild cairo_t 大约 **< 0.5 ms**，可忽略。

---

## 6. 典型场景

### 6.1 Win32 GDI 显示（最简单，MFC 示例程序用的就是这个）

```cpp
// 启动时
KGCanvas canvas = KG_Create(800, 600);
unsigned char* pixels = KG_GetData(canvas);
int stride = KG_GetStride(canvas);

// 绘制一帧
KG_Clear(canvas, 0, 0, 0, 1);
KG_SetSourceColor(canvas, 1, 1, 0, 1);
KG_DrawCircle(canvas, 400, 300, 100, 1);
KG_Flush(canvas);          // 内部 swap

// 显示 (OnPaint / WM_PAINT)
BITMAPINFO bmi = {};
bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
bmi.bmiHeader.biWidth = 800;
bmi.bmiHeader.biHeight = -600;          // 顶向下 DIB
bmi.bmiHeader.biPlanes = 1;
bmi.bmiHeader.biBitCount = 32;
bmi.bmiHeader.biCompression = BI_RGB;

CRect rc;
GetClientRect(hWnd, &rc);
::StretchDIBits(hdc, 0, 0, rc.Width(), rc.Height(),
                0, 0, 800, 600, pixels, &bmi, DIB_RGB_COLORS, SRCCOPY);
```

### 6.2 Direct2D 显示（GPU 加速，最佳性能）

```cpp
// 启动
KGCanvas canvas = KG_Create(1920, 1080);
D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(D2D1::PixelFormat(
    DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
ID2D1Bitmap* d2dBitmap = nullptr;
m_d2dContext->CreateBitmap(D2D1::SizeU(1920, 1080), nullptr, 0, props, &d2dBitmap);

// 绘制
KG_Flush(canvas);
unsigned char* pixels = KG_GetData(canvas);
d2dBitmap->CopyFromMemory(nullptr, pixels, KG_GetStride(canvas));
m_d2dContext->DrawBitmap(d2dBitmap, D2D1::RectF(0, 0, w, h));
```

### 6.3 SDL / SDL2 纹理上传

```cpp
KGCanvas canvas = KG_Create(1280, 720);
SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                      SDL_TEXTUREACCESS_STREAMING, 1280, 720);

// 绘制 + 显示
KG_Flush(canvas);
unsigned char* pixels = KG_GetData(canvas);
SDL_UpdateTexture(tex, nullptr, pixels, KG_GetStride(canvas));
SDL_RenderCopy(renderer, tex, nullptr, nullptr);
SDL_RenderPresent(renderer);
```

### 6.4 零拷贝模式（自己管内存）

适合：你已经有 Direct2D bitmap / SDL texture / 显存映射 / 自定义内存池。

```cpp
// 启动
std::vector<unsigned char> buf(800 * 600 * 4);  // 你的 buffer
KGCanvas canvas = KG_CreateFromBuffer(800, 600, buf.data(),
                                       800 * 4, KG_FMT_ARGB32);
// 此后不需要再 KG_Flush (用户直接读 buf 即可), 但调了也没事
```

---

## 7. 字节序

`CAIRO_FORMAT_ARGB32` 在 Windows x86/x64 上是 **BGRA 字节序**（B 在低地址）。

| 内存地址 | 字节 |
|---|---|
| buf[0] | B |
| buf[1] | G |
| buf[2] | R |
| buf[3] | A |

这与以下框架字节序**一致**：
- Win32 `BITMAPINFOHEADER` / `CreateDIBSection`
- Direct2D `DXGI_FORMAT_B8G8R8A8_*`
- WPF / WinUI
- GDI+ `PixelFormat32bppARGB`（注意名字虽然叫 ARGB 但实际内存里也是 BGRA）

所以这些框架对接都是**零拷贝**。

如果你的 buffer 是严格 R,G,B,A 顺序（OpenGL ES 部分平台、Cairo 旧文档示例），需要手动 swap 一道，或用 `KG_SaveToFile` 之类走 Cairo 的转换。

---

## 8. 常见问题

### Q1: 编译找不到 `KindyunGraphic.h`
A: 把 `Install\include` 加入 VC++ 头文件路径（项目属性 → C/C++ → 常规 → 附加包含目录）。

### Q2: 链接报 LNK2019 找不到 `KG_*` 符号
A: 把 `Install\lib\Release`（或 `Debug`）加入库目录，链接器输入加 `KindyunGraphic.lib`。

### Q3: 运行时弹"找不到 KindyunGraphic.dll"
A: 把 `Install\bin\Release\KindyunGraphic.dll` 复制到 `.exe` 同目录。

### Q4: 文字显示成方块
A: 系统找不到指定字体。在 cmd 跑 `dir C:\Windows\Fonts\*.ttf` 看有哪些字体，把 `KG_SetFont` 第一个参数换成确实存在的字体名。

### Q5: 中文不显示
A: cairo 1.18 的 toy text API 按 UTF-8 解析字节。代码里**必须**传 UTF-8 字符串，不能是 GBK 编码：

```cpp
// 正确: UTF-8 源文件, 中文字面量
KG_DrawText(canvas, x, y, "中文测试");

// 错误: GBK 源文件
KG_DrawText(canvas, x, y, "中文测试");
```

MSVC 编译加 `/utf-8` 即可保证源文件是 UTF-8 编码（见本项目示例 vcxproj）。

### Q6: 实时绘制会闪烁
A: 用托管模式 `KG_Create`（默认开双缓冲），并在每帧绘制后调 `KG_Flush`。外部分别通过 `KG_GetData` 拿数据时就永远是一帧完整画面，**不会撕裂 / 闪烁**。

### Q7: 想要真矢量 SVG 导出
A: 用零拷贝模式 `KG_CreateFromBuffer`，不要开双缓冲。`KG_SaveToFile(..., 1)` 导出 SVG 时是真正的 `<path>` / `<circle>` 等元素（可被 Illustrator / Inkscape 二次编辑缩放）。

### Q8: 进程退出时 `_CrtDumpMemoryLeaks()` 报几百 KB leak?
A: 这是 cairo + pixman 的进程级 lazy cache, cairo 1.18 有意不在进程退出时自动释放. 本项目已通过下述修改达到 **0 leak**:

**改动 1 — pixman**: pixman 默认用 `__attribute__((destructor))` 在进程结束时释放 5 个 `pixman_implementation_t` (general / fast_path / noop / sse2 / ssse3), 但 **MSVC 不支持**这个 GCC 扩展. 修复:

```c
// pixman-0.46.4/sources/pixman/pixman.c 末尾
static void __cdecl _pixman_fini_impl(void);  // MSVC CRT 析构
// 或更简单: 提供 pixman_fini() 公开 API, 让用户在 main 末尾调
void pixman_fini(void) { /* 释放 global_implementations 链表 */ }
PIXMAN_API void pixman_fini(void);  // 在 pixman.h 加声明
```

**改动 2 — cairo win32 device**: `_cairo_win32_device_get()` 缓存一个全局 `__cairo_win32_device`, cairo_debug_reset_static_data() 默认不清理它. 修复:

```c
// cairo-1.18.4/sources/src/win32/cairo-win32-device.c 加:
void _cairo_win32_device_reset_static_data(void) {
    if (__cairo_win32_device == NULL) return;
    cairo_device_t* d = __cairo_win32_device;
    __cairo_win32_device = NULL;
    cairo_device_destroy(d);  /* 实测 refcount=1, 1 次 destroy 即可 */
}

// 在 cairo-1.18.4/sources/src/cairo-debug.c 的 _cairo_debug_reset_static_data() 末尾加:
#if CAIRO_HAS_WIN32_SURFACE
    _cairo_win32_device_reset_static_data();
#endif
```

**用户代码**末尾:

```cpp
#include <cairo.h>
#include <pixman.h>
// ... 所有 cairo_t / cairo_surface_t 都已 destroy
cairo_debug_reset_static_data();  // cairo 自身 11 个 cache 全部 release
pixman_fini();                    // pixman 5 个 implementation release
// 现在 crtdbg / valgrind / 静态分析都报 0 leak
```

**实测**: `TestCAiro2.exe` (Debug) 在做完 PNG/SVG/PDF/PS 导出后调用上面三个函数, 末尾打印:

```
[OK] 0 leak
```

之前没这些清理时同样的 demo 报 ~526 KB leak (大量 cairo `pattern` / `scaled_font` hash table entry).

---

## 9. 函数索引（按字母）

```
KG_Arc                  KG_Clear              KG_ClearDash
KG_Create               KG_CreateFromBuffer   KG_CurveTo
KG_Destroy              KG_DrawArc            KG_DrawCircle
KG_DrawEllipse          KG_DrawLine           KG_DrawPolygon
KG_DrawPolyline         KG_DrawRectangle      KG_DrawText
KG_Fill                 KG_Flush              KG_GetData
KG_GetHeight            KG_GetStride          KG_GetTextWidth
KG_GetWidth             KG_IsDoubleBuffered   KG_IsLiveMode
KG_LineTo               KG_MarkDirty          KG_MarkDirtyRect
KG_MoveTo               KG_NewPath            KG_Restore
KG_Rotate               KG_Save               KG_SaveToFile
KG_Scale                KG_SetAutoSwapOnFlush KG_SetDash
KG_SetFont              KG_SetFontSize        KG_SetLineCap
KG_SetLineJoin          KG_SetLineWidth       KG_SetSourceColor
KG_SetSourceRGB         KG_Stroke             KG_SwapBuffers
KG_Translate            KG_ClosePath          KG_SetFillRule (略)
```

完整签名见 `KindyunGraphic.h`。

---

## 10. 故障排查清单

- [ ] `KindyunGraphic.h` 在 include 路径中
- [ ] `KindyunGraphic.lib` 在 lib 路径中
- [ ] `KindyunGraphic.dll` 在 `.exe` 同目录（或 PATH 里）
- [ ] 工程字符集设 `Unicode`，编译加 `/utf-8`
- [ ] 如果用 MFC：项目属性 → 高级 → MFC 使用 → 在共享 DLL 中使用 MFC
- [ ] 如果还不行：用 `dumpbin /dependents your.exe` 看缺什么 DLL

---

**© 2026 KindyunGraphic** | Cairo 1.18.4 backend | VS 2022 / v143 toolset
