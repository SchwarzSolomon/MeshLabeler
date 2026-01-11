cmake_minimum_required(VERSION 3.12)
project(MeshLabeler VERSION 1.0.0 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 自动处理
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 查找依赖包
find_package(Qt5 REQUIRED COMPONENTS
    Core
    Widgets
    Gui
)

find_package(VTK REQUIRED COMPONENTS
    CommonCore
    CommonDataModel
    FiltersSources
    FiltersGeometry
    IOGeometry
    IOXML
    InteractionStyle
    RenderingCore
    RenderingOpenGL2
    RenderingFreeType
    GUISupportQt
)

if(VTK_VERSION VERSION_LESS "8.90.0")
    include(${VTK_USE_FILE})
endif()

# 源文件
set(SOURCES
    main.cpp
    mainwindow.cpp
    meshlabeler.cpp
)

set(HEADERS
    mainwindow.h
    meshlabeler.h
)

set(UI_FILES
    mainwindow.ui
)

# 创建可执行文件
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${UI_FILES}
)

# 链接库
target_link_libraries(${PROJECT_NAME}
    Qt5::Core
    Qt5::Widgets
    Qt5::Gui
    ${VTK_LIBRARIES}
)

# VTK 模块初始化（VTK 9+）
if(VTK_VERSION VERSION_GREATER_EQUAL "8.90.0")
    vtk_module_autoinit(
        TARGETS ${PROJECT_NAME}
        MODULES ${VTK_LIBRARIES}
    )
endif()

# 安装规则
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# 编译选项
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4
        /utf-8
        /MP
    )
    # 设置为 Release 模式
    set(CMAKE_CONFIGURATION_TYPES "Release" CACHE STRING "" FORCE)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

# 调试信息
message(STATUS "=== MeshLabeler Build Configuration ===")
message(STATUS "CMake version: ${CMAKE_VERSION}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Qt5 version: ${Qt5_VERSION}")
message(STATUS "VTK version: ${VTK_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "=======================================")
