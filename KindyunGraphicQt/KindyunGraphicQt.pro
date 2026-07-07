# =============================================================================
# KindyunGraphicQt.pro - qmake 工程文件 (Qt Creator 打开)
#
# 用法: Qt Creator 打开此文件, 配置 kit (Qt 5.14+ / MinGW 或 MSVC) → 编译运行
# =============================================================================

QT       += core gui widgets
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG   += c++17
TARGET   = KindyunGraphicQt
TEMPLATE = app

# -----------------------------------------------------------------------------
# KindyunGraphic 路径 (改成你自己的 Install 目录)
# Windows 默认: ../Install
# -----------------------------------------------------------------------------
KINDYUNGRAPHIC_DIR = $$absolute_path($$_PRO_FILE_PWD_/../Install)

isEmpty(KINDYUNGRAPHIC_DIR) {
    error("KINDYUNGRAPHIC_DIR not set; edit KindyunGraphicQt.pro to point at your Install/")
}

INCLUDEPATH += $$KINDYUNGRAPHIC_DIR/include
LIBS        += -L$$KINDYUNGRAPHIC_DIR/lib -lKindyunGraphic

# 运行时把 KindyunGraphic.dll 拷到 build 目录 (Windows)
win32 {
    KG_DLL_SRC = $$KINDYUNGRAPHIC_DIR/bin/$${CONFIG}/KindyunGraphic.dll
    exists($$KG_DLL_SRC) {
        KG_DLL_DST = $$OUT_PWD/KindyunGraphic.dll
        QMAKE_POST_LINK += $$QMAKE_COPY $$KG_DLL_SRC $$KG_DLL_DST
    } else {
        warning("KindyunGraphic.dll not found at $$KG_DLL_SRC -- copy it manually or set KINDYUNGRAPHIC_PATH")
    }
}

SOURCES += main.cpp \
           MainWindow.cpp

HEADERS += MainWindow.h