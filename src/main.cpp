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

#include "player_object.h"
#include "backgroud.h"
#include "block_object.h"
#include "move_spike.h"
#include "spike.h"
#include "checkpoint.h"

// 全局变量：
// 全局帧计数
std::atomic<int> g_frame_count{0};
// 全局帧率（每秒帧数）
int g_frame_rate = 50; 
// 多播委托：无参数、无返回值
Delegate<> main_thread_on_update;

int main(int argc, char* argv[])
{
	//--------------------------初始化应用程序--------------------------
	using namespace Cute;
	// 打印 debug 配置
	OUTPUT({"Main"}, "MCG_DEBUG=", MCG_DEBUG, " MCG_DEBUG_LEVEL=", MCG_DEBUG_LEVEL);

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
		OUTPUT({"VFS"}, "Mounting content directory: ", base.c_str(), " -> virtual root \"\"");
		fs_mount(base.c_str(), "");
	}

	// 设置目标帧率
	cf_set_target_framerate(g_frame_rate);

	//--------------------------创建对象--------------------------
	// 为简短调用创建引用别名
	auto& objs = ObjManager::Instance();

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
	float hw = DrawUI::half_w;
	float hh = DrawUI::half_h;
	for (float y = -hh; y < hh; y += 36) {
		objs.Create<BlockObject>(cf_v2(-hw, y), false);
	}
	for (float y = -hh; y < hh; y += 36) {
		objs.Create<BlockObject>(cf_v2(hw - 36.0f, y), false);
	}
	for (float x = -hw + 36; x < hw - 36; x += 36) {
		objs.Create<BlockObject>(cf_v2(x, hh - 36.0f), false);
	}
	for (float x = -hw + 36; x < hw - 36; x += 36) {
		objs.Create<BlockObject>(cf_v2(x, -hh), true);
	}

	// 注册主线程更新委托：每帧调用 ObjManager::UpdateAll()
	auto update_token = main_thread_on_update.add([]() {
		ObjManager::Instance().UpdateAll();
		});
	// ------------------------对象创建结束------------------------


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
		if (!_once) { OUTPUT({"LOOP"}, "app_update OK"); _once = true; }

		//--------------------更新阶段--------------------
		// 全局帧计数递增
		g_frame_count++;
		// 调用 Cute Framework 更新
		app_update();
		// 调用主线程更新委托
		main_thread_on_update(); 

		//---------------------- 输入处理阶段 --------------------
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

						// 若已存在玩家，取消 game over 显示，等待下一次死亡
						game_over = false;
				}
				catch (...) {
					OUTPUT({"Main"}, "Teleport failed");
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

						// 玩家复活，取消 game over 显示，等待下一次死亡
						game_over = false;
					}
				}
				catch (...) {
					OUTPUT({"Main"}, "Respawn: failed to initialize new player");
				}
				
			}
		}

		// 检测玩家是否已“死亡”（player_token 无效）
		// 注意：TryGetRegisteration 会在需要时将 pending token 升级为真实 token，或把无效 registered token 置为 Invalid。
		objs.TryGetRegisteration(player_token);
		if (!player_token.isValid() && !game_over) {
			game_over = true;
			game_over_start = std::chrono::steady_clock::now();
			OUTPUT({"Main"}, "Player died -> GAME OVER");
		}

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
		// 上传阶段（保持原样）
		try {
			DrawingSequence::Instance().DrawAll();
		} catch (const std::exception& ex) {
			OUTPUT({"Draw"}, "绘制异常 (upload): ", ex.what());
			break;
		}
		// ---- 你当前的测试绘制（参考方形 / 文本 等） ----
		DrawUI::TestDraw();
		// 如果处于 game over 状态，绘制覆盖提示（屏幕中央），放大并居中，行间插分割线
		if (game_over) { DrawUI::GameOverDraw(); }
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