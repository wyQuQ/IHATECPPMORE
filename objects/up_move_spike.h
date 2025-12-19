#pragma once
#include "base_object.h"
#include "act_seq.h"

class UpMoveSpike : public BaseObject {
public:
	UpMoveSpike() noexcept : BaseObject() {}
	~UpMoveSpike() noexcept {}
	void Start() override;
	void Update() override;
	void OnCollisionStay(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept override;

private:
	ActSeq m_act_seq;
};