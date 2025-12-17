#pragma once
#include <vector>
#include "obj_manager.h"
#include "player_object.h"

class GlobalPlayer {
public:
	GlobalPlayer() noexcept {}
	~GlobalPlayer() noexcept {}

	// 创建单例实例的接口
	static GlobalPlayer& Instance() noexcept {
		static GlobalPlayer instance;
		return instance;
	}

	void SetRespawnPoint(CF_V2 pos) noexcept {
		respawn_point = pos;
	}

	void CreatePlayerAtRespawn() noexcept {
		auto player_token = ObjManager::Instance().Create<PlayerObject>(respawn_point);
	}

	void CreateCheckpointAtPosition(CF_V2 pos) noexcept {
		auto checkpoint_token = ObjManager::Instance().Create<Checkpoint>(pos);
	}

private:
	CF_V2 respawn_point = cf_v2(0.0f, 0.0f);
	std::vector<CF_V2> checkpoint_position;
	

};