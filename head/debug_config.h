#pragma once
// --- debug_config.h (控制debug信息显示与输出) ---
#ifndef MCG_DEBUG
#define MCG_DEBUG 0
#endif

#ifndef MCG_DEBUG_LEVEL
#define MCG_DEBUG_LEVEL 0
#endif

#if MCG_DEBUG
#pragma message("debug_config: MCG_DEBUG=1")
#else
#pragma message("debug_config: MCG_DEBUG=0")
#endif

// 组件宏
#ifndef COLLISION_DEBUG
#define COLLISION_DEBUG MCG_DEBUG
#endif
#ifndef SHAPE_DEBUG
#define SHAPE_DEBUG MCG_DEBUG
#endif
#ifndef TESTER
#define TESTER 0
#endif
#ifndef MESSAGE_DEBUG
#define MESSAGE_DEBUG MCG_DEBUG
#endif
#ifndef OUTPUT_DEBUG
#define OUTPUT_DEBUG MCG_DEBUG
#endif 

#if OUTPUT_DEBUG
#include <concepts> // 引入 concepts 头文件
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace {
    // 1. 定义一个结构体，用于强制使用 'header' 标识符
    struct Header {
        const char* header;
    };

    // 定义一个 concept，用于检查类型是否可以被输出到 std::ostream
    // 这个约束要求表达式 os << t 必须是合法的，并且返回的类型必须与 std::ostream& 相同
    template<typename T>
    concept Streamable = requires(std::ostream & os, const T & t) {
        { os << t } -> std::same_as<std::ostream&>;
    };

    std::string FormatTimestamp()
    {
        using namespace std::chrono;
        const auto now = system_clock::now();
        const auto time = system_clock::to_time_t(now);
        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &time);
#else
        localtime_r(&time, &tm);
#endif
        const auto milliseconds_since_epoch = duration_cast<milliseconds>(now.time_since_epoch());
        const auto milliseconds_part = milliseconds_since_epoch % milliseconds(1000);
        std::ostringstream oss;
        oss << std::setfill('0')
            << std::setw(2) << tm.tm_hour << "`"
            << std::setw(2) << tm.tm_min << "``"
            << std::setw(2) << tm.tm_sec << '.'
            << std::setw(3) << milliseconds_part.count();
        return oss.str();
    }

    // 2. 修改 Output 函数，使其第一个参数为 Header 结构体
    template<Streamable... Args> // 使用 concept 直接约束模板参数包
    void Output(Header h, const Args&... args)
    {
        std::cerr << "[" << FormatTimestamp() << "] ";
        std::cerr << "[" << h.header << "]";
        // 将所有参数依次输出到 std::cerr
        ((std::cerr << ' ' << args), ...);
        std::cerr << std::endl;
    }

    // 3. 为不带 Header 的调用提供一个重载
    template<Streamable... Args>
    void Output(const Args&... args)
    {
        // 调用带 Header 的版本，并提供默认值
        Output({.header = "Debug Message"}, args...);
    }
} // namespace

// 宏定义：在调试版本中启用调试输出功能
// 使用 __VA_OPT__ (C++20) 来处理可变参数
#define OUTPUT(...) Output(__VA_ARGS__)
#else
// 宏定义：确保在发布版本中完全移除调用和参数求值
#define OUTPUT(...) do {} while(0)
#endif