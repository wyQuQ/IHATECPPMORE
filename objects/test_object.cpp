#include "test_object.h"

static constexpr float force = 0.5f;

void TestObject::Start()
{
	// 仅设置贴图路径，其他参数使用默认值
	SpriteSetSource("/sprites/plate.png", 1);
	// 可选：初始化位置（根据需要调整）
	SetPosition(cf_v2(0.0f, -200.0f));
	// 设置碰撞箱为固体
	SetColliderType(ColliderType::SOLID);
}

void TestObject::Update()
{
	// 使用 BaseObject 暴露的物理接口来修改速度/受力：
	// 1) 根据按键添加力（SetForce）
	// 3) 可选阻尼（SetVelocity）

	CF_V2 inputForce = cf_v2(0.0f, 0.0f);
	if (cf_key_down(CF_KEY_LEFT)) {
		inputForce.x -= force;
	}
	if (cf_key_down(CF_KEY_RIGHT)) {
		inputForce.x += force;
	}

	// 添加力
	SetForce(inputForce);

	// 简单水平方向阻尼：无按键时减速，避免无限滑动
	// 这里使用已有的速度接口来读取/写入速度
	CF_V2 vel = GetVelocity();
	constexpr float damping = 0.90f;
	if (!cf_key_down(CF_KEY_LEFT) && !cf_key_down(CF_KEY_RIGHT)) {
		vel.x *= damping;
		SetVelocity(vel);
	}

<<<<<<< Updated upstream
	// 当按住空格时，使该物体不可见
	if (cf_key_down(CF_KEY_W)) {
		SetVisible(false);
	} else {
		SetVisible(true);
	}
}
=======

}
>>>>>>> Stashed changes
