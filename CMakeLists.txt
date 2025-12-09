cmake_minimum_required(VERSION 3.28...3.30)

project(
	mygame
	VERSION 0.0.1
	LANGUAGES CXX
)

# ��������ʹ�� C++20��
set(CMAKE_CXX_STANDARD 20)

if(CMAKE_SYSTEM_NAME MATCHES "Emscripten")
	set(EMSCRIPTEN TRUE)
endif()

# ȷ�����ж������������ͬһ������Ŀ¼��
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Ĭ�Ͻ��ù��� Cute Framework ʾ��
set(CF_FRAMEWORK_BUILD_SAMPLES OFF)

# Ĭ�Ͻ��ù��� Cute Framework ����
set(CF_FRAMEWORK_BUILD_TESTS OFF)

# �⽫�ڵ�һ�ι�����Ϸʱ���ز����� Cute Framework��ִֻ��һ�Σ���
include(FetchContent)
FetchContent_Declare(
	cute
	GIT_REPOSITORY https://github.com/RandyGaul/cute_framework
	GIT_TAG master
	GIT_SHALLOW
)
FetchContent_MakeAvailable(cute)

# ��ѡ������ʱ������Ժ꣨ͨ�� CMake ѡ����ƣ�
option(ENABLE_DEBUG "Enable MyCuteGame debug features" OFF)
if(ENABLE_DEBUG)
	message(STATUS "MyCuteGame: ENABLE_DEBUG=ON -> compiling with debug macros")
else()
	message(STATUS "MyCuteGame: ENABLE_DEBUG=OFF -> compiling without debug macros")
endif()

# �Զ��ռ�Դ�ļ���ֻ��ָ��ԴĿ¼���ɣ�
# ʹ�� CONFIGURE_DEPENDS �Ա������/ɾ���ļ�ʱ�Զ���������
file(GLOB_RECURSE PROJECT_SOURCES
	CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# ��ѡ����ͷ�ļ�Ҳ����Ŀ���Ա� IDE ��ʾ�����ᱻ���룩
file(GLOB_RECURSE PROJECT_HEADERS
	"${CMAKE_CURRENT_SOURCE_DIR}/head/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/head/*.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

add_executable(${PROJECT_NAME}
	${PROJECT_SOURCES}
	${PROJECT_HEADERS}
 "objects/player_object.h" "objects/player_object.cpp" "objects/test_object.h" "objects/test_object.cpp" "objects/backgroud.h" "objects/backgroud.cpp" "objects/block_object.h")
 
# �� head Ŀ¼���Ϊ��Ŀ��İ���·����
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/head)
# ���ǵ�Դ����λ�� `src` �ļ��С�
target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)

target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/objects>)

# �� ENABLE_DEBUG ����ΪĿ�꼶���Ԥ����꣨����ʹ�� target_compile_definitions��
# debug ���ã�MCG_DEBUG �� MCG_DEBUG_LEVEL �ᱻͷ�ļ� head/debug_config.h ʹ��
if(ENABLE_DEBUG)
	target_compile_definitions(${PROJECT_NAME} PRIVATE MCG_DEBUG=1 MCG_DEBUG_LEVEL=2)
else()
	target_compile_definitions(${PROJECT_NAME} PRIVATE MCG_DEBUG=0 MCG_DEBUG_LEVEL=0)
endif()

# CMake ���� Info.plist �ļ������һЩ������Ϣ��
# ���磺iOS ������Ҫ��Щ��Ϣ��
if(APPLE)
	set_target_properties(
		${PROJECT_NAME}
		PROPERTIES
		MACOSX_BUNDLE_GUI_IDENTIFIER "com.myteam.${PROJECT_NAME}"
		MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
		MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
	)
endif()

# ����Ϸ���ӵ� Cute Framework��
target_link_libraries(${PROJECT_NAME} cute)

# Ϊ���� Windows �Ϸ��㣬���� MSVC �������Ĺ���Ŀ¼Ϊ�����ļ��С�
# ������ MSVC ������Ϸ����Ϊ�����Ŀ��
if (MSVC)
	set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:${PROJECT_NAME}>)
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
endif()

# �� content Ŀ¼���Ƶ���ִ���ļ�������Ŀ¼������������
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/content"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/content"
)

# �� CMakeLists.txt �У�target �Ѵ���֮��
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)