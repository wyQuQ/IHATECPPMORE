#include "player_object.h"

// 每帧移动的速度
static constexpr float speed = 3.0f;
const float PI = 3.14159265358979f;

void PlayerObject::Start()
{
	// 统一设置贴图路径、竖排帧数、动画更新频率和绘制深度，并注册到绘制序列
	// 如需要默认值，请使用高粒度的 SetSprite*() 和 Set*() 方法逐一设置非默认值参数
	// 资源路径无默认值，必须手动设置
    SpriteSetStats("/sprites/idle.png", 3, 7, 0);

    // 可选：初始化位置（根据需要调整），例如屏幕中心附近
    SetPosition(cf_v2(0.0f, 0.0f));
}

void PlayerObject::Update()
{
	// 当检测到按键按下时，设置速度方向（不直接 SetPosition，使用速度积分）
    CF_V2 dir(0,0);
    if (cf_key_down(CF_KEY_A)) {
        dir.x -= 1;
    }
    if (cf_key_down(CF_KEY_D)) {
		dir.x += 1;
    }
    if (cf_key_down(CF_KEY_W)) {
		dir.y += 1;
    }
    if (cf_key_down(CF_KEY_S)) {
		dir.y -= 1;
    }
    if (dir.x != 0) {
		SpriteFlipX(dir.x < 0); // 根据水平移动方向翻转贴图
        SpriteSetStats("/sprites/walk.png", 2, 7, 0, false);
    }
    else {
		SpriteSetStats("/sprites/idle.png", 3, 7, 0, false);
    }
	// 归一化速度向量以防对角移动过快
	dir = v2math::normalized(dir);
    CF_V2 vel = cf_v2(dir.x * speed, dir.y * speed);

    // 使用物理接口设置速度，让 FramelyUpdate 的 ApplyVelocity 去移动并保存 prev_position
    SetVelocity(vel);

	// 计算朝向角度（弧度制，0 度为正右，逆时针旋转）
    float angle = 0;
    if (cf_key_down(CF_KEY_Q)) {
		angle += PI / 60.0f; // 每帧逆时针旋转 3 度
    }
    if (cf_key_down(CF_KEY_E)) {
		angle -= PI / 60.0f; // 每帧顺时针旋转 3 度
    }
	SpriteRotate(angle);
}