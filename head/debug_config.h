#pragma once
// --- debug_config.h (临时诊断用) ---
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
#ifndef PNG_LOAD_DEBUG
#define	PNG_LOAD_DEBUG 0
#endif
#ifndef TESTER
#define TESTER 0
#endif
// --- 结束临时诊断 ---