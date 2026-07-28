# CairoCanvas 内存画布封装类 - 调用说明文档

基于 [Cairo 1.18.4](https://www.cairographics.org/) 矢量绘图库封装的一套 C++
内存画布类, 提供了绘制直线 / 曲线 / 圆 / 椭圆 / 多边形 / 文字 等矢量图元的
高级接口, 并能一键导出为 PNG / SVG / PDF / PostScript 等多种格式。

---

## 1. 目录结构

```
TestCAiro2/
├── CairoCanvas.h           # 封装类头文件 (接口定义)
├── CairoCanvas.cpp         # 封装类实现
├── TestCAiro2.cpp          # 演示程序 (main)
├── TestCAiro2.vcxproj      # VS 项目文件 (已配置好 cairo 链接)
├── TestCAiro2.vcxproj.filters
├── README.md               # 本文档
└── x64/Debug/              # 构建产物 (运行后还会生成 demo_output.*)
```

依赖的第三方库位于上级目录:

```
MemoryGraphic/
├── cairo-1.18.4/           # Cairo 1.18.4 头文件与库
│   ├── include/cairo.h
│   └── lib/x64-v143/
│       ├── cairo2.lib      # Release 版导入库
│       ├── cairo2d.lib     # Debug 版导入库
│       ├── cairo2.dll      # Release 版运行时 DLL
│       └── cairo2d.dll     # Debug 版运行时 DLL
├── scribus-lib-paths.props # 公共依赖路径宏 (CAIRO_INCLUDE_DIR 等)
└── ...
```

> 其他依赖 (pixman / freetype / libpng / zlib) 已被静态链接进
> `cairo2.dll` / `cairo2d.dll`, 因此运行程序时**只需要 cairo2.dll
> 或 cairo2d.dll** 在可被找到的路径上 (推荐放在 exe 同目录)。

---

## 2. 构建方式

### 2.1 Visual Studio 2022

1. 用 VS2022 打开上级目录的 `MemoryGraphic.sln`。
2. 选择平台 **x64**, 配置 **Debug** (或 **Release**)。
3. 右键 `TestCAiro2` -> "生成"。
4. 构建成功后, 在 `TestCAiro2/x64/Debug/` 下会生成 `TestCAiro2.exe`。
5. **首次运行前**: 把 `cairo-1.18.4/lib/x64-v143/cairo2d.dll`
   (Debug) 或 `cairo2.dll` (Release) 复制到 `TestCAiro2.exe` 同目录,
   否则会报 "找不到 cairo2d.dll" 的错误。

### 2.2 命令行 (MSBuild)

```powershell
# 在 MemoryGraphic 目录执行
msbuild MemoryGraphic.sln /p:Configuration=Debug /p:Platform=x64 /m
```

---

## 3. 运行示例程序

进入 `TestCAiro2/x64/Debug/` 后执行:

```powershell
.\TestCAiro2.exe
```

正常情况下会在当前工作目录生成 4 个文件:

| 文件 | 类型 | 说明 |
|------|------|------|
| `demo_output.png` | 位图 | 1000x700 像素, ARGB32 格式 |
| `demo_output.svg` | **矢量** | cairo 生成的真正 SVG, 可在浏览器 / Inkscape 打开 |
| `demo_output.pdf` | **矢量** | PDF 1.4, 适合打印 |
| `demo_output.ps`  | **矢量** | PostScript Level 3 |

> 因为内部使用 `recording surface`, cairo 在导出 SVG / PDF / PS 时会
> **replay 所有绘图操作** 生成真正的矢量描述 (而不是把位图嵌入 SVG)。

---

## 4. 头文件一览

```cpp
#include "CairoCanvas.h"
using namespace memgc;          // 可选, 推荐使用命名空间避免名字冲突

CairoCanvas canvas(800, 600);   // 创建 800x600 内存画布
```

### 4.1 数据结构

| 类型 | 说明 |
|------|------|
| `Color` | RGBA 颜色 (double 0.0-1.0), 提供 `Color::Red() / Blue() / ...` 等预设 |
| `Color::FromRGB(r,g,b,a=255)` | 从 0-255 字节构造颜色 |
| `FontStyle` | 字体描述: family / size / bold / italic |
| `Point` | 二维点 `{x, y}` |
| `LineCap` | `Butt` / `Round` / `Square` |
| `LineJoin` | `Miter` / `Round` / `Bevel` |
| `FillRule` | `NonZero` / `EvenOdd` |
| `SaveFormat` | `Png` / `Svg` / `Pdf` / `Ps` |

### 4.2 接口分类

| 类别 | 方法 |
|------|------|
| 状态设置 | `SetSourceColor` / `SetSourceRGB` / `SetLineWidth` / `SetLineCap` / `SetLineJoin` / `SetDash` / `ClearDash` / `SetFillRule` |
| 底层路径 | `MoveTo` / `LineTo` / `CurveTo` / `Arc` / `ArcTo` / `ClosePath` / `Stroke` / `Fill` / `NewPath` |
| 高级图形 | `DrawLine` / `DrawPolyline` / `DrawPolygon` / `DrawRectangle` / `DrawCircle` / `DrawEllipse` / `DrawArc` / `DrawCurve` / `DrawCurveFrom` |
| 文字 | `SetFont` / `SetFontSize` / `DrawText` / `DrawTextUTF8` / `GetTextWidth` / `GetTextExtents` |
| 画布操作 | `Clear` / `Translate` / `Scale` / `Rotate` / `Save` / `Restore` |
| 导出 | `SaveAsPng` / `SaveAsSvg` / `SaveAsPdf` / `SaveAsPs` / `Save` |
| 元信息 | `GetWidth` / `GetHeight` / `GetCairoContext` / `GetRecordingSurface` |

---

## 5. 典型用法

### 5.1 最小示例: 画一条红线并保存

```cpp
#include "CairoCanvas.h"
using namespace memgc;

int main() {
    CairoCanvas canvas(400, 300);              // 1) 创建画布
    canvas.Clear(Color::White());              // 2) 清空 (默认白)
    canvas.SetSourceColor(Color::Red());       // 3) 选红色
    canvas.SetLineWidth(3.0);                  // 4) 线宽 3 像素
    canvas.DrawLine(20, 20, 380, 280);         // 5) 画一条对角线
    canvas.SaveAsPng("red_line.png");          // 6) 保存
    return 0;
}
```

### 5.2 状态管理: Save / Restore

```cpp
canvas.Save();                            // 保存当前状态
canvas.Translate(100, 100);               // 平移原点
canvas.Rotate(M_PI / 4);                  // 旋转 45°
canvas.SetSourceColor(Color::Blue());
canvas.DrawCircle(0, 0, 50, true);        // 在 (0,0) 画一个圆 (实际是 (100,100))
canvas.Restore();                         // 恢复到之前的状态
```

### 5.3 自定义路径 (一段一段拼出来)

```cpp
canvas.NewPath();
canvas.MoveTo(50, 200);
canvas.LineTo(150, 50);
canvas.CurveTo(200, 100, 250, 200, 350, 50);  // 贝塞尔
canvas.LineTo(450, 200);
canvas.ClosePath();
canvas.SetSourceColor(Color::Blue());
canvas.SetLineWidth(2.0);
canvas.Stroke();                              // 描边
// canvas.Fill();                            // 或者填充
// canvas.StrokeAndFill();                   // 或者描边 + 填充
```

### 5.4 中文 / 多字节文字

`cairo_show_text` 内部按 UTF-8 解析字节, 因此只需要把 UTF-8 字符串传入:

```cpp
canvas.SetFont(FontStyle("Microsoft YaHei", 16, false, false));
canvas.SetSourceColor(Color::Black());
canvas.DrawTextUTF8(50, 100, "你好, cairo! 中文测试 ✓");
```

### 5.5 虚线

```cpp
canvas.SetSourceColor(Color::Red());
canvas.SetLineWidth(2.0);
canvas.SetDash({8.0, 4.0});                 // 8 实 + 4 空, 循环
canvas.DrawLine(0, 50, 400, 50);
canvas.ClearDash();                         // 取消虚线, 恢复实线
```

### 5.6 半透明颜色

```cpp
canvas.SetSourceColor(Color::FromRGB(255, 0, 0, 128));   // 50% 透明红
canvas.DrawRectangle(50, 50, 100, 100, true);

canvas.SetSourceColor(Color::FromRGB(0, 0, 255, 128));   // 50% 透明蓝
canvas.DrawRectangle(100, 100, 100, 100, true);          // 重叠区域会做 alpha 混合
```

### 5.7 同时保存 PNG + SVG

```cpp
canvas.SaveAsPng("out.png");
canvas.SaveAsSvg("out.svg");   // 矢量 SVG, 可在浏览器 / Illustrator 二次编辑
```

---

## 6. 高级特性

### 6.1 直接访问 cairo 上下文

如果需要使用 `CairoCanvas` 尚未封装的 cairo API (例如 pattern, gradient,
mask 等), 可以直接拿到底层 `cairo_t*`:

```cpp
cairo_t* cr = canvas.GetCairoContext();

// 例如: 绘制一个线性渐变填充矩形
cairo_pattern_t* pat = cairo_pattern_create_linear(0, 0, 200, 0);
cairo_pattern_add_color_stop_rgba(pat, 0.0, 1.0, 0.0, 0.0, 1.0);
cairo_pattern_add_color_stop_rgba(pat, 1.0, 0.0, 0.0, 1.0, 1.0);
cairo_set_source(cr, pat);
cairo_rectangle(cr, 0, 0, 200, 100);
cairo_fill(cr);
cairo_pattern_destroy(pat);
```

### 6.2 圆弧示例

```cpp
// 从 0 弧度画到 π (半圆)
canvas.SetSourceColor(Color::Blue());
canvas.SetLineWidth(4.0);
canvas.DrawArc(100, 100, 50, 0.0, M_PI);

// 注意: cairo 弧度方向为逆时针, 0 弧度 = +X 方向
```

---

## 7. 注意事项

1. **DLL 部署**: 把 `cairo2.dll` (Release) 或 `cairo2d.dll` (Debug) 放到
   `TestCAiro2.exe` 同目录, 或把 `cairo-1.18.4/lib/x64-v143/` 加入 PATH。
2. **不支持的平台**: cairo 的 Win32 surface / Quartz surface 等其他后端
   在本封装中未使用; 只用 image / svg / pdf / ps 后端。
3. **中文文字**: 字体族名必须存在系统中; 推荐 `"Microsoft YaHei"` /
   `"SimSun"` / `"Arial Unicode MS"`。找不到字体时 cairo 会自动回退。
4. **Y 轴方向**: Cairo 的 Y 轴向下 (屏幕坐标), 与大多数 GUI 框架一致。
5. **recording surface**: 所有绘制都被记录, 即使 SaveAsPng 多次, 也可以
   任意顺序导出多个不同格式, 而不需要重新绘制。
6. **线程**: 单个 `CairoCanvas` 实例不是线程安全的, 多线程场景下
   每个线程使用独立实例, 或在外层加锁。

---

## 8. 常见问题

### Q1: 编译报 "无法打开包括文件: 'cairo.h'"

检查 vcxproj 中是否正确导入了 `scribus-lib-paths.props`, 以及
`$(CAIRO_INCLUDE_DIR)` 宏是否解析到 `cairo-1.18.4/include`。

### Q2: 链接报 "无法解析的外部符号 cairo_xxx"

确认 `$(CAIRO_LIB_DIR)` 与 `$(CAIRO_LIB)` 宏正确解析。Debug 配置应该链接
`cairo2d.lib`, Release 配置应该链接 `cairo2.lib`。

### Q3: 运行报 "cairo2d.dll 找不到"

把 `cairo-1.18.4/lib/x64-v143/cairo2d.dll` 复制到 `TestCAiro2.exe` 同目录。

### Q4: SVG 文件打开后是空白的

确认 `cairo_surface_flush` 与 `cairo_surface_destroy` 都已被调用
(CairoCanvas 内部已处理); 或确认没有在保存前抛出异常。

### Q5: 中文显示为方块

说明系统找不到指定字体。可以在 Windows 上 `Win+R` -> 输入 `fonts` 打开
字体管理器, 确认 `Microsoft YaHei` 等字体存在, 或换一个确实存在的字体名。

---

## 9. 参考链接

- [Cairo 官方文档](https://www.cairographics.org/documentation/)
- [Cairo 1.18.4 API 参考](https://www.cairographics.org/manual/)
- 仓库根目录下的 `MemoryGraphic.sln` 解决方案, 包含 cairo 库本身的
  编译配置, 可作为参考。