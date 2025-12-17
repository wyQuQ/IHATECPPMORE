#include "room_loader.h"
#include "UI_draw.h"
#include "input.h"

#include "player_object.h"
#include "backgroud.h"
#include "block_object.h"
#include "move_spike.h"
#include "up_move_spike.h"
#include "down_move_spike.h"
#include "rotate_spike.h"
#include "spike.h"
#include "down_spike.h"
#include "lateral_spike.h"
#include "checkpoint.h"
#include "lateral_spike.h"

#include "globalplayer.h"

class TestRoom : public BaseRoom {
public:
	TestRoom() noexcept {}
	~TestRoom() noexcept override {}

	// 在这里添加房间加载逻辑
	void RoomLoad() override {
		OUTPUT({ "TestRoom" }, "RoomLoad called.");

		// 为简短调用创建引用别名
		auto& objs = ObjManager::Instance();
		auto& g_player = GlobalPlayer::Instance();

		// Ԥ����Ҹ����λ��
		g_player.SetRespawnPoint(cf_v2(-300.0f, 0.0f));

		// ʹ�� ObjManager �����������ڷ��� token��ObjectToken��
		g_player.CreatePlayerAtRespawn();
		auto checkpoint_token = objs.Create<Checkpoint>(CF_V2(-200.0f, -200.0f));
		auto spike_token = objs.Create<MoveSpike>();
		auto up_move_spike_token = objs.Create<UpMoveSpike>();
		auto down_move_spike_token = objs.Create<DownMoveSpike>();
		auto rotate_spike_token = objs.Create<RotateSpike>();
		auto standing_spike1_token = objs.Create<Spike>(CF_V2(154.0f, -324.0f));
		auto standing_down_spike1_token = objs.Create<DownSpike>(CF_V2(200.0f, -324.0f));
		auto standing_lateral_spike1_token = objs.Create<RightLateralSpike>(CF_V2(-150.0f, 324.0f));
		auto standing_lateral_spike2_token = objs.Create<LeftLateralSpike>(CF_V2(-150.0f, 0.0f));

		// 创建背景对象
		auto background_token = objs.Create<Backgroud>();

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
#if TESTER
		auto test_token = objs.Create<Tester>();
#endif
	}

	// 在这里添加房间更新逻辑
	void RoomUpdate() override {
		if (Input::IsKeyInState(CF_KEY_N, KeyState::Down)) {
			RoomLoader::Instance().Load("EmptyRoom");
		}

		if (Input::IsKeyInState(CF_KEY_R, KeyState::Down)) {
			RoomLoader::Instance().Load("TestRoom");
		}
	}

	// 在这里添加房间卸载逻辑
	void RoomUnload() override {
		OUTPUT({ "TestRoom" }, "RoomUnload called.");

	}
};

REGISTER_INITIAL_ROOM("TestRoom", TestRoom);