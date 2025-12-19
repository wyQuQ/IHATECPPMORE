#pragma once
#include "base_object.h"
#include "act_seq.h"

class RotateSpike : public BaseObject {
public:
	RotateSpike() noexcept : BaseObject() {}
	RotateSpike(const CF_V2& pos) : BaseObject(), initial_pos(pos) {}
	~RotateSpike() noexcept {}
	void Start() override;
	void Update() override;
	void OnCollisionStay(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept override;

private:
	ActSeq m_act_seq;
	CF_V2 initial_pos{0.0f, 0.0f};
};