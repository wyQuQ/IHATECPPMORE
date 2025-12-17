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

class FirstRoom : public BaseRoom {
public:
	FirstRoom() noexcept {}
	~FirstRoom() noexcept override {}

	// 在这里添加房间加载逻辑
	void RoomLoad() override {
		OUTPUT({ "FirstRoom" }, "RoomLoad called.");

		// 为简短调用创建引用别名
		auto& objs = ObjManager::Instance();

		//创建玩家对象
		auto player_token = objs.Create<PlayerObject>();
		
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

		//手搓地图ing……
		auto bolck1_token = objs.Create<BlockObject>(cf_v2(-hw + 36 * 4, -hh + 36), false);
		auto bolck2_token = objs.Create<BlockObject>(cf_v2(-hw + 36 * 8, -hh + 36), false);
		auto bolck3_token = objs.Create<BlockObject>(cf_v2(-hw + 36 * 8, -hh + 36 * 2), false);
		auto bolck4_token = objs.Create<BlockObject>(cf_v2(-hw + 36 * 12, -hh + 36), false);
		auto bolck5_token = objs.Create<BlockObject>(cf_v2(-hw + 36 * 12, -hh + 36 * 2), false);
		auto bolck6_token = objs.Create<BlockObject>(cf_v2(-hw + 36 * 12, -hh + 36 * 3), false);
		auto bolck7_token = objs.Create<BlockObject>(cf_v2(-hw + 36 * 12, -hh + 36 * 4), false);
		auto spike1_token = objs.Create<Spike>(CF_V2(-hw + 36 * 16 + 18, -hh + 36));
		auto spike2_token = objs.Create<Spike>(CF_V2(-hw + 36 * 20 + 18, -hh + 36 * 3));
		auto bolck8_token = objs.Create<BlockObject>(cf_v2(-hw + 36 * 20, -hh + 36 * 1), false);
		auto bolck9_token = objs.Create<BlockObject>(cf_v2(-hw + 36 * 20, -hh + 36 * 2), false);
		auto bolck10_token = objs.Create<BlockObject>(cf_v2(-hw + 36 * 24, -hh + 36 * 4), false);
		auto down_spike1_token = objs.Create<DownSpike>(CF_V2(-hw + 36 * 24 + 18, -hh + 36 * 4));
		auto bolck11_token = objs.Create<BlockObject>(cf_v2(-hw + 36 * 25, -hh + 36 * 4), false);
		auto down_spike2_token = objs.Create<DownSpike>(CF_V2(-hw + 36 * 25 + 18, -hh + 36 * 4));
		auto bolck12_token = objs.Create<BlockObject>(cf_v2(-hw + 36 * 26, -hh + 36 * 4), false);
		auto down_spike3_token = objs.Create<DownSpike>(CF_V2(-hw + 36 * 26 + 18, -hh + 36 * 4));
		auto bolck13_token = objs.Create<BlockObject>(cf_v2(-hw + 36 * 27, -hh + 36 * 4), false);
		auto down_spike4_token = objs.Create<DownSpike>(CF_V2(-hw + 36 * 27 + 18, -hh + 36 * 4));
		auto bolck14_token = objs.Create<BlockObject>(cf_v2(-hw + 36 * 28, -hh + 36 * 4), false);
		auto down_spike5_token = objs.Create<DownSpike>(CF_V2(-hw + 36 * 28 + 18, -hh + 36 * 4));
		auto bolck15_token = objs.Create<BlockObject>(cf_v2(-hw + 36 * 29, -hh + 36 * 4), false);
		auto bolck16_token = objs.Create<BlockObject>(cf_v2(-hw + 36 * 30, -hh + 36 * 4), false);


#if TESTER
		auto test_token = objs.Create<Tester>();
#endif
	}

	// 在这里添加房间更新逻辑
	void RoomUpdate() override {
		if (Input::IsKeyInState(CF_KEY_P, KeyState::Down)) {
			RoomLoader::Instance().Load("TestRoom");
		}
	}

	// 在这里添加房间卸载逻辑
	void RoomUnload() override {
		OUTPUT({ "FirstRoom" }, "RoomUnload called.");

	}
};

REGISTER_INITIAL_ROOM("FirstRoom", FirstRoom);