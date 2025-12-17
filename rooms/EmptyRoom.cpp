#include "room_loader.h"
#include "UI_draw.h"
#include "input.h"

#include "player_object.h"
#include "backgroud.h"
#include "block_object.h"
#include "move_spike.h"
#include "up_move_spike.h"
#include "down_move_spike.h"
#include "spike.h"
#include "down_spike.h"
#include "checkpoint.h"

class EmptyRoom : public BaseRoom {
public:
	EmptyRoom() noexcept {}
	~EmptyRoom() noexcept override {}

	void RoomLoad() override {
		OUTPUT({ "EmptyRoom" }, "RoomLoad called.");

		// 为简短调用创建引用别名
		auto& objs = ObjManager::Instance();
		
		float hw = DrawUI::half_w;
		float hh = DrawUI::half_h;

		// 使用 ObjManager 创建对象：现在返回 token（ObjectToken）
		auto player_token = objs.Create<PlayerObject>(-hw + 36.0f,-hh + 72.0f);

		// 创建背景对象
		auto background_token = objs.Create<Backgroud>();

		// 记录上一个（或默认） checkpoint 的位置，用于玩家复活/传送使用
		// -当前记默认位置为
		CF_V2 last_checkpoint_pos = cf_v2(-300.0f, 0.0f);

		// 创建方块对象。
		// -构造函数传参方式（位置、是否为草坪）
		

		//第一列下方的方块
		for (float y = -hh; y < hh - 7 * 36; y += 72) {
			objs.Create<BlockObject>(cf_v2(-hw, y), false);
		}

		//第三列下方的方块
		for (float y = -hh; y < hh - 7 * 36; y += 72) {
			objs.Create<BlockObject>(cf_v2(-hw + 72, y), false);
		}

		//第四列的连续下方的方块
		for (float y = -hh; y < hh - 6 * 36; y += 36) {
			objs.Create<BlockObject>(cf_v2(-hw + 108, y), false);
		}

		//(-15,-16）的单独一个方块
		objs.Create<BlockObject>(cf_v2(-hw + 36, -hh), false);
		
	}

	void RoomUpdate() override {
		if (Input::IsKeyInState(CF_KEY_P, KeyState::Down)) {
			RoomLoader::Instance().Load("FirstRoom");
		}
	}
	void RoomUnload() override {
		OUTPUT({ "TestRoom" }, "RoomUnload called.");
	}
};

REGISTER_ROOM("EmptyRoom", EmptyRoom);
