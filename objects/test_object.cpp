#include "test_object.h"

static constexpr float force = 0.5f;

void TestObject::Start()
{
	// 仅设置贴图路径，其他参数使用默认值
	SpriteSetSource("/sprites/plate.png", 1);
}

void TestObject::Update()
{
	// 使用 BaseObject 暴露的物理接口来修改速度/受力：
	// 1) 根据按键添加力（SetForce）
	// 2) 将力积分到速度（ApplyForce）
	// 3) 可选阻尼（SetVelocity）
	// 4) 将速度积分到位置（ApplyVelocity）

	int dir = 0;
	if (cf_key_down(CF_KEY_LEFT)) {
		dir--;
	}
	if (cf_key_down(CF_KEY_RIGHT)) {
		dir++;
	}

	// 添加力
	SetForce(dir * v2math::angled(CF_V2(force), GetRotation()));

	// 简单水平方向阻尼：无按键时减速，避免无限滑动
	// 这里使用已有的速度接口来读取/写入速度
	CF_V2 vel = GetVelocity();
	constexpr float damping = 0.90f;
	if (!cf_key_down(CF_KEY_LEFT) && !cf_key_down(CF_KEY_RIGHT)) {
		vel *= damping;
		SetVelocity(vel);
	}

<<<<<<< Updated upstream
	// ����ס�ո�ʱ��ʹ�����岻�ɼ�
	if (cf_key_down(CF_KEY_W)) {
		SetVisible(false);
	} else {
		SetVisible(true);
	}
}
=======

}
>>>>>>> Stashed changes
