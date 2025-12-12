#include "player_object.h"
#include "bullet.h"

extern std::atomic<int> g_frame_count;
extern int g_frame_rate; // 全局帧率（每秒帧数）

// 当前全局帧计数（用于射击CD）
int cur_frame = 0;

// 每帧移动的速度
static constexpr float speed = 3.0f;
const float PI = 3.14159265358979f;

// 可调参数
static constexpr int max_jump_hold_frames = 12;       // 按住跳跃最多允许的帧数（按住越久跳得越高）
static constexpr float low_gravity_multiplier = 0.4f; // 按住跳跃时的重力倍率（较小，保留上升速度）
static constexpr float fall_gravity_multiplier = 0.4f; // 松开或下落时的加强重力倍率（更快下落）
static constexpr int coyote_time_frames = 6;         // 离地后仍可跳的帧数（coyote time）

static constexpr float gravity = 0.3f;        // 基础每帧重力加速度（可根据需要调整）
static constexpr float max_fall_speed = -12.0f; // 终端下落速度（负值）

void PlayerObject::Start()
{
    // 统一设置贴图路径、竖排帧数、动画更新频率和绘制深度，并注册到绘制序列
    // 如需要默认值，请使用高粒度的 SetSprite*() 和 Set*() 方法逐一设置非默认值参数
    // 资源路径无默认值，必须手动设置
    SpriteSetStats("/sprites/idle.png", 3, 6, 0);

    // 可选：初始化位置（根据需要调整），例如屏幕中心附近
    SetPosition(cf_v2(0.0f, 0.0f));

    Scale(0.6f);
    jump_count = 2;
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
    CF_V2 cur_vel_move = GetVelocity();
    cur_vel_move.x = dir * speed;
    SetVelocity(cur_vel_move);

    // 计算朝向角度（弧度制，0 度为正右，逆时针旋转）（测试用）
    float angle = 0;
    if (Input::IsKeyInState(CF_KEY_Q, KeyState::Hold)) {
        angle += PI / 60.0f; // 每帧逆时针旋转 3 度
        std::cout << GetRotation() << std::endl;
    }
    if (Input::IsKeyInState(CF_KEY_E, KeyState::Hold)) {
        angle -= PI / 60.0f; // 每帧顺时针旋转 3 度
    }
    Rotate(angle);

    // 按 W 键发射 TestObject 实例
    if (Input::IsKeyInState(CF_KEY_W, KeyState::Down) && cur_frame + g_frame_rate * 0.2 <= g_frame_count) {
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

    // 读取按住/保留时间引用（直接修改 map）
    bool space_down = Input::IsKeyInState(CF_KEY_SPACE, KeyState::Hold);
    float& hold_time = hold_time_left;
    int& coyote_left = coyote_time_left;
    bool grd = grounded;

    // 若不在地面且有 coyote 时间，逐帧递减（在 Update 而非 OnCollisionExit 中递减）
    if (!grd && coyote_left > 0) {
        coyote_left -= 1;
        if (coyote_left < 0) coyote_left = 0;
    }

	int& count = jump_count;// 当前剩余跳跃次数

    if (Input::IsKeyInState(CF_KEY_SPACE, KeyState::Down)) {

        if (grd || coyote_left > 0||count>0) {
            CF_V2 v = GetVelocity();
            v.y = 8.0f; // 初始跳跃速度（可调）
            SetVelocity(v);

            // 开启按住延长跳跃的计时（仅在跳跃开始时设置）
            hold_time = static_cast<float>(max_jump_hold_frames);

            // 清理状态，防止重复起跳
            grounded = false;
            coyote_left = 0;

			// 减少跳跃次数
            count--;
            if (count < 0) count = 0;
        }
    }

    // 获取当前垂直速度，用于判断上升/下落阶段
    CF_V2 cur_vel_jump = GetVelocity();
    float gravity_multiplier = 1.0f;

    // 可变跳跃逻辑：仅在上升阶段、按住且有保留时间时生效
    if (cur_vel_jump.y > 0.0f && space_down && hold_time > 0.0f) {
        gravity_multiplier = low_gravity_multiplier;
        // 消耗保留帧（每帧）
        hold_time -= 1.0f;
        if (hold_time < 0.0f) hold_time = 0.0f;
    }
    else if (cur_vel_jump.y < 0.0f) {
        gravity_multiplier = fall_gravity_multiplier;
    }
    else {
        gravity_multiplier = 1.0f;
    }

    // 应用重力
    AddVelocity(cf_v2(0.0f, -gravity * gravity_multiplier));

    // 限制下落速度
    cur_vel_jump = GetVelocity();
    if (cur_vel_jump.y < max_fall_speed) {
        cur_vel_jump.y = max_fall_speed;
        SetVelocity(cur_vel_jump);
    }

    // 如果处于着地，重置 coyote（可选）
    if (grounded) {
        coyote_time_left = coyote_time_frames;
    }


}

void PlayerObject::EndFrame() {
    auto vel = GetVelocity();

    // 设置贴图翻转（根据移动方向）
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

void PlayerObject::Exclusion(const CF_Manifold& m) {

    CF_V2 correction = cf_v2(-m.n.x * m.depths[0], -m.n.y * m.depths[0]);
    CF_V2 current_position = GetPosition();
    CF_V2 new_position = cf_v2(current_position.x + correction.x, current_position.y + correction.y);
    SetPosition(new_position, true);

    if (correction.y > 0.001f && GetVelocity().y < 0 && v2math::length(m.contact_points[0]-m.contact_points[1]) > speed) {
        grounded = true;
        hold_time_left = 0.0f;
        coyote_time_left = coyote_time_frames;
        jump_count = 2;
        SetVelocity(cf_v2(GetVelocity().x, 0.0f));
    }
}

void PlayerObject::OnCollisionEnter(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept {

    if (objs[other_token].GetColliderType() != ColliderType::SOLID) return;
	Exclusion(manifold);
}

void PlayerObject::OnCollisionStay(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept {

    if (objs[other_token].GetColliderType() != ColliderType::SOLID) return;
	Exclusion(manifold);
}

void PlayerObject::OnCollisionExit(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept {

    if (objs[other_token].GetColliderType() != ColliderType::SOLID) return;

    // 离开碰撞时取消着地标记
    grounded = false;

    // 启动 coyote 时间（离地后短时间仍可起跳）
    coyote_time_left = coyote_time_frames;
}
