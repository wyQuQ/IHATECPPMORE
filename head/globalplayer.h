#pragma once
#include "room_loader.h"
#include "obj_manager.h"
#include "player_object.h"
#include "blood.h"

// 管理全局玩家状态（复活点、出现点、技能等）的单例类
class GlobalPlayer {
public:
	GlobalPlayer() noexcept
		: respawn_room(RoomLoader::Instance().GetInitialRoom()) // 确保初始复活房间被设置
	{
	}

	~GlobalPlayer() noexcept {}

	// 获取单例实例
	static GlobalPlayer& Instance() noexcept {
		static GlobalPlayer instance;
		return instance;
	}

	// 设置当前复活点以及对应房间
	void SetRespawnPoint(CF_V2 pos) noexcept {
		respawn_point = pos;
		respawn_room = RoomLoader::Instance().GetCurrentRoom();
		has_record = true;
	}

	// 判断是否已有复活记录
	bool HasRespawnRecord() const noexcept { return has_record; }

	// 获取记录的复活房间指针
	const BaseRoom* GetRespawnRoom() const noexcept {
		return respawn_room;
	}

	// 在记录的点或者目标房间创建/移动玩家对象
	void Respawn() {
		if (respawn_room != RoomLoader::Instance().GetCurrentRoom()) {
			OUTPUT({ "GlobalPlayer::Respawn" }, "Warning: Respawning in a different room without loading it.");
		}
		if (!ObjManager::Instance().TryGetRegisteration(player_token)) {
			player_token = ObjManager::Instance().Create<PlayerObject>(respawn_point);
		}
		else {
			ObjManager::Instance()[player_token].SetPosition(respawn_point);
		}
	}

	// 设置下次出现的位置（不同于复活，通常用于剧情或特殊事件）
	void SetEmergePosition(CF_V2 pos) noexcept {
		emerge_pos.need_emerge = true;
		emerge_pos.position = pos;
	}

	// 处理玩家从出现点或复活点返回游戏的逻辑
	void Emerge() {
		if (!emerge_pos.need_emerge) Respawn();
		else if (!ObjManager::Instance().TryGetRegisteration(player_token)) {
			player_token = ObjManager::Instance().Create<PlayerObject>(emerge_pos.position);
		}
		else {
			ObjManager::Instance()[player_token].SetPosition(emerge_pos.position);
		}
		emerge_pos.need_emerge = false;
	}

	// 返回玩家对象标识符的引用，供外部系统使用
	ObjManager::ObjToken& Player() { return player_token; }

	// 触发玩家受伤效果：产生血迹并销毁当前玩家实例
	void Hurt() {
		if (!objs.TryGetRegisteration(player_token)) return;
		CF_V2 pos = objs[player_token].GetPosition();
		int amt = 8;
		float speed = 3.0f;
		for (int i = 0; i < amt; i++) {
			objs.Create<Blood>(pos, speed * v2math::get_dir(pi * 2 * i / amt));
		}
		objs.Destroy(player_token);
	}

private:
	// 记录玩家复活位置
	CF_V2 respawn_point = cf_v2(0.0f, 0.0f);
	const BaseRoom* respawn_room;
	bool has_record = false;

	// 储存当前玩家对象标识与出现点信息
	ObjManager::ObjToken player_token = ObjManager::ObjToken::Invalid();
	struct EmergePos {
		bool need_emerge = false;
		CF_V2 position = cf_v2(0.0f, 0.0f);
	} emerge_pos;
};