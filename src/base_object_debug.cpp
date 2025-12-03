#include "base_object.h"
#include "base_physics.h" // 提供 CF_ShapeWrapper / v2math 等
#include <cmath>

// 实现：根据 BaseObject 中的 CF_ShapeWrapper 绘制红色轮廓（noexcept）
void RenderBaseObjectCollisionDebug(const BaseObject* obj) noexcept
{
    if (!obj) return;

    const CF_ShapeWrapper& s = obj->GetShape();
    const CF_V2 pos = obj->GetPosition();

    // 使用红色并保存绘制状态
    cf_draw_push();
    cf_draw_push_color(cf_color_red());

    switch (s.type)
    {
    case CF_SHAPE_TYPE_AABB:
    {
        const CF_Aabb& a = s.u.aabb;
        CF_V2 p1{ a.min.x, a.min.y };
        CF_V2 p2{ a.max.x, a.min.y };
        CF_V2 p3{ a.max.x, a.max.y };
        CF_V2 p4{ a.min.x, a.max.y };
        // 将 shape 坐标与对象位置叠加（与原始项目约定保持一致）
        p1 += pos; p2 += pos; p3 += pos; p4 += pos;
        cf_draw_line(p1, p2, 0.0f);
        cf_draw_line(p2, p3, 0.0f);
        cf_draw_line(p3, p4, 0.0f);
        cf_draw_line(p4, p1, 0.0f);
        break;
    }
    case CF_SHAPE_TYPE_CIRCLE:
    {
        const CF_Circle& c = s.u.circle;
        // cf_draw_circle2(center, radius, angle)
        cf_draw_circle2(c.p + pos, c.r, 0.0f);
        break;
    }
    case CF_SHAPE_TYPE_CAPSULE:
    {
        const CF_Capsule& cap = s.u.capsule;
        CF_V2 a = cap.a + pos;
        CF_V2 b = cap.b + pos;
        float r = cap.r;

        // 绘制中轴线
        cf_draw_line(a, b, 0.0f);

        // 绘制端点圆
        cf_draw_circle2(a, r, 0.0f);
        cf_draw_circle2(b, r, 0.0f);

        // 绘制连接矩形（用四条线近似）
        CF_V2 dir = CF_V2{ b.x - a.x, b.y - a.y };
        CF_V2 ndir = v2math::normalized(dir);
        CF_V2 perp = CF_V2{ -ndir.y * r, ndir.x * r }; // 垂直向量 * r

        CF_V2 p1{ a.x + perp.x, a.y + perp.y };
        CF_V2 p2{ b.x + perp.x, b.y + perp.y };
        CF_V2 p3{ b.x - perp.x, b.y - perp.y };
        CF_V2 p4{ a.x - perp.x, a.y - perp.y };

        cf_draw_line(p1, p2, 0.0f);
        cf_draw_line(p2, p3, 0.0f);
        cf_draw_line(p3, p4, 0.0f);
        cf_draw_line(p4, p1, 0.0f);
        break;
    }
    case CF_SHAPE_TYPE_POLY:
    {
        const CF_Poly& poly = s.u.poly;
        int n = poly.count;
        if (n > 1) {
            for (int i = 0; i < n; ++i) {
                CF_V2 p0 = poly.verts[i] + pos;
                CF_V2 p1 = poly.verts[(i + 1) % n] + pos;
                cf_draw_line(p0, p1, 0.0f);
            }
        }
        break;
    }
    default:
        // 未知类型：无动作
        break;
    }

    cf_draw_pop_color();
    cf_draw_pop();
}