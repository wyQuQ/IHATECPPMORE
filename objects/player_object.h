#pragma once
#include "base_object.h"
#include <iostream>
#include <cmath>
#include <unordered_map>

#include "debug_config.h"

class PlayerObject : public BaseObject {
public:
    PlayerObject(const CF_V2& pos) noexcept : BaseObject() , respawn_point(pos) {}
    ~PlayerObject() noexcept {}

    void Start() override;
    void Update() override;
	void StartFrame() override;
	void EndFrame() override;

	void OnExclusionSolid(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept override;
private:
	bool grounded = false;
	bool double_jump_ready = true;
	float jump_input_timer = 0.0f;
	// 记录上一个checkpoint（或默认复活点) 的位置，用于玩家复活/传送使用
	// -当前向量为默认位置：
	CF_V2 respawn_point;
};

#if TESTER
class Tester : public BaseObject {
	public:
	Tester() noexcept : BaseObject() {}
	~Tester() noexcept {}
	void Start() override;
	void Update() override;
	float tspeed = 1.6f;
};
#endif