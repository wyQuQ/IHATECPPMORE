#include "player_object.h"
#include "bullet.h"

// 每帧移动的速度
static constexpr float speed = 3.0f;
const float PI = 3.14159265358979f;
ObjToken test[9] = { ObjToken::Invalid() };
// 为每个 PlayerObject 实例保存是否着地
static std::unordered_map<const PlayerObject*, bool> s_grounded_map;

// 为每个实例保存按住跳跃可用的剩余帧数（variable jump）
static std::unordered_map<const PlayerObject*, float> s_jump_hold_time_left;

// 为每个实例保存离开地面后允许跳跃的 coyote 时间（帧）
static std::unordered_map<const PlayerObject*, float> s_coyote_time_left;

// 可调参数
static constexpr int max_jump_hold_frames = 12;       // 按住跳跃最多允许的帧数（按住越久跳得越高）
static constexpr float low_gravity_multiplier = 0.4f; // 按住跳跃时的重力倍率（较小，保留上升速度）
static constexpr float fall_gravity_multiplier = 1.6f; // 松开或下落时的加强重力倍率（更快下落）
static constexpr int coyote_time_frames = 6;         // 离地后仍可跳的帧数（coyote time）

// 每帧移动的速度
static constexpr float speed = 3.0f;
const float PI = 3.14159265358979f;
ObjToken test[3] = { ObjToken::Invalid() };
int i = 0;

static constexpr float gravity = 3.0f;        // 基础每帧重力加速度（可根据需要调整）
static constexpr float max_fall_speed = -12.0f; // 终端下落速度（负值）

void PlayerObject::Start()
{
    // 统一设置贴图路径、竖排帧数、动画更新频率和绘制深度，并注册到绘制序列
    // 如需要默认值，请使用高粒度的 SetSprite*() 和 Set*() 方法逐一设置非默认值参数
    // 资源路径无默认值，必须手动设置
    SpriteSetStats("/sprites/idle.png", 3, 7, 0);

    // 可选：初始化位置（根据需要调整），例如屏幕中心附近
    SetPosition(cf_v2(0.0f, 0.0f));

    Scale(0.6f);
	SetPivot(1,1);

    // 确保 maps 有默认条目（可选）
    s_grounded_map[this] = false;
    s_jump_hold_time_left[this] = 0.0f;
    s_coyote_time_left[this] = 0.0f;
}

void PlayerObject::Update()
{
    // 当检测到按键按下时，设置速度方向（不直接 SetPosition，使用速度积分）
    CF_V2 dir(0,0);
    if (Input::IsKeyInState(CF_KEY_A, KeyState::Hold)) {
        dir.x -= 1;
    }
    if (Input::IsKeyInState(CF_KEY_D, KeyState::Hold)) {
        dir.x += 1;
    }
    if (Input::IsKeyInState(CF_KEY_SPACE, KeyState::Hold)) {
        dir.y += 1;
    }
    if (Input::IsKeyInState(CF_KEY_S, KeyState::Hold)) {
        dir.y -= 1;
    }

	// 计算朝向角度（弧度制，0 度为正右，逆时针旋转）
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
    if (Input::IsKeyInState(CF_KEY_W, KeyState::Down)) {
        if (objs.TryGetRegisteration(test[i])) {
            objs.Destroy(test[i]);
        }
        auto test_token = objs.Create<Bullet>();
        if (test_token.isValid()) test[i] = test_token;
        auto rot = GetRotation();
        int flip = (SpriteGetFlipX() ? -1 : 1);
        objs[test[i]].SetRotation(rot);
        objs[test[i]].SpriteFlipX(SpriteGetFlipX());
        objs[test[i]].SetPosition(GetPosition());
        objs[test[i]].SetVisible(true);
        objs[test[i]].SetVelocity(v2math::angled(CF_V2(30.0f), rot) * flip);
        i = (i + 1) % 3; // 场上仅存在3个 TestObject 实例，若多出则销毁最早生成的那个
    }

    // 读取当前垂直速度以判断是上升还是下落
    CF_V2 cur_vel = GetVelocity();
    float gravity_multiplier = 1.0f;

    // 读取按住跳跃相关状态与保留时间（map 中可能没有条目则 operator[] 会插入默认值）
    bool space_down = Input::IsKeyInState(CF_KEY_SPACE, KeyState::Hold);
    float hold_time_left = s_jump_hold_time_left[this];
    float coyote_left = s_coyote_time_left[this];

    // 仅在上升阶段（垂直速度为正）并且正在按住且有保留时间时使用低重力
    if (cur_vel.y > 0.0f && space_down && hold_time_left > 0.0f) {
        gravity_multiplier = low_gravity_multiplier;
    }
    else if (cur_vel.y < 0.0f) {
        // 下落阶段使用加强重力
        gravity_multiplier = fall_gravity_multiplier;
    }
    else {
        gravity_multiplier = 1.0f;
    }

    // 应用重力（向 y 负方向施加）
    AddVelocity(cf_v2(0.0f, -gravity * gravity_multiplier));

    // 限制下落速度（防止无限加速）
    cur_vel = GetVelocity();
    if (cur_vel.y < max_fall_speed) {
        cur_vel.y = max_fall_speed;
        SetVelocity(cur_vel);
    }

    // 每帧递减 coyote 时间（若有）
    if (coyote_left > 0.0f) {
        coyote_left -= 1.0f;
        if (coyote_left < 0.0f) coyote_left = 0.0f;
        s_coyote_time_left[this] = coyote_left;
    }

    // 每帧递减 jump hold 时间（若有）
    if (hold_time_left > 0.0f) {
        hold_time_left -= 1.0f;
        if (hold_time_left < 0.0f) hold_time_left = 0.0f;
        s_jump_hold_time_left[this] = hold_time_left;
    }
}

void PlayerObject::OnCollisionEnter(const ObjManager::ObjToken & other_token, const CF_Manifold & manifold) noexcept {
    // 碰撞进入时的处理逻辑
    printf("Collided with object token: %u\n", other_token.index);

    // 将Player对象的位置重置到上一帧位置，避免穿透
    SetPosition(GetPrevPosition());

    // 判断是否为“地面接触”：使用修正向量的 y 分量（correction = -n * depth）
    // 如果 correction.y > 0 则说明被向上修正（站在别的物体上）
    if (manifold.count > 0) {
        float correction_y = -manifold.n.y * manifold.depths[0];
        if (correction_y > 0.001f) {
            s_grounded_map[this] = true;
            // 着地时取消按住跳跃保留时间与 coyote 时间
            s_jump_hold_time_left[this] = 0.0f;
            s_coyote_time_left[this] = 0.0f;
        }
    }
}

void PlayerObject::OnCollisionStay(const ObjManager::ObjToken & other_token, const CF_Manifold & manifold) noexcept {
    // 碰撞持续时的处理逻辑
    printf("Still colliding with object token: %u\n", other_token.index);

    CF_V2 correction = cf_v2(-manifold.n.x * manifold.depths[0], -manifold.n.y * manifold.depths[0]);
    CF_V2 current_position = GetPosition(); // 使用公开接口
    CF_V2 new_position = cf_v2(current_position.x + correction.x, current_position.y + correction.y);
    SetPosition(new_position);

    // 如果修正向量有正的 y 分量，说明我们被向上推，视为着地
    if (correction.y > 0.001f) {
        s_grounded_map[this] = true;
        // 着地时取消按住跳跃保留时间与 coyote 时间
        s_jump_hold_time_left[this] = 0.0f;
        s_coyote_time_left[this] = 0.0f;
    }
}

void PlayerObject::OnCollisionExit(const ObjManager::ObjToken & other_token, const CF_Manifold & manifold) noexcept {
    // 碰撞退出时的处理逻辑
    printf("No longer colliding with object token: %u\n", other_token.index);

    // 离开碰撞时取消着地标记，并启动 coyote 时间（短暂允许再次跳跃）
    s_grounded_map[this] = false;
    s_coyote_time_left[this] = static_cast<float>(coyote_time_frames);
    // 离开时不保留按住时间（按住跳跃仅在起跳后短时间内有效）
    s_jump_hold_time_left[this] = 0.0f;
}
