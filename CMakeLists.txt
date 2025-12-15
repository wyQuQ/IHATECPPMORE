cmake_minimum_required(VERSION 3.28...3.30)

project(
	mygame
	VERSION 0.0.1
	LANGUAGES CXX
)

# 指定默认使用 C++20 标准
set(CMAKE_CXX_STANDARD 20)

if(CMAKE_SYSTEM_NAME MATCHES "Emscripten")
	set(EMSCRIPTEN TRUE)
endif()

# 将运行时输出目录设置为构建目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 默认关闭 Cute Framework 示例项目
set(CF_FRAMEWORK_BUILD_SAMPLES OFF)

# 默认关闭 Cute Framework 测试构建
set(CF_FRAMEWORK_BUILD_TESTS OFF)

# 使用 FetchContent 获取 Cute Framework（仅在需要时）
include(FetchContent)
FetchContent_Declare(
	cute
	GIT_REPOSITORY https://github.com/RandyGaul/cute_framework
	GIT_TAG master
	GIT_SHALLOW
)
FetchContent_MakeAvailable(cute)

# 可选：启用 MyCuteGame 调试功能
option(ENABLE_DEBUG "Enable MyCuteGame debug features" OFF)
if(ENABLE_DEBUG)
	message(STATUS "MyCuteGame: ENABLE_DEBUG=ON -> compiling with debug macros")
else()
	message(STATUS "MyCuteGame: ENABLE_DEBUG=OFF -> compiling without debug macros")
endif()

# 自动收集源代码文件（CONFIGURE_DEPENDS 在添加/删除文件时会触发重新配置）
file(GLOB_RECURSE PROJECT_SOURCES
	CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/objects/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/objects/*.c"
)

# 可选：收集头文件，以便 IDE 在项目树中显示
file(GLOB_RECURSE PROJECT_HEADERS
	"${CMAKE_CURRENT_SOURCE_DIR}/head/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/head/*.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/objects/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/objects/*.hpp"
)

add_executable(${PROJECT_NAME}
	${PROJECT_SOURCES}
	${PROJECT_HEADERS}
 "objects/checkpoint.h" "objects/checkpoint.cpp")

# 将 head 目录添加到目标的包含路径
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/head)
# 将 src 目录仅作为构建接口包含路径（分离源代码和头文件）
target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)

# 同样 objects 目录添加包含路径
target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/objects>)

# FetchContent 后、target_link_libraries 之前
target_include_directories(${PROJECT_NAME}
    PRIVATE
    ${cute_SOURCE_DIR}/src
)

# 根据 ENABLE_DEBUG 定义目标的宏标记（使用 target_compile_definitions）
# debug 宏（MCG_DEBUG / MCG_DEBUG_LEVEL）会被 head/debug_config.h 使用
if(ENABLE_DEBUG)
	target_compile_definitions(${PROJECT_NAME} PRIVATE MCG_DEBUG=1 MCG_DEBUG_LEVEL=2)
else()
	target_compile_definitions(${PROJECT_NAME} PRIVATE MCG_DEBUG=0 MCG_DEBUG_LEVEL=0)
endif()

# 为 macOS 应用设置 Info.plist 中的一些属性（如果需要）
if(APPLE)
	set_target_properties(
		${PROJECT_NAME}
		PROPERTIES
		MACOSX_BUNDLE_GUI_IDENTIFIER "com.myteam.${PROJECT_NAME}"
		MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
		MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
	)
endif()

# 链接 Cute Framework 到游戏项目
target_link_libraries(${PROJECT_NAME} cute)

# 为 Windows 平台（如 MSVC 中）的工作目录设置启动目标
if (MSVC)
	set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:${PROJECT_NAME}>)
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
endif()

# 构建后将 content 目录复制到可执行文件目录
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/content"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/content"
)

# 为 MSVC 设置编译选项，启用 UTF-8 源文件编码支持
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)