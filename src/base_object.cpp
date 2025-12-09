#include "base_object.h"
#include "drawing_sequence.h" // 在 C++ 文件中引用以便使用 DrawingSequence 接口
#include <iostream>

// BaseObject 的精灵资源与绘制注册相关逻辑：
// - SpriteSetSource 在设置新路径时会注册/注销 DrawingSequence 以纳入统一的上传与绘制流程。
// - SpriteClearPath 会卸载资源并从 DrawingSequence 中注销。
// - TweakColliderWithPivot 用于在用户改变 pivot 时同步调整碰撞形状。

void BaseObject::SpriteSetSource(const std::string& path, int count, bool set_shape_aabb) noexcept
{
    std::string current_path = "";
    bool had = HasPath(&current_path);
    if (had && current_path == path)return;
    if (had) {
        // 注销旧资源的绘制注册，释放 per-owner canvas 的引用
        DrawingSequence::Instance().Unregister(this);
    }
    PngSprite::ClearPath();
    PngSprite::SetPath(path);
    SpriteSetVerticalFrameCount(count);
    bool has = HasPath();
    if (has) {
        // 注册到绘制序列，便于 DrawAll/BlitAll 处理
        DrawingSequence::Instance().Register(this);
        if (set_shape_aabb) {
            // 如果需要，根据当前帧自动设置碰撞 AABB（以贴图中心为基准）
            PngFrame frame = SpriteGetFrame();
            if (frame.w > 0 && frame.h > 0) {
                SetCenteredAabb(static_cast<float>(frame.w) * 0.5f, static_cast<float>(frame.h) * 0.5f);
            }
        }
    }
}

void BaseObject::SpriteClearPath() noexcept
{
    bool had = HasPath();
    PngSprite::ClearPath();
    if (had) {
        DrawingSequence::Instance().Unregister(this);
    }
}

// 当用户想要将 pivot 应用于碰撞器时，调整本地 shape 以将枢轴偏移应用到形状（便于渲染/碰撞对齐）
void BaseObject::TweakColliderWithPivot(const CF_V2& pivot) noexcept
{
    CF_ShapeWrapper shape = get_shape();
    switch (shape.type) {
    case CF_ShapeType::CF_SHAPE_TYPE_AABB:
        shape.u.aabb.min -= pivot;
        shape.u.aabb.max -= pivot;
		break;
    case CF_ShapeType::CF_SHAPE_TYPE_CIRCLE:
		shape.u.circle.p -= pivot;
        break;
	case CF_ShapeType::CF_SHAPE_TYPE_CAPSULE:
		shape.u.capsule.a -= pivot;
        shape.u.capsule.b -= pivot;
		break;
	case CF_ShapeType::CF_SHAPE_TYPE_POLY:
        for (int i = 0; i < shape.u.poly.count; ++i) {
            shape.u.poly.verts[i] -= pivot;
		}
        break;
	default:
		std::cerr << "[BaseObject] TweakColliderWithPivot: Unsupported shape type " << static_cast<int>(shape.type) << std::endl;
		break;
    }
	set_shape(shape);
}

BaseObject::~BaseObject() noexcept
{
    // 在销毁时通知 OnDestroy 并确保从绘制序列注销，释放与绘制相关的所有资源引用。
    OnDestroy();
    DrawingSequence::Instance().Unregister(this);
}