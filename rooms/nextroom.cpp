#include "room_loader.h"
#include "globalplayer.h"
#include "UI_draw.h"
#include "input.h"

#include "player_object.h"
#include "backgroud.h"
#include "checkpoint.h"
#include "block_object.h"
#include "spike.h"
#include "lateral_spike.h"
#include "straight_cherry.h"
#include "hidden_block.h"
#include "hidden_spike.h"

class NextRoom : public BaseRoom {
public:
	NextRoom() noexcept {}
	~NextRoom() noexcept override {}

	// 在这里添加房间加载逻辑
	void RoomLoad() override {
		OUTPUT({ "NextRoom" }, "RoomLoad called.");

		auto& g = GlobalPlayer::Instance();
		float hw = DrawUI::half_w;
		float hh = DrawUI::half_h;

		// 创建背景对象
		auto background_token = objs.Create<Backgroud>();

		if (!g.HasRespawnRecord())g.SetRespawnPoint(cf_v2(-hw + 36 * 2, -hh + 36 * 2));
		g.Emerge();

		for (float y = -hh + 4 * 36.0f; y < hh; y += 36) {
			objs.Create<BlockObject>(cf_v2(-hw, y), false);
		}
		for (float y = -hh; y < hh; y += 36) {
			objs.Create<BlockObject>(cf_v2(hw - 36.0f, y), false);
		}
		for (float x = -hw + 36; x < hw - 36; x += 36) {
			objs.Create<BlockObject>(cf_v2(x, hh - 36.0f), false);
		}
		for (float x = -hw; x < hw - 36; x += 36) {
			objs.Create<BlockObject>(cf_v2(x, -hh), true);
		}

		//手搓地图ing……
		float w = 36.0f;
		float half = 18.0f;

		// 刺之海
		objs.Create<Spike>(cf_v2(-6 * w + half, -11 * w));
		objs.Create<Spike>(cf_v2(-5 * w + half, -11 * w));
		objs.Create<Spike>(cf_v2(-4 * w + half, -11 * w));
		objs.Create<Spike>(cf_v2(-3 * w + half, -11 * w));
		objs.Create<Spike>(cf_v2(-2 * w + half, -11 * w));
		objs.Create<Spike>(cf_v2(-1 * w + half, -11 * w));
		objs.Create<Spike>(cf_v2(0 * w + half, -11 * w));
		objs.Create<Spike>(cf_v2(1 * w + half, -11 * w));
		objs.Create<Spike>(cf_v2(2 * w + half, -11 * w));
		objs.Create<Spike>(cf_v2(3 * w + half, -11 * w));
		objs.Create<Spike>(cf_v2(4 * w + half, -11 * w));
		objs.Create<Spike>(cf_v2(5 * w + half, -11 * w));
		objs.Create<Spike>(cf_v2(6 * w + half, -11 * w));
		objs.Create<Spike>(cf_v2(7 * w + half, -11 * w));
		objs.Create<Spike>(cf_v2(8 * w + half, -11 * w));
		objs.Create<Spike>(cf_v2(9 * w + half, -11 * w));
		objs.Create<Spike>(cf_v2(10 * w + half, -11 * w));
		objs.Create<Spike>(cf_v2(11 * w + half, -11 * w));
		objs.Create<Spike>(cf_v2(12 * w + half, -11 * w));
		objs.Create<Spike>(cf_v2(13 * w + half, -11 * w));
		objs.Create<Spike>(cf_v2(14 * w + half, -11 * w));
		
		// 初始垫脚块
		objs.Create<BlockObject>(cf_v2(-15 * w, -9 * w), true);

		// 奇形障碍
		objs.Create<BlockObject>(cf_v2(-14 * w, -6 * w), true);
		objs.Create<BlockObject>(cf_v2(-13 * w, -6 * w), true);
		objs.Create<BlockObject>(cf_v2(-12 * w, -7 * w), true);
		objs.Create<Spike>(cf_v2(-12 * w + half, -5 * w));

		// 带尖刺的落脚点
		objs.Create<BlockObject>(cf_v2(-6 * w, -8 * w), true);
		objs.Create<LeftLateralSpike>(cf_v2(-6 * w, -8 * w + half));
		objs.Create<RightLateralSpike>(cf_v2(-5 * w, -8 * w + half));

		// 齿形双平台
		objs.Create<BlockObject>(cf_v2(-3 * w, -7 * w), true);
		objs.Create<BlockObject>(cf_v2(-2 * w, -7 * w),true);
		objs.Create<BlockObject>(cf_v2(-1 * w, -7 * w),true);
		objs.Create<BlockObject>(cf_v2(-3 * w, -3 * w), true);
		objs.Create<BlockObject>(cf_v2(-2 * w, -3 * w), true);
		objs.Create<BlockObject>(cf_v2(-1 * w, -3 * w), true);
		objs.Create<HiddenSpike>(cf_v2(-3 * w + half, -2 * w), 3, false);
		objs.Create<HiddenSpike>(cf_v2(-2 * w + half, -2 * w), 3, false);
		objs.Create<HiddenSpike>(cf_v2(-1 * w + half, -2 * w), 3, false);

		// 落脚点
		objs.Create<BlockObject>(cf_v2(3 * w + half, -7 * w + half), true);

		// 坑点
		objs.Create<BlockObject>(cf_v2(6 * w, -6 * w), true);
		objs.Create<BlockObject>(cf_v2(7 * w, -6 * w), true);
		objs.Create<BlockObject>(cf_v2(8 * w, -6 * w), true);
		objs.Create<HiddenSpike>(cf_v2(7 * w + half, -6 * w), 2, true);
		objs.Create<HiddenSpike>(cf_v2(8 * w + half, -6 * w), 2, true);

		// 藏刺小平台
		objs.Create<BlockObject>(cf_v2(10 * w, -4 * w), true);
		objs.Create<BlockObject>(cf_v2(11 * w, -4 * w), true);
		objs.Create<BlockObject>(cf_v2(12 * w, -4 * w), true);
		objs.Create<HiddenSpike>(cf_v2(11 * w + half, -4 * w), 2, true);

		// 移动苹果
		objs.Create<StraightCherry>(cf_v2(9 * w + half, 6 * w + half), 6 * w, false);
		objs.Create<StraightCherry>(cf_v2(10 * w + half, -1 * w + half), 6 * w, true);
		objs.Create<StraightCherry>(cf_v2(12 * w + half, -1 * w + half), 6 * w, true);
		objs.Create<StraightCherry>(cf_v2(13 * w + half, 6 * w + half), 6 * w, false);

		// 左跳块序列
		objs.Create<BlockObject>(cf_v2(8 * w, -1 * w), true);
		objs.Create<BlockObject>(cf_v2(8 * w, 1 * w), true);
		objs.Create<BlockObject>(cf_v2(8 * w, 3 * w), true);
		objs.Create<BlockObject>(cf_v2(8 * w, 5 * w), true);

		// 中跳块序列
		objs.Create<BlockObject>(cf_v2(11 * w, 0 * w), true);
		objs.Create<BlockObject>(cf_v2(11 * w, 2 * w), true);
		objs.Create<BlockObject>(cf_v2(11 * w, 4 * w), true);
		objs.Create<BlockObject>(cf_v2(11 * w, 6 * w), true);
		objs.Create<HiddenSpike>(cf_v2(11 * w + half, 6 * w), 1, true);

		// 右跳块序列
		objs.Create<BlockObject>(cf_v2(14 * w, -1 * w), true);
		objs.Create<BlockObject>(cf_v2(14 * w, 1 * w), true);
		objs.Create<BlockObject>(cf_v2(14 * w, 3 * w), true);
		objs.Create<BlockObject>(cf_v2(14 * w, 5 * w), true);

		// 顶部短平台
		objs.Create<BlockObject>(cf_v2(5 * w, 5 * w), true);
		objs.Create<BlockObject>(cf_v2(6 * w, 5 * w), true);
		objs.Create<BlockObject>(cf_v2(7 * w, 5 * w), true);

		// “出口”左侧长平台
		objs.Create<BlockObject>(cf_v2(5 * w, 8 * w), true);
		objs.Create<BlockObject>(cf_v2(6 * w, 8 * w), true);
		objs.Create<BlockObject>(cf_v2(7 * w, 8 * w), true);
		objs.Create<BlockObject>(cf_v2(8 * w, 8 * w), true);
		objs.Create<BlockObject>(cf_v2(9 * w, 8 * w), true);
		objs.Create<BlockObject>(cf_v2(10 * w, 8 * w), true);

		// “出口”右侧短平台
		objs.Create<BlockObject>(cf_v2(12 * w, 8 * w), true);
		objs.Create<BlockObject>(cf_v2(13 * w, 8 * w), true);
		objs.Create<BlockObject>(cf_v2(14 * w, 8 * w), true);

		// 隐藏方块四连
		objs.Create<HiddenBlock>(cf_v2(5 * w, 6 * w));
		objs.Create<HiddenBlock>(cf_v2(5 * w, 7 * w));
		objs.Create<HiddenBlock>(cf_v2(6 * w, 6 * w));
		objs.Create<HiddenBlock>(cf_v2(6 * w, 7 * w));

		// 带刺小路
		objs.Create<BlockObject>(cf_v2(4 * w, 2 * w), true);
		objs.Create<BlockObject>(cf_v2(5 * w, 2 * w), true);
		objs.Create<HiddenSpike>(cf_v2(5 * w + half, 2 * w), 2, true);

		// 带刺明线
		objs.Create<BlockObject>(cf_v2(0 * w, 4 * w), true);
		objs.Create<HiddenSpike>(cf_v2(0 * w + half, 4 * w), 2, true);

		// 上方拦截隐藏块
		objs.Create<HiddenBlock>(cf_v2(0 * w, 7 * w));
		objs.Create<HiddenBlock>(cf_v2(0 * w, 8 * w));
		objs.Create<HiddenBlock>(cf_v2(1 * w, 8 * w));
		objs.Create<HiddenBlock>(cf_v2(2 * w, 8 * w));
		objs.Create<HiddenBlock>(cf_v2(3 * w, 8 * w));
		objs.Create<HiddenBlock>(cf_v2(4 * w, 8 * w));

		// 中间拦截隐藏块
		objs.Create<HiddenBlock>(cf_v2(0 * w, 3 * w));
		objs.Create<HiddenBlock>(cf_v2(0 * w, 2 * w));
		objs.Create<HiddenBlock>(cf_v2(0 * w, 1 * w));
		objs.Create<HiddenBlock>(cf_v2(0 * w, 0 * w));
		objs.Create<HiddenBlock>(cf_v2(0 * w, -1 * w));
		objs.Create<HiddenBlock>(cf_v2(0 * w, -2 * w));

		// 死路
		objs.Create<HiddenBlock>(cf_v2(-1 * w, 4 * w));
		objs.Create<HiddenBlock>(cf_v2(-2 * w, 4 * w));
		objs.Create<HiddenBlock>(cf_v2(-1 * w, 6 * w));
		objs.Create<HiddenBlock>(cf_v2(-2 * w, 6 * w));
		objs.Create<HiddenBlock>(cf_v2(-3 * w, 5 * w));

		// 隐藏的登神长阶

		objs.Create<HiddenBlock>(cf_v2(-1 * w, 0 * w));
		objs.Create<HiddenBlock>(cf_v2(-2 * w, 0 * w));
		objs.Create<HiddenBlock>(cf_v2(-3 * w, 0 * w));

		objs.Create<HiddenBlock>(cf_v2(-4 * w, 2 * w));
		objs.Create<HiddenBlock>(cf_v2(-5 * w, 3 * w));
		objs.Create<HiddenBlock>(cf_v2(-6 * w, 4 * w));

#if TESTER
		auto test_token = objs.Create<Tester>();
#endif
	}

	// 在这里添加房间更新逻辑
	void RoomUpdate() override {
		auto& g = GlobalPlayer::Instance();
		if (objs.TryGetRegisteration(g.Player()) && objs[g.Player()].GetPosition().x < -DrawUI::half_w) {
			g.SetEmergePosition(CF_V2(DrawUI::half_w - 36 * 0.3f, objs[g.Player()].GetPosition().y));
			RoomLoader::Instance().Load("FirstRoom");
		}
		if (Input::IsKeyInState(CF_KEY_N, KeyState::Down)) {
			GlobalPlayer::Instance().SetEmergePosition(CF_V2(-DrawUI::half_w + 36 * 1.5f, -DrawUI::half_h + 36 * 2));
			RoomLoader::Instance().Load("EndRoom");
		}
	}

	// 在这里添加房间卸载逻辑
	void RoomUnload() override {
		OUTPUT({ "NextRoom" }, "RoomUnload called.");

	}
};

REGISTER_ROOM("NextRoom", NextRoom);