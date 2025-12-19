#pragma once
#include "base_object.h"

class End : public BaseObject {
public:
	End() noexcept : BaseObject() {}
	~End() noexcept {}

	void Start() override
	{
		SpriteSetSource("/sprites/end.png", 1);
		SetPosition(cf_v2(0.0f, 0.0f));
		SetColliderType(ColliderType::VOID);
		IsColliderRotate(false);
	}
};