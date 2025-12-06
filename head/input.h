#pragma once
#include<cute_input.h>
#include<bitset>
#include "v2math.h"

enum class KeyState {
	Up, // 被抬起时
	Down, // 被按下时
	Hold, // 持续按下时
	Hang, // 持续抬起时
	Repeatable // 可重复触发时（按下某个键时触发，以及按下一小段时间后重复触发）
};

enum class MouseState {
	Up, // 刚抬起
	Down, // 刚按下
	Hold, // 持续按下
	Hang, // 持续抬起
	DoubleClick, // 双击
	DoubleClickAndHold // 双击并持续按下
};

namespace Input {
	inline bool IsKeyInState(CF_KeyButton key, KeyState state) noexcept;
	inline bool KeyDown(CF_KeyButton& out_key) noexcept;
	inline bool KeysDown(std::bitset<CF_KEY_COUNT>& out_keys) noexcept;

	inline bool IsMouseInState(CF_MouseButton button, MouseState state) noexcept;
	inline bool MouseDown(CF_MouseButton& out_button) noexcept;
	inline bool MouseButtonsDown(std::bitset<CF_MOUSE_BUTTON_COUNT>& out_buttons) noexcept;

	inline void SetMouseHide(bool hide) { cf_mouse_hide(hide); }
	inline bool IsMouseHidden() { return cf_mouse_hidden(); }

	inline bool MousePos(CF_V2& out_pos) noexcept {
		if (cf_mouse_hidden()) return false;
		out_pos.x = cf_mouse_x();
		out_pos.y = cf_mouse_y();
		return true;
	}
	inline bool MouseMotion(CF_V2& out_motion) noexcept {
		out_motion.x = cf_mouse_motion_x();
		out_motion.y = cf_mouse_motion_y();
		return v2math::equal(out_motion, v2math::zero());
	}
	inline bool WheelMotion(float& out_motion) noexcept {
		out_motion = cf_mouse_wheel_motion();
		return out_motion != 0.0f;
	}
}

inline bool Input::IsKeyInState(CF_KeyButton key, KeyState state) noexcept {
	switch (state) {
	case KeyState::Up:
		return cf_key_just_released(key);
	case KeyState::Down:
		return cf_key_just_pressed(key);
	case KeyState::Hold:
		return cf_key_down(key) || cf_key_just_pressed(key);
	case KeyState::Hang:
		return cf_key_up(key) || cf_key_just_released(key);
	case KeyState::Repeatable:
		return cf_key_just_pressed(key) || cf_key_repeating(key);
	default:
		return false;
	}
}

// 遍历所有可能的按键，返回第一个触发“按下”的按键。
// 优先检测刚按下（cf_key_just_pressed），同时也包含重复触发（cf_key_repeating）。
// 若无按键触发，将 out_key 设为 CF_KEY_UNKNOWN 并返回 false。
inline bool Input::KeyDown(CF_KeyButton& out_key) noexcept {
	if (!cf_key_just_pressed(CF_KEY_ANY)) {
		out_key = CF_KEY_UNKNOWN;
		return false;
	}
	for (int i = 1; i < CF_KEY_COUNT; ++i) { // 从 1 开始跳过 CF_KEY_UNKNOWN(0)
		CF_KeyButton key = static_cast<CF_KeyButton>(i);
		if (key == CF_KEY_ANY) continue; // 跳过 CF_KEY_ANY
		if (cf_key_just_pressed(key)) {
			out_key = key;
			return true;
		}
	}
	out_key = CF_KEY_UNKNOWN;
	return false;
}

// 返回所有被按下/重复触发的按键，使用 std::bitset<CF_KEY_COUNT> 做为安全数组结构。
// out_keys 的索引与 CF_KeyButton 的整数值对应；索引 0 对应 CF_KEY_UNKNOWN（通常不使用）。
inline bool Input::KeysDown(std::bitset<CF_KEY_COUNT>& out_keys) noexcept {
	out_keys.reset();
	bool any = false;
	// 遍历所有键（跳过 CF_KEY_UNKNOWN 与 CF_KEY_ANY）
	for (int i = 1; i < CF_KEY_COUNT; ++i) {
		if (i == CF_KEY_ANY) continue;
		CF_KeyButton key = static_cast<CF_KeyButton>(i);
		if (cf_key_just_pressed(key) || cf_key_repeating(key)) {
			out_keys.set(static_cast<std::size_t>(i));
			any = true;
		}
	}
	return any;
}

// ----------------- 鼠标相关封装（与键盘接口风格一致） -----------------

inline bool Input::IsMouseInState(CF_MouseButton button, MouseState state) noexcept {
	switch (state) {
	case MouseState::Up:
		return cf_mouse_just_released(button);
	case MouseState::Down:
		return cf_mouse_just_pressed(button);
	case MouseState::Hold:
		return cf_mouse_down(button) || cf_mouse_just_pressed(button);
	case MouseState::Hang:
		return (!cf_mouse_down(button)) || cf_mouse_just_released(button);
	case MouseState::DoubleClick:
		return cf_mouse_double_clicked(button);
	case MouseState::DoubleClickAndHold:
		return cf_mouse_double_click_held(button);
	default:
		return false;
	}
}

// 遍历所有鼠标按钮，返回第一个刚按下的按钮。
// 若无按键触发，将 out_button 设为 CF_MOUSE_BUTTON_COUNT（或其他非法值）并返回 false。
inline bool Input::MouseDown(CF_MouseButton& out_button) noexcept {
	for (int i = 0; i < CF_MOUSE_BUTTON_COUNT; ++i) {
		CF_MouseButton b = static_cast<CF_MouseButton>(i);
		if (cf_mouse_just_pressed(b)) {
			out_button = b;
			return true;
		}
	}
	// 将 out_button 置为一个不可用值（使用枚举上界作为 sentinel）
	out_button = static_cast<CF_MouseButton>(CF_MOUSE_BUTTON_COUNT);
	return false;
}

// 返回所有被按下或刚按下的鼠标按钮，使用 std::bitset<CF_MOUSE_BUTTON_COUNT> 作为位集。
// out_buttons 的索引与 CF_MouseButton 的整数值对应。
inline bool Input::MouseButtonsDown(std::bitset<CF_MOUSE_BUTTON_COUNT>& out_buttons) noexcept {
	out_buttons.reset();
	bool any = false;
	for (int i = 0; i < CF_MOUSE_BUTTON_COUNT; ++i) {
		CF_MouseButton b = static_cast<CF_MouseButton>(i);
		// 这里把“刚按下”与“当前按下”都视为被按下（与 KeysDown 行为保持相近）
		if (cf_mouse_just_pressed(b) || cf_mouse_down(b)) {
			out_buttons.set(static_cast<std::size_t>(i));
			any = true;
		}
	}
	return any;
}