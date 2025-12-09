#include "bullet.h"
#include <atomic>

extern std::atomic<int> g_frame_count;
extern int g_frame_rate; // 全局帧率（每秒帧数）

// 设置子弹的生存期
int Bullet::kLifetimeFrames = g_frame_rate * 5; 

void Bullet::Start()
{
    // 记录生成时的全局帧计数
    m_spawn_frame = static_cast<int>(g_frame_count.load());
    // 设置子弹贴图源，其他参数使用默认值
    SpriteSetSource("/sprites/bullet.png", 2);
    SpriteSetUpdateFreq(5); // 设置动画更新频率为每 5 帧更新一次
}

void Bullet::Update()
{
    // 基于全局帧计数判断是否到期（帧差 >= kLifetimeFrames 则请求销毁）
    int now = static_cast<int>(g_frame_count.load());
    if ((now - m_spawn_frame) >= kLifetimeFrames) {
        // 使用 ObjManager 安全销毁（在下一帧的安全点实际释放）
        objs.Destroy(GetObjToken());
        return;
    }

    // 这里可以加入其他子弹行为（移动、碰撞响应等）
}