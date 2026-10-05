cmake_minimum_required(VERSION 3.16)

project(ADASim VERSION 2.8.1 LANGUAGES CXX)

include(GNUInstallDirs) #提供标准 Linux 安装目录变量

# 生成 compile_commands.json
# 后续 clangd / VSCode / 静态分析工具可以使用
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# C++ 标准与 Qt 自动生成工具配置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找 Qt (向下兼容 Qt5，向上支持 Qt6)
find_package(QT 
            NAMES Qt6 Qt5 
            REQUIRED COMPONENTS 
            Core 
            Gui 
            Widgets
            Network
            Test)

find_package(
            Qt${QT_VERSION_MAJOR}
            REQUIRED COMPONENTS
            Core
            Gui
            Widgets
            Network
            Test
)

configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/Version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/Version.h
    @ONLY
)

# ============================================================================
# [源文件清单]
# ============================================================================
set(PROJECT_SOURCES
    src/main.cpp

    src/gui/MainWindow.cpp
    src/gui/View2D.cpp
    src/gui/SensorView.cpp
    src/gui/ControlMonitor.cpp

    src/backend/DataLoader.cpp
    src/backend/DataManager.cpp
    src/backend/SimulationRecorder.cpp
    src/backend/SimulationEngine.cpp

    src/algorithm/ObstacleDetector.cpp
    src/algorithm/PathPredictor.cpp
    src/algorithm/VehicleModel.cpp
    src/algorithm/TrajectoryController.cpp
    src/algorithm/PurePursuitController.cpp
    src/algorithm/LongitudinalController.cpp

    src/communication/Socket.cpp
    src/communication/PlannerLink.cpp
    src/communication/SerialPort.cpp
    src/communication/SerialProtocol.cpp

    src/system/LinuxLogger.cpp
    src/system/LinuxSignalHandler.cpp

    src/config/ConfigManager.cpp

    src/headless/HeadlessRunner.cpp

    src/scenario/ScenarioLoader.cpp

    src/test/TestEvaluator.cpp
    src/test/TestSuite.cpp
    src/test/TestSuiteRunner.cpp
)

# ==========================================
# V2.1 默认运行配置
# ==========================================

file( MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/config)

if(NOT EXISTS
   "${CMAKE_BINARY_DIR}/config/adasim.ini")

    configure_file(
        ${CMAKE_SOURCE_DIR}/config/adasim.ini
        ${CMAKE_BINARY_DIR}/config/adasim.ini
        COPYONLY
    )

endif()

# 创建可执行文件
add_executable(${PROJECT_NAME}
    ${PROJECT_SOURCES}
)

# 交叉编译时，代码生成工具必须在宿主机运行
if(CMAKE_CROSSCOMPILING)
    set(RK_HOST_QT_BIN "/usr/lib/qt5/bin"
        CACHE PATH "Host Qt 5 tools directory"
    )

    if(NOT QT_VERSION_MAJOR EQUAL 5)
        message(FATAL_ERROR
            "This cross-build setup requires Qt 5"
        )
    endif()

    foreach(tool moc uic rcc)
        if(NOT EXISTS "${RK_HOST_QT_BIN}/${tool}")
            message(FATAL_ERROR
                "Host Qt tool not found: ${RK_HOST_QT_BIN}/${tool}"
            )
        endif()
    endforeach()

    set_target_properties(${PROJECT_NAME} PROPERTIES
        AUTOMOC_EXECUTABLE "${RK_HOST_QT_BIN}/moc"
        AUTOUIC_EXECUTABLE "${RK_HOST_QT_BIN}/uic"
        AUTORCC_EXECUTABLE "${RK_HOST_QT_BIN}/rcc"
    )
endif()

include(CTest)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# 链接第三方库
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
)

# 包含头文件目录 (Include Directories)
target_include_directories(${PROJECT_NAME} PRIVATE

    ${CMAKE_CURRENT_BINARY_DIR} #编译 ${PROJECT_NAME} 这个程序的时候，除了去 src/ 等目录找头文件，也去当前构建目录找头文件。

    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/gui
    ${CMAKE_SOURCE_DIR}/src/backend
    ${CMAKE_SOURCE_DIR}/src/system
    ${CMAKE_SOURCE_DIR}/src/config
    ${CMAKE_SOURCE_DIR}/src/communication
    ${CMAKE_SOURCE_DIR}/src/algorithm
    ${CMAKE_SOURCE_DIR}/src/test
    
)

# ============================================================
# 编译警告
# ============================================================

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(
        ${PROJECT_NAME}
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

endif()

# 安装部署规则
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ============================================================
# 安装默认配置
# ============================================================

install(
    FILES
    ${CMAKE_SOURCE_DIR}/config/adasim.ini

    DESTINATION
    ${CMAKE_INSTALL_BINDIR}/config
)