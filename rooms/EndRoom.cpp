#include "room_loader.h"
#include "globalplayer.h"
#include "UI_draw.h"
#include "input.h"

#include "player_object.h"
#include "backgroud.h"
#include "block_object.h"
#include "end.h"

class EndRoom : public BaseRoom {
public:
	EndRoom() noexcept {}
	~EndRoom() noexcept override {}

	// 在这里添加房间加载逻辑
	void RoomLoad() override {
		OUTPUT({ "EndRoom" }, "RoomLoad called.");

		auto& g = GlobalPlayer::Instance();
		float hw = DrawUI::half_w;
		float hh = DrawUI::half_h;

		// 创建背景对象
		auto Background_token = objs.Create<Backgroud>();
		auto End_token = objs.Create<End>();

		if (!g.HasRespawnRecord())g.SetRespawnPoint(cf_v2(-hw + 36 * 2, -hh + 36 * 2));
		g.Emerge();

		for (float x = -hw; x < hw; x += 36) {
			objs.Create<BlockObject>(cf_v2(x, -hh), true);
		}



#if TESTER
		auto test_token = objs.Create<Tester>();
#endif
	}

	// 在这里添加房间更新逻辑
	void RoomUpdate() override {

	}

	// 在这里添加房间卸载逻辑
	void RoomUnload() override {
		OUTPUT({ "EndRoom" }, "RoomUnload called.");

	}
};

REGISTER_INITIAL_ROOM("EndRoom", EndRoom);