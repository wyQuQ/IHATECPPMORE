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

#include "player_object.h"
#include "backgroud.h"
#include "block_object.h"
#include "move_spike.h"
#include "spike.h"
#include "checkpoint.h"

static void print_debug_flags_once() {
	std::cerr << "MCG_DEBUG=" << MCG_DEBUG << " MCG_DEBUG_LEVEL=" << MCG_DEBUG_LEVEL << std::endl;
}

// 全局帧计数
std::atomic<int> g_frame_count{0};
int g_frame_rate = 50; // 目标帧率
// 多播委托：无参数、无返回值
Delegate<> main_thread_on_update;

int main(int argc, char* argv[])
{
	print_debug_flags_once();
	using namespace Cute;

	// 为简短调用创建引用别名
	auto& objs = ObjManager::Instance();

	// 窗口大小
	int window_width = 1024;
	int window_height = 720;
	// 创建应用程序窗口
	int options = CF_APP_OPTIONS_WINDOW_POS_CENTERED_BIT;
	CF_Result result = make_app("My I Wanna", 0, 0, 0, window_width, window_height, options, argv[0]);
	if (is_error(result)) return -1;

	float half_width = static_cast<float>(window_width) * 0.5f;
	float half_height = static_cast<float>(window_height) * 0.5f;

	// 挂载 content 目录到虚拟根 "/"，使资源可用为 "/sprites/idle.png"
	{
		CF_Path base = fs_get_base_directory();
		base.normalize();
		base += "/content";
		std::cerr << "[VFS] Mounting content directory: " << base.c_str() << " -> virtual root \"\"\n";
		fs_mount(base.c_str(), "");
	}

	// 使用 ObjManager 创建对象：现在返回 token（ObjectToken）
	auto player_token = objs.Create<PlayerObject>();
	auto spike_token = objs.Create<MoveSpike>();
	auto standing_spike_token = objs.Create<Spike>(CF_V2(154.0f, -324.0f));

#if TESTER
	auto tester_token = objs.Create<Tester>();
#endif

	// 创建背景对象
	auto background_token = objs.Create<Backgroud>();

	// 创建检查点对象
	auto checkpoint1_token = objs.Create<Checkpoint>(cf_v2(-400.0f, -308.0f));
	auto checkpoint2_token = objs.Create<Checkpoint>(cf_v2(400.0f, -308.0f));

	// 记录上一个（或默认） checkpoint 的位置，用于玩家复活/传送使用
	// -当前记默认位置为
	CF_V2 last_checkpoint_pos = cf_v2(-300.0f, 0.0f);

	// 创建方块对象。
	// -构造函数传参方式（位置、是否为草坪）
	auto block1_token = objs.Create<BlockObject>(cf_v2(-494.0f, -342.0f), true);
	auto block2_token = objs.Create<BlockObject>(cf_v2(-458.0f, -342.0f), true);
	auto block3_token = objs.Create<BlockObject>(cf_v2(-422.0f, -342.0f), true);
	auto block4_token = objs.Create<BlockObject>(cf_v2(-386.0f, -342.0f), true);
	auto block5_token = objs.Create<BlockObject>(cf_v2(-350.0f, -342.0f), false);
	auto block6_token = objs.Create<BlockObject>(cf_v2(-314.0f, -342.0f), false);
	auto block7_token = objs.Create<BlockObject>(cf_v2(-278.0f, -342.0f), false);
	auto block7_1_token = objs.Create<BlockObject>(cf_v2(-278.0f, -306.0f), false);
	auto block7_2_token = objs.Create<BlockObject>(cf_v2(-278.0f, -270.0f), false);
	auto block7_3_token = objs.Create<BlockObject>(cf_v2(-278.0f, -234.0f), true);
	auto block8_token = objs.Create<BlockObject>(cf_v2(-242.0f, -342.0f), true);
	auto block9_token = objs.Create<BlockObject>(cf_v2(-206.0f, -342.0f), true);
	auto block10_token = objs.Create<BlockObject>(cf_v2(-170.0f, -342.0f), false);
	auto block11_token = objs.Create<BlockObject>(cf_v2(-134.0f, -342.0f), false);
	auto block11_1_token = objs.Create<BlockObject>(cf_v2(-134.0f, -126.0f), false);
	auto block12_token = objs.Create<BlockObject>(cf_v2(-98.0f, -342.0f), true);
	auto block13_token = objs.Create<BlockObject>(cf_v2(-62.0f, -342.0f), true);
	auto block14_token = objs.Create<BlockObject>(cf_v2(-26.0f, -342.0f), false);
	auto block15_token = objs.Create<BlockObject>(cf_v2(10.0f, -342.0f), false);
	auto block16_token = objs.Create<BlockObject>(cf_v2(46.0f, -342.0f), true);
	auto block17_token = objs.Create<BlockObject>(cf_v2(82.0f, -342.0f), true);
	auto block17_1_token = objs.Create<BlockObject>(cf_v2(82.0f, -234.0f), true);
	auto block18_token = objs.Create<BlockObject>(cf_v2(118.0f, -342.0f), false);
	auto block18_1_token = objs.Create<BlockObject>(cf_v2(118.0f, -234.0f), true);
	auto block19_token = objs.Create<BlockObject>(cf_v2(154.0f, -342.0f), false);
	auto block19_1_token = objs.Create<BlockObject>(cf_v2(154.0f, -234.0f), true);
	auto block20_token = objs.Create<BlockObject>(cf_v2(190.0f, -342.0f), true);
	auto block21_token = objs.Create<BlockObject>(cf_v2(226.0f, -342.0f), true);
	auto block22_token = objs.Create<BlockObject>(cf_v2(262.0f, -342.0f), false);
	auto block23_token = objs.Create<BlockObject>(cf_v2(298.0f, -342.0f), false);
	auto block24_token = objs.Create<BlockObject>(cf_v2(334.0f, -342.0f), true);
	auto block25_token = objs.Create<BlockObject>(cf_v2(370.0f, -342.0f), true);
	auto block26_token = objs.Create<BlockObject>(cf_v2(406.0f, -342.0f), false);
	auto block27_token = objs.Create<BlockObject>(cf_v2(442.0f, -342.0f), false);
	auto block28_token = objs.Create<BlockObject>(cf_v2(478.0f, -342.0f), true);
	auto block29_token = objs.Create<BlockObject>(cf_v2(514.0f, -342.0f), true);

	auto update_token = main_thread_on_update.add([]() {
		ObjManager::Instance().UpdateAll();
		});

	cf_set_target_framerate(g_frame_rate);

	auto esc_hold_threshold = std::chrono::seconds(3);
	bool esc_was_down = false;
	std::chrono::steady_clock::time_point esc_down_start;

	bool debug_player_canvas_reported = false;

	while (app_is_running())
	{
		app_update();

		static bool _once = false;
		if (!_once) { std::cerr << "[LOOP] app_update OK\n-----\n"; _once = true; }

		//处理 R 键：瞬移或复活到上一个 checkpoint(利用last_checkpoint中存储的地址确定)
		if (Input::IsKeyInState(CF_KEY_R, KeyState::Down))
		{
			// 重要：若 player_token 是 pending（Create 返回的）则 IsValid 会返回 false，
			// 因此先尝试把可能的 pending token 升级为已注册 token（若已被合并）。
			// 非 const TryGetRegisteration 会在成功时用真实 token 覆盖 player_token。
			objs.TryGetRegisteration(player_token);

			// 若玩家存在（registered 或 pending 并已被合并），直接瞬移到上一个 checkpoint
			if (objs.IsValid(player_token))
			{

				try {
					// 找到上一个激活的的 checkpoint（registered）
					ObjManager::ObjToken nearest = objs.FindTokensByTag("checkpoint_active");
					if (nearest.isValid() && objs.IsValid(nearest)) {
						last_checkpoint_pos = objs[nearest].GetPosition() + CF_V2(0.0f, 30.0f);// 更新 last checkpoint 记录
					}
						// 未找到已合并的 checkpoint：退回到 last_checkpoint_pos（可能是启动时的默认）
						objs[player_token].SetPosition(last_checkpoint_pos);
						objs[player_token].SetVelocity(cf_v2(0.0f, 0.0f));
						objs[player_token].SetForce(cf_v2(0.0f, 0.0f));
				}
				catch (...) {
					std::cerr << "[Main] Teleport failed\n";
				}
			}
			else
			{
				try {
					// 玩家已被销毁或 token 无效：复活（Create 返回 pending token，可立即通过非 const operator[] 初始化）
					player_token = objs.Create<PlayerObject>();
					ObjManager::ObjToken nearest = objs.FindTokensByTag("checkpoint_active");
					if (nearest.isValid() && objs.IsValid(nearest)) {
						// 更新 last checkpoint 记录
						last_checkpoint_pos = objs[nearest].GetPosition() + CF_V2(0.0f, 30.0f);
					}
					if (player_token.isValid()) {
						// 将新创建（pending）玩家放到上一个激活的 checkpoint 位置
						objs[player_token].SetPosition(last_checkpoint_pos);
						objs[player_token].SetVelocity(cf_v2(0.0f, 0.0f));
						objs[player_token].SetForce(cf_v2(0.0f, 0.0f));
					}
				}
				catch (...) {
					std::cerr << "[Main] Respawn: failed to initialize new player\n";
				}
				
			}
		}

		// 处理 ESC 键：按住 3 秒退出程序
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
				{
					break;
				}
			}
		}
		else
		{
			esc_was_down = false;
		}

		g_frame_count++;
		main_thread_on_update();

		// 上传阶段（保持原样）
		try {
			DrawingSequence::Instance().DrawAll();
		} catch (const std::exception& ex) {
			std::cerr << "绘制异常 (upload): " << ex.what() << std::endl;
			break;
		}

		// ---- 你当前的测试绘制（参考方形 / 文本 等） ----
		{
			cf_draw_push();

			// 绿色中心点（容易辨识）
			cf_draw_push_color(cf_color_green());
			{
				cf_draw_circle2(cf_v2(0.0f, 0.0f), 8.0f, 0.0f);
			}
			cf_draw_pop_color();

			// 白色文本显示当前全局帧计数（若字体可用）
			cf_draw_translate(-half_width, -half_height);
			cf_draw_push_color(cf_color_white());
			cf_draw_text(("frame: " + std::to_string(g_frame_count.load())).c_str(), cf_v2(10.0f, 16.0f), -1);
			cf_draw_pop_color();

			cf_draw_pop();
		}

		// 退出提示与最终呈现
		if (esc_was_down)
		{
			cf_draw_push();
			cf_draw_translate(-half_width, half_height);

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

		// ---- 在测试绘制之后把已上传的 canvas 绘制到屏幕 ----
		try {
			DrawingSequence::Instance().BlitAll();
		} catch (const std::exception& ex) {
			std::cerr << "绘制异常 (blit): " << ex.what() << std::endl;
			break;
		}

		app_draw_onto_screen(true);
	}
	// 程序退出：由控制器销毁所有对象
	objs.DestroyAll();

	main_thread_on_update.clear();

	// 销毁应用程序
	destroy_app();

	return 0;
}