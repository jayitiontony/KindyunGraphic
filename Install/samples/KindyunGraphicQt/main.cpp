// =============================================================================
// main.cpp - KindyunGraphicQt 程序入口
//
// Qt 调用示例: 动态加载 KindyunGraphic.dll, 16ms 定时器驱动动画,
// 用 QPainter 把 read buffer 画到 widget, 标题栏实时显示 FPS + Render ms。
//
// 编译:
//   - 推荐: 用 Qt Creator 打开 KindyunGraphicQt.pro
//   - 或 CMake: cmake -B build -DCMAKE_PREFIX_PATH=<Qt 路径>
//   - 或 MSVC: 见 README.md
// =============================================================================

#include "MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    MainWindow w;
    w.show();
    return app.exec();
}