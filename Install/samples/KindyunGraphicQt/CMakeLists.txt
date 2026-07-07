# =============================================================================
# CMakeLists.txt - KindyunGraphicQt (推荐编译方式, 跨平台)
#
# 用法:
#   mkdir build && cd build
#   cmake .. -DCMAKE_PREFIX_PATH=/path/to/Qt/5.14.2/msvc2017_64 \
#            -DKINDYUNGRAPHIC_DIR=/path/to/Install        # KindyunGraphic 目录
#   cmake --build . --config Release
#
# Qt5 / Qt6 都支持 (自动找 Qt6Core / Qt5Core)
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(KindyunGraphicQt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)   # 让 CMake 自动跑 moc (处理 Q_OBJECT)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ----------------------------------------------------------------------------
# 找 Qt (Qt5 或 Qt6 都可以)
# ----------------------------------------------------------------------------
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets)

# ----------------------------------------------------------------------------
# 找 KindyunGraphic (头文件 + 动态库 + import lib)
# KINDYUNGRAPHIC_DIR 指向 KindyunGraphic/Install, 默认 ../Install
# ----------------------------------------------------------------------------
if(NOT DEFINED KINDYUNGRAPHIC_DIR)
    set(KINDYUNGRAPHIC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Install")
endif()

if(NOT EXISTS "${KINDYUNGRAPHIC_DIR}/include/KindyunGraphic.h")
    message(FATAL_ERROR
        "KindyunGraphic.h not found at ${KINDYUNGRAPHIC_DIR}/include/KindyunGraphic.h\n"
        "Set -DKINDYUNGRAPHIC_DIR=/path/to/Install")
endif()

if(WIN32)
    set(_kg_lib_name "KindyunGraphic")
elseif(APPLE)
    set(_kg_lib_name "KindyunGraphic")
else()
    set(_kg_lib_name "KindyunGraphic")
endif()

# 运行时需要的 .dll / .so 我们在 build 阶段 copy 到可执行文件旁边
add_executable(KindyunGraphicQt
    main.cpp
    MainWindow.cpp
    MainWindow.h
)

target_include_directories(KindyunGraphicQt PRIVATE
    "${KINDYUNGRAPHIC_DIR}/include"
)

target_link_libraries(KindyunGraphicQt PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
)

# ----------------------------------------------------------------------------
# 运行时 DLL/SO: 拷贝到可执行文件旁边, 这样不用配 PATH/LD_LIBRARY_PATH
# ----------------------------------------------------------------------------
if(WIN32)
    add_custom_command(TARGET KindyunGraphicQt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${KINDYUNGRAPHIC_DIR}/bin/${CMAKE_BUILD_TYPE}/KindyunGraphic.dll"
            "$<TARGET_FILE_DIR:KindyunGraphicQt>/"
        COMMENT "[KindyunGraphicQt] Copying KindyunGraphic.dll to output dir"
    )
elseif(APPLE)
    add_custom_command(TARGET KindyunGraphicQt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${KINDYUNGRAPHIC_DIR}/lib/libKindyunGraphic.dylib"
            "$<TARGET_FILE_DIR:KindyunGraphicQt>/"
    )
else()
    add_custom_command(TARGET KindyunGraphicQt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${KINDYUNGRAPHIC_DIR}/lib/libKindyunGraphic.so"
            "$<TARGET_FILE_DIR:KindyunGraphicQt>/"
    )
endif()

# 拷贝 Qt 运行时依赖 (windeployqt for Windows)
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt${QT_VERSION_MAJOR}_DIR}/../../../bin")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET KindyunGraphicQt POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations --no-system-d3d-compiler --no-opengl-sw
                "$<TARGET_FILE:KindyunGraphicQt>"
            COMMENT "[KindyunGraphicQt] Running windeployqt to bundle Qt DLLs"
        )
    endif()
endif()