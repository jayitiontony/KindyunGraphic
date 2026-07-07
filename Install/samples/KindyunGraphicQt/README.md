# KindyunGraphicQt - Qt 调用示例

和 [`KindyunGraphicMfc`](../KindyunGraphicMfc) 功能 1:1 的 Qt 实现:
- 16ms 定时器驱动 cairo 动画
- 双缓冲由 `KG_Create` 默认开启 (内部分配 2 个 buffer)
- 标题栏实时显示 `FPS` + 纯内存绘制平均耗时 (`Render: X.XX ms`)
- 用 `QImage::Format_ARGB32` (Windows 小端 = BGRA 内存布局) wrap read buffer
  → `QPainter::drawImage` **零拷贝** 上屏

## 文件

| 文件                  | 说明                                                |
| --------------------- | --------------------------------------------------- |
| `main.cpp`            | `QApplication` 入口                                 |
| `MainWindow.h`        | 主窗口头 (类成员 + DLL 函数指针)                    |
| `MainWindow.cpp`      | 主窗口实现 (DLSYM / 渲染 / paintEvent)              |
| `CMakeLists.txt`      | **推荐** — CMake, 跨平台, 自动找 Qt5/Qt6           |
| `KindyunGraphicQt.pro`| qmake 工程 (Qt Creator 直接打开)                    |

## 编译

### 方式 A — Qt Creator (最简单)

1. 打开 `KindyunGraphicQt.pro`
2. 配置 Kit (任意 Qt 5.12+ / Qt 6, MSVC / MinGW 均可)
3. Build & Run

### 方式 B — CMake (推荐, 跨平台)

```bash
cd KindyunGraphicQt
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/5.14.2/msvc2017_64 \
                 -DKINDYUNGRAPHIC_DIR=/path/to/MemoryGraphic/Install
cmake --build build --config Release
```

产物在 `build/Release/KindyunGraphicQt.exe`, 同目录会自动有 `KindyunGraphic.dll`
和 Qt 的运行时 DLL (Windows 上通过 `windeployqt` 自动 bundle)。

### 方式 C — MSVC `cl.exe` 直接编

```cmd
cl /std:c++17 /EHsc /MD ^
   /I ..\Install\include ^
   /I C:\Qt\Qt5.14.2\5.14.2\msvc2017_64\include ^
   /I C:\Qt\Qt5.14.2\5.14.2\msvc2017_64\include\QtCore ^
   /I C:\Qt\Qt5.14.2\5.14.2\msvc2017_64\include\QtGui ^
   /I C:\Qt\Qt5.14.2\5.14.2\msvc2017_64\include\QtWidgets ^
   /D QT_CORE_LIB /D QT_GUI_LIB /D QT_WIDGETS_LIB ^
   main.cpp MainWindow.cpp /link ^
   /LIBPATH:..\Install\lib KindyunGraphic.lib ^
   /LIBPATH:C:\Qt\Qt5.14.2\5.14.2\msvc2017_64\lib Qt5Core.lib Qt5Gui.lib Qt5Widgets.lib
```

## 运行

- 确保 `KindyunGraphic.dll` 与 `KindyunGraphicQt.exe` 在同一目录
- Windows 也支持通过环境变量指定 DLL 路径:
  ```cmd
  set KINDYUNGRAPHIC_PATH=D:\Develop\MemoryGraphic\Install\bin\Release\KindyunGraphic.dll
  ```

## 关键点对照 (Qt vs MFC)

| 功能        | MFC                          | Qt                           |
| ----------- | ---------------------------- | ---------------------------- |
| 加载 DLL    | `LoadLibrary` + `GetProcAddress` | `dlopen` + `dlsym` (跨平台) |
| 窗口        | `CFrameWnd`                  | `QMainWindow`                |
| 定时器      | `SetTimer` + `OnTimer`       | `startTimer` + `timerEvent`  |
| 触发重绘    | `Invalidate()`               | `update()`                   |
| 绘制回调    | `OnPaint` + `CPaintDC`       | `paintEvent` + `QPainter`    |
| 显示 buffer | `StretchDIBits` (GDI)        | `QPainter::drawImage` (零拷贝) |
| 标题栏      | `SetWindowText`              | `setWindowText`              |

## 截图

和 MFC 版本画面一致 — 蓝色小球 + 6 个橙色方块 + 绿色螺旋线 + 黄色标题文字。

标题栏示例:
```
KindyunGraphic Qt Demo   |   FPS: 60.0   |   Render: 4.12 ms
```