#pragma once
#include<cute.h>
#include<algorithm>

namespace DrawUI {
	float half_w;
	float half_h;
	void GameOverDraw();
	void EscDraw(std::chrono::steady_clock::time_point esc_down_start, std::chrono::seconds esc_hold_threshold);
	void TestDraw();
}

void DrawUI::GameOverDraw() {
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

void DrawUI::EscDraw(std::chrono::steady_clock::time_point esc_down_start, std::chrono::seconds esc_hold_threshold) {
	cf_draw_push();
	cf_draw_translate(-half_w, half_h);

	std::stringstream ss;
	auto now = std::chrono::steady_clock::now();
	auto held = now - esc_down_start;
	double held_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(held).count();
	double threshold_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(esc_hold_threshold).count();
	ss << "hold ESC to exit ("
		<< std::fixed << std::setprecision(1) << held_seconds
		<< " / " << threshold_seconds << "s)";
	std::string s = ss.str();

	cf_draw_push_color(cf_color_white());
	cf_draw_text(s.c_str(), cf_v2(10.0f, -16.0f), -1);
	cf_draw_pop_color();

	cf_draw_pop();
}

void DrawUI::TestDraw() {
#if MESSAGE_DEBUG
	cf_draw_push();
#if TESTER
	// 绿色中心点（容易辨识）
	cf_draw_push_color(cf_color_green());
	{
		cf_draw_circle2(cf_v2(0.0f, 0.0f), 8.0f, 0.0f);
	}
	cf_draw_pop_color();
#endif

	// 白色文本显示当前帧率（FPS）
	cf_draw_translate(-half_w, -half_h);
	cf_draw_push_color(cf_color_white());

	// 计算0.5秒内的平均帧率
	static auto last_time = std::chrono::steady_clock::now();
	static int frame_count = 0;
	static float elapsed_time = 0.0f;
	static int displayed_fps = 0;

	auto current_time = std::chrono::steady_clock::now();
	float delta_time = std::chrono::duration<float>(current_time - last_time).count();
	last_time = current_time;

	frame_count++;
	elapsed_time += delta_time;

	if (elapsed_time >= 0.5f) {
		displayed_fps = static_cast<int>(frame_count / elapsed_time);
		frame_count = 0;
		elapsed_time = 0.0f;
	}

	std::string fps_text = "FPS: " + std::to_string(displayed_fps);
	cf_draw_text(fps_text.c_str(), cf_v2(10.0f, 16.0f), -1);

	cf_draw_pop_color();

	cf_draw_pop();
#endif
}