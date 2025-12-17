#include <chrono>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <atomic>
#include <string>
#include <iomanip>

#include "debug_config.h"
#include "delegate.h"
#include "base_object.h"
#include "drawing_sequence.h"
#include "obj_manager.h"
#include "UI_draw.h"
#include "room_loader.h"

// 全局变量：
// 全局帧计数
std::atomic<int> g_frame_count{0};
// 多播委托：无参数、无返回值
Delegate<> main_thread_on_update;
// 全局帧率（每秒帧数）
int g_frame_rate = 50;

int main(int argc, char* argv[])
{
	//--------------------------初始化应用程序--------------------------
	using namespace Cute;
	// 打印 debug 配置
	OUTPUT({"Main"}, "MCG_DEBUG =", MCG_DEBUG, " MCG_DEBUG_LEVEL =", MCG_DEBUG_LEVEL);

	// 窗口大小
	int window_width = 1152;
	int window_height = 864;

	// 创建应用程序窗口
	int options = CF_APP_OPTIONS_WINDOW_POS_CENTERED_BIT;
	CF_Result result = make_app("My I Wanna", 0, 0, 0, window_width, window_height, options, argv[0]);
	if (is_error(result)) return -1;

	// 记录窗口半宽高，用于 UI 绘制
	DrawUI::half_w = static_cast<float>(window_width) * 0.5f;
	DrawUI::half_h = static_cast<float>(window_height) * 0.5f;
	{
		// 挂载 content 目录到虚拟根 "/"，使资源可用为 "/sprites/idle.png"
		CF_Path base = fs_get_base_directory();
		base.normalize();
		base += "/content";
		OUTPUT({"VFS"}, "Mounting content directory:", base.c_str(), "-> virtual root \"\"");
		fs_mount(base.c_str(), "");
	}

	// 设置目标帧率
	cf_set_target_framerate(g_frame_rate);

	// 生成初始房间
	RoomLoader::Instance().LoadInitial();

	// esc 键长按退出相关参数
	auto esc_hold_threshold = std::chrono::seconds(3);
	bool esc_was_down = false;
	std::chrono::steady_clock::time_point esc_down_start;

	// 游戏结束显示相关参数
	bool game_over = false;
	std::chrono::steady_clock::time_point game_over_start;

	//--------------------------主循环--------------------------
	while (app_is_running())
	{
		// 一次性日志：主循环成功启动，app_update 可用
		static bool _once = false;
		if (!_once) { OUTPUT({"LOOP"}, "app_update OK, current frame rate:", g_frame_rate); _once = true; }

		//--------------------更新阶段--------------------
		// 全局帧计数递增
		g_frame_count++;
		// 调用 Cute Framework 更新
		app_update();
		// 调用主线程更新委托
		main_thread_on_update(); 
		// 更新所有对象（物理积分/碰撞检测/行为更新等）
		objs.UpdateAll();
		// 更新当前房间
		RoomLoader::Instance().UpdateCurrent();
		
		// 处理 ESC 键：计时退出
		if (cf_key_down(CF_KEY_ESCAPE))
		{
			auto now = std::chrono::steady_clock::now();
			if (!esc_was_down)
			{
				esc_was_down = true;
				esc_down_start = now;
			}
			else
			{
				auto held = now - esc_down_start;
				if (held >= esc_hold_threshold)
					break; // 退出主循环
			}
		}
		else
		{
			// 重置 ESC 状态
			esc_was_down = false;
		}

		//--------------------绘制阶段--------------------
		try {
			DrawingSequence::Instance().DrawAll();
		} catch (const std::exception& ex) {
			OUTPUT({"Draw"}, "绘制异常 (upload):", ex.what());
			break;
		}
		// ---- 你当前的测试绘制（参考方形 / 文本 等） ----
		DrawUI::TestDraw();
		// 如果处于 game over 状态，绘制覆盖提示（屏幕中央），放大并居中，行间插分割线
		if (game_over) {
			const char* title = "GAME OVER";
			const char* hint = "Press R to continue";

			// 放大倍数（可调）
			const float scale_mult = 1.8f;

			// 基准字号（像素），根据需要调整
			const float base_title_size = 48.0f;
			const float base_hint_size = 20.0f;
			float title_size = base_title_size * scale_mult;
			float hint_size = base_hint_size * scale_mult;

			// push font size -> cf_text_size 会使用当前 font size
			cf_push_font_size(title_size);
			CF_V2 title_sz = cf_text_size(title, -1);
			cf_pop_font_size();

			cf_push_font_size(hint_size);
			CF_V2 hint_sz = cf_text_size(hint, -1);
			cf_pop_font_size();

			float max_w = std::max(title_sz.x, hint_sz.x);
			// 行 Y 偏移（以像素为单位，屏幕中心为原点，向上为正）
			const float title_y = title_sz.y;
			const float hint_y = -12.0f;
			const float separator_y = 0;

			// 绘制半透明背景遮罩（提高对比度）
			cf_draw_push();
			cf_draw_push_color(cf_color_black());
			// 绘制稍大矩形作为遮罩
			CF_Aabb mask = cf_make_aabb(cf_v2(-max_w * 0.6f, hint_y - 40.0f), cf_v2(max_w * 0.6f, title_y + 20.0f));
			cf_draw_quad_fill(mask, 0.0f);
			cf_draw_pop_color();
			cf_draw_pop();

			// 绘制主标题
			cf_push_font_size(title_size);
			cf_draw_push_color(cf_color_yellow());
			cf_draw_text(title, cf_v2(-title_sz.x * 0.5f, title_y), -1);
			cf_draw_pop_color();
			cf_pop_font_size();

			// 绘制分割线（使用 draw_line，厚度可调）
			float line_x0 = -max_w * 0.5f;
			float line_x1 = max_w * 0.5f;
			cf_draw_push_color(cf_color_white());
			cf_draw_line(cf_v2(line_x0, separator_y), cf_v2(line_x1, separator_y), 4.0f);
			cf_draw_pop_color();

			// 绘制提示文字
			cf_push_font_size(hint_size);
			cf_draw_push_color(cf_color_white());
			cf_draw_text(hint, cf_v2(-hint_sz.x * 0.5f, hint_y), -1);
			cf_draw_pop_color();
			cf_pop_font_size();
		}

		// 退出提示与最终呈现
		if (esc_was_down) { DrawUI::EscDraw(esc_down_start, esc_hold_threshold); }

		app_draw_onto_screen(true);
	}

	// 程序退出：
	// 由控制器销毁所有对象
	objs.DestroyAll();
	// 清理主线程更新委托
	main_thread_on_update.clear();
	// 销毁应用程序
	destroy_app();

	return 0;
}