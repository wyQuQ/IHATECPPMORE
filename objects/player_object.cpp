#include "player_object.h"
#include "bullet.h"

extern std::atomic<int> g_frame_count;
extern int g_frame_rate; // 全局帧率（每秒帧数）

// 当前全局帧计数（用于射击CD）
int cur_frame = 0;

const float PI = 3.14159265358979f;

// 可调参数
static constexpr float speed = 4.0f;            // 水平移动速度
static constexpr float gravity = 0.4f;        // 基础每帧重力加速度（可根据需要调整）
static constexpr float max_fall_speed = -7.5f; // 终端下落速度（负值）

void PlayerObject::Start()
{
    // 统一设置贴图路径、竖排帧数、动画更新频率和绘制深度，并注册到绘制序列
    // 如需要默认值，请使用高粒度的 SetSprite*() 和 Set*() 方法逐一设置非默认值参数
    // 资源路径无默认值，必须手动设置
    SpriteSetStats("/sprites/idle.png", 3, 6, 0, false);

    // 可选：初始化位置（根据需要调整），例如屏幕中心附近
    SetPosition(cf_v2(0.0f, 0.0f));

    Scale(0.5f);
	AddTag("player");
	ExcludeWithSolids(true);
    SetCenteredAabb(18.0f, SpriteHeight()); // 设置以贴图中心为基准的碰撞 AABB

	// 初始化跳跃状态
	double_jump_ready = true;
}

void PlayerObject::Update()
{
    // 当检测到按键按下时，设置速度方向（不直接 SetPosition，使用速度积分）
    int dir = 0;
    if (Input::IsKeyInState(CF_KEY_A, KeyState::Hold)) {
        dir -= 1;
    }
    if (Input::IsKeyInState(CF_KEY_D, KeyState::Hold)) {
        dir += 1;
    }
    // 应用水平速度
    SetVelocityX(dir * speed);

    // 按 W 键发射 TestObject 实例
    if (Input::IsKeyInState(CF_KEY_W, KeyState::Down) && cur_frame + g_frame_rate * 0.1 <= g_frame_count) {
        // 更新射击时间
        cur_frame = g_frame_count;

        // 创建 Bullet 对象并设置初始位置
        auto token = objs.Create<Bullet>();
        int flip = (SpriteGetFlipX() ? -1 : 1);
        CF_V2 dir = v2math::get_dir(GetRotation()) * flip;
        if (token.isValid()) objs[token].SetPosition(GetPosition() + dir * SpriteWidth() * 0.5f);

        // 设置发射方向与速度（测试用）
        auto rot = GetRotation();
        objs[token].SetRotation(rot);
        objs[token].SetVelocity(v2math::angled(CF_V2(12.0f), rot) * flip);
    }

    if (Input::IsKeyInState(CF_KEY_SPACE, KeyState::Down)) {

        if (grounded || double_jump_ready) {
            if (!grounded) {
				double_jump_ready = false;
				grounded = true; // 允许二段跳
            }
            // 初始跳跃速度（可调）
            SetVelocityY(double_jump_ready? 8.25f:7.9f);
            // 清理状态，防止重复起跳
            grounded = false;
        }
    }
    else if (Input::IsKeyInState(CF_KEY_SPACE, KeyState::Up)) {
		float vy = GetVelocity().y;
        if(vy > 0) {
            // 松开跳跃键时，如果仍在上升则减速
            SetVelocityY(vy * 0.25f);
		}
    }

    // 应用重力
    AddVelocity(cf_v2(0.0f, -gravity));

    // 限制下落速度
    CF_V2 cur_vel_jump = GetVelocity();
    if (cur_vel_jump.y < max_fall_speed) {
        cur_vel_jump.y = max_fall_speed;
        SetVelocity(cur_vel_jump);
    }
}

void PlayerObject::StartFrame() {
    // 每帧开始时重置 grounded 状态（等待碰撞回调更新）
    grounded = false;
}

void PlayerObject::EndFrame() {
    // 设置贴图翻转（根据移动方向）
    auto vel = GetVelocity();
    if (vel.x != 0) SpriteFlipX(vel.x < 0);

    // 根据状态切换贴图
    if (grounded) {
        if (vel.x != 0) {
            SpriteSetStats("/sprites/walk.png", 2, 5, 0, false);
        }
        else {
            SpriteSetStats("/sprites/idle.png", 3, 6, 0, false);
        }
    }
    else {
        if (vel.y > 0) {
            SpriteSetStats("/sprites/jump.png", 2, 4, 0, false);
        }
        else {
            SpriteSetStats("/sprites/fall.png", 2, 4, 0, false);
        }
    }
}

void PlayerObject::OnExclusionSolid(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept {
    if(std::abs(manifold.n.y) > 1 - 1e-3f)
        SetVelocity(cf_v2(GetVelocity().x, 0.0f));
    if (manifold.n.y > 1e-3f - 1 || v2math::length(manifold.contact_points[0] - manifold.contact_points[1]) < 1e-3f) return;
    grounded = true;
	double_jump_ready = true;
}

#if TESTER
//-----------------------------测试用-----------------------------
// Tester 类的简单实现（用于测试 BaseObject 功能）
void Tester::Start()
{
    // 设置贴图路径、竖排帧数、动画更新频率和绘制深度
    SpriteSetStats("/sprites/common_square.png", 1, 1, 0);
    // 初始化位置（根据需要调整）
    SetPosition(cf_v2(-200.0f, 0.0f));
	tspeed = 4.0f; // 每帧移动速度
	ExcludeWithSolids(true); // 与实体排斥
    Scale(0.8f);
}

void Tester::Update()
{
    // 当检测到按键按下时，设置速度方向（不直接 SetPosition，使用速度积分）
    CF_V2 dir = v2math::zero();
    if (Input::IsKeyInState(CF_KEY_J, KeyState::Hold)) {
        dir.x -= 1;
    }
    if (Input::IsKeyInState(CF_KEY_L, KeyState::Hold)) {
        dir.x += 1;
    }
    if (Input::IsKeyInState(CF_KEY_K, KeyState::Hold)) {
        dir.y -= 3;
    }
    if (Input::IsKeyInState(CF_KEY_I, KeyState::Hold)) {
        dir.y += 3;
    }
	SetVelocity(dir * tspeed);
}
//-----------------------------测试用-----------------------------
#endif