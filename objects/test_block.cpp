#include "test_block.h"

void TestBlock::Start()
{
	// 仅设置贴图路径，其他参数使用默认值
	SpriteSetSource("/sprites/plate.png", 1);
	SetPosition(cf_v2(0.0f, -200.0f));
	SetColliderType(ColliderType::SOLID);
	ScaleX(3.0f);
}

void TestBlock::Update()
{
	// TestBlock 不需要每帧更新逻辑
}