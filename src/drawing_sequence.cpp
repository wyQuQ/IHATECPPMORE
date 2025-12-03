#include "drawing_sequence.h"
#include "base_object.h"

#include <algorithm>
#include <vector>
#include <mutex>
#include <iostream>
#include <unordered_map>

// per-owner canvas 缓存（文件内静态）
namespace {
	struct CanvasCache {
		CF_Canvas canvas{};
		int w = 0;
		int h = 0;
	};
	static std::unordered_map<BaseObject*, CanvasCache> g_canvas_cache;
	static std::mutex g_canvas_cache_mutex;
}

// 单例
DrawingSequence& DrawingSequence::Instance() noexcept
{
	static DrawingSequence inst;
	return inst;
}

void DrawingSequence::Register(BaseObject* obj) noexcept
{
	if (!obj) return;
	std::lock_guard<std::mutex> lk(m_mutex);
	auto e = std::make_unique<Entry>();
	e->owner = obj;
	e->reg_index = m_next_reg_index++;
	m_entries.push_back(std::move(e));
}

void DrawingSequence::Unregister(BaseObject* obj) noexcept
{
	if (!obj) return;

	// 从缓存中销毁对应 canvas（如果存在）
	{
		std::lock_guard<std::mutex> lk(g_canvas_cache_mutex);
		auto it = g_canvas_cache.find(obj);
		if (it != g_canvas_cache.end()) {
			if (it->second.canvas.id != 0) {
				cf_destroy_canvas(it->second.canvas);
				it->second.canvas.id = 0;
			}
			g_canvas_cache.erase(it);
		}
	}

	// 从注册列表中移除
	std::lock_guard<std::mutex> lk(m_mutex);
	m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(),
		[obj](const std::unique_ptr<Entry>& p) { return p->owner == obj; }), m_entries.end());
}

// DrawAll: UPLOAD ONLY（只做帧提取与像素上传，不绘制）
// 在主循环早期调用以保证纹理已上传
void DrawingSequence::DrawAll()
{
	// 取得注册快照（减少持锁时间）
	std::vector<Entry> snapshot;
	{
		std::lock_guard<std::mutex> lk(m_mutex);
		snapshot.reserve(m_entries.size());
		for (auto& p : m_entries) snapshot.push_back(*p);
	}

	// 排序以保持确定性（Depth + reg_index）
	std::sort(snapshot.begin(), snapshot.end(), [](const Entry& a, const Entry& b) {
		int da = a.owner ? a.owner->GetDepth() : 0;
		int db = b.owner ? b.owner->GetDepth() : 0;
		if (da != db) return da < db;
		return a.reg_index < b.reg_index;
	});

	// 遍历：提取帧并上传到 per-owner canvas（不做绘制）
	for (auto& e : snapshot) {
		BaseObject* owner = e.owner;
		if (!owner) continue;

		// 跳过不可见对象（避免创建/上传画布）
		if (!owner->IsVisible()) continue;

		PngFrame frame = owner->SpriteGetFrame();
		if (frame.empty()) continue;

		// 简单验证像素大小
		size_t expected_bytes = static_cast<size_t>(frame.w) * static_cast<size_t>(frame.h) * 4u;
		if (frame.pixels.size() < expected_bytes) {
			std::cerr << "[DrawingSequence] pixel data size mismatch for owner\n";
			continue;
		}

		// 获取或创建 per-owner canvas（保护 g_canvas_cache）
		CF_Canvas local_canvas{};
		{
			std::lock_guard<std::mutex> lk(g_canvas_cache_mutex);
			auto it = g_canvas_cache.find(owner);
			if (it == g_canvas_cache.end()) {
				CanvasCache cc;
				cc.canvas = cf_make_canvas(cf_canvas_defaults(frame.w, frame.h));
				cc.w = frame.w;
				cc.h = frame.h;
				g_canvas_cache.emplace(owner, std::move(cc));
				local_canvas = g_canvas_cache[owner].canvas;
			} else {
				CanvasCache& cc = it->second;
				if (cc.canvas.id == 0 || cc.w != frame.w || cc.h != frame.h) {
					if (cc.canvas.id != 0) {
						cf_destroy_canvas(cc.canvas);
						cc.canvas.id = 0;
					}
					cc.canvas = cf_make_canvas(cf_canvas_defaults(frame.w, frame.h));
					cc.w = frame.w;
					cc.h = frame.h;
				}
				local_canvas = cc.canvas;
			}
		}

		if (local_canvas.id == 0) {
			std::cerr << "[DrawingSequence] failed to create canvas for owner\n";
			continue;
		}

		// 上传像素到 canvas 的目标纹理（在主线程执行）
		CF_Texture tgt = cf_canvas_get_target(local_canvas);
		size_t bytes = static_cast<size_t>(frame.w) * static_cast<size_t>(frame.h) * sizeof(CF_Pixel);
		cf_texture_update(tgt, frame.pixels.data(), bytes);
	}
}

// BlitAll: 将已经上传的 canvas 绘制到屏幕。
// 已移除回调分支：默认绘制由 DrawingSequence 统一处理。
// 使用与 SetDrawCallback 相同的互斥策略：在绘制时对 m_mutex 加锁以避免并发修改回调/注册表导致的竞态。
void DrawingSequence::BlitAll()
{
	// 取得注册快照（避免持锁时间过长）
	std::vector<Entry> snapshot;
	{
		std::lock_guard<std::mutex> lk(m_mutex);
		snapshot.reserve(m_entries.size());
		for (auto& p : m_entries) snapshot.push_back(*p);
	}

	std::sort(snapshot.begin(), snapshot.end(), [](const Entry& a, const Entry& b) {
		int da = a.owner ? a.owner->GetDepth() : 0;
		int db = b.owner ? b.owner->GetDepth() : 0;
		if (da != db) return da < db;
		return a.reg_index < b.reg_index;
	});

	for (auto& e : snapshot) {
		BaseObject* owner = e.owner;
		if (!owner) continue;

		// 如果不可见则跳过绘制
		if (!owner->IsVisible()) continue;

		// 查找缓存 canvas（短锁粒度）
		CF_Canvas local_canvas{};
		int w = 0, h = 0;
		{
			std::lock_guard<std::mutex> lk(g_canvas_cache_mutex);
			auto it = g_canvas_cache.find(owner);
			if (it == g_canvas_cache.end()) {
				// 没有上传过的 canvas：跳过
				continue;
			}
			local_canvas = it->second.canvas;
			w = it->second.w;
			h = it->second.h;
		}

		if (local_canvas.id == 0 || w <= 0 || h <= 0) continue;

		// 使用与回调相同的互斥设计：在读取/使用与绘制相关的共享状态时保护 m_mutex，
		// 同时将绘制主体放在 try/catch 内以保证稳定性并记录异常。
		std::lock_guard<std::mutex> draw_lk(m_mutex);
		try {
			// 默认绘制：
			//  - 所有翻转（flipX/flipY）始终绕贴图中心执行
			//  - 旋转绕用户提供的枢轴（pivot），pivot 为相对于贴图中心的偏移（以像素为单位）
			CF_V2 pos = owner ? owner->GetPosition() : cf_v2(0.0f, 0.0f);
			const float fw = static_cast<float>(w);
			const float fh = static_cast<float>(h);
			const float cx = fw * 0.5f;
			const float cy = fh * 0.5f;

			// 从 BaseObject 读取旋转、翻转标志与枢轴点（枢轴以"相对于贴图中心"的像素偏移表示）
			const float rot = owner ? owner->SpriteGetRotation() : 0.0f;
			const bool flipx = owner ? owner->SpriteGetFlipX() : false;
			const bool flipy = owner ? owner->SpriteGetFlipY() : false;
			CF_V2 pivot = owner ? owner->GetPivot() : cf_v2(0.0f, 0.0f);

			// 绘制顺序（语义清晰且高效）：
			// 1) 把枢轴移动到世界坐标 pos（pos 表示枢轴在世界的位置）
			// 2) 若需要翻转：以贴图中心为轴做平移->scale->回平移
			// 3) 若需要旋转：在枢轴位置旋转
			// 4) 把贴图左上角绘制到 (-pivot_tl) 处，使枢轴映射到世界 pos
			cf_draw_push();

			// 将枢轴移动到世界坐标 pos
			cf_draw_translate(pos.x, pos.y);

			// 翻转：围绕贴图中心执行（通过相对于枢轴的 center_rel 平移实现）
			const float sx = flipx ? -1.0f : 1.0f;
			const float sy = flipy ? -1.0f : 1.0f;
			if (flipx || flipy) {
				cf_draw_translate(- pivot.x, - pivot.y);
				cf_draw_scale(sx, sy);
				cf_draw_translate(pivot.x, pivot.y);
			}

			// 旋转：在枢轴处旋转（此时枢轴位于原点）
			if (rot != 0.0f) {
				cf_draw_translate(-2 * pivot.x * flipx, -2 * pivot.y * flipy);
				cf_draw_rotate(-rot * sx * sy);
				cf_draw_translate(2 * pivot.x * flipx, 2 * pivot.y * flipy);
			}

			cf_draw_canvas(local_canvas, -pivot, cf_v2(fw, fh));

			cf_draw_pop();
		} catch (const std::exception& ex) {
			std::cerr << "[DrawingSequence] draw threw std::exception: " << ex.what() << "\n";
		} catch (...) {
			std::cerr << "[DrawingSequence] draw threw unknown exception\n";
		}
	}
}