cmake_minimum_required(VERSION 3.28...3.30)

project(
	mygame
	VERSION 0.0.1
	LANGUAGES CXX
)

# 必须至少使用 C++20。
set(CMAKE_CXX_STANDARD 20)

if(CMAKE_SYSTEM_NAME MATCHES "Emscripten")
	set(EMSCRIPTEN TRUE)
endif()

# 确保所有二进制输出放在同一个构建目录。
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 默认禁用构建 Cute Framework 示例
set(CF_FRAMEWORK_BUILD_SAMPLES OFF)

# 默认禁用构建 Cute Framework 测试
set(CF_FRAMEWORK_BUILD_TESTS OFF)

# 这将在第一次构建游戏时下载并构建 Cute Framework（只执行一次）。
include(FetchContent)
FetchContent_Declare(
	cute
	GIT_REPOSITORY https://github.com/RandyGaul/cute_framework
	GIT_TAG master
	GIT_SHALLOW
)
FetchContent_MakeAvailable(cute)

# 可选：编译时开启调试宏（通过 CMake 选项控制）
option(ENABLE_DEBUG "Enable MyCuteGame debug features" OFF)
if(ENABLE_DEBUG)
	message(STATUS "MyCuteGame: ENABLE_DEBUG=ON -> compiling with debug macros")
else()
	message(STATUS "MyCuteGame: ENABLE_DEBUG=OFF -> compiling without debug macros")
endif()

# 自动收集源文件（只需指定源目录即可）
# 使用 CONFIGURE_DEPENDS 以便在添加/删除文件时自动重新配置
file(GLOB_RECURSE PROJECT_SOURCES
	CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/objects/*.cpp"
)

# 可选：把头文件也加入目标以便 IDE 显示（不会被编译）
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
 "objects/player_object.h" "objects/player_object.cpp" "objects/test_object.h" "objects/test_object.cpp")
 
# 将 head 目录添加为该目标的包含路径：
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/head)
# 我们的源代码位于 `src` 文件夹。
target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)

target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/objects>)

# 将 ENABLE_DEBUG 翻译为目标级别的预处理宏（优先使用 target_compile_definitions）
# debug 配置：MCG_DEBUG 与 MCG_DEBUG_LEVEL 会被头文件 head/debug_config.h 使用
if(ENABLE_DEBUG)
	target_compile_definitions(${PROJECT_NAME} PRIVATE MCG_DEBUG=1 MCG_DEBUG_LEVEL=2)
else()
	target_compile_definitions(${PROJECT_NAME} PRIVATE MCG_DEBUG=0 MCG_DEBUG_LEVEL=0)
endif()

# CMake 生成 Info.plist 文件所需的一些基本信息。
# 例如：iOS 构建需要这些信息。
if(APPLE)
	set_target_properties(
		${PROJECT_NAME}
		PROPERTIES
		MACOSX_BUNDLE_GUI_IDENTIFIER "com.myteam.${PROJECT_NAME}"
		MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
		MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
	)
endif()

# 将游戏链接到 Cute Framework。
target_link_libraries(${PROJECT_NAME} cute)

# 为了在 Windows 上方便，设置 MSVC 调试器的工作目录为构建文件夹。
# 并请求 MSVC 将该游戏设置为启动项目。
if (MSVC)
	set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:${PROJECT_NAME}>)
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
endif()

# 把 content 目录复制到可执行文件的运行目录（构建后动作）
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/content"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/content"
)

# 在 CMakeLists.txt 中（target 已创建之后）
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)