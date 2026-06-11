// Copyright (c) Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#include "jolt_viewer.h"
#include "joltc_conversions.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

#ifdef JPH_DEBUG_RENDERER
#include <Jolt/Renderer/DebugRendererSimple.h>
#endif

#ifdef JPH_VIEWER_WITH_TEST_FRAMEWORK
#include <TestFramework.h>
#include <Renderer/DebugRendererImp.h>
#include <Renderer/Font.h>
#include <Renderer/Renderer.h>
#ifdef JPH_USE_VK
#include <Renderer/VK/RendererVK.h>
#endif
#ifdef JPH_PLATFORM_WINDOWS
#include <Window/ApplicationWindowWin.h>
#elif defined(JPH_PLATFORM_LINUX)
#include <Window/ApplicationWindowLinux.h>
#elif defined(JPH_PLATFORM_MACOS)
#include <Window/ApplicationWindowMacOS.h>
#endif
#endif

#include <chrono>
#include <cstdio>

#ifdef JPH_DEBUG_RENDERER

using namespace JPH;

class ObjDebugRenderer final : public DebugRendererSimple
{
public:
	explicit ObjDebugRenderer(FILE* file_)
		: file(file_)
	{
	}

	~ObjDebugRenderer() override
	{
		if (file != nullptr)
			fclose(file);
	}

	void DrawLine(RVec3Arg inFrom, RVec3Arg inTo, ColorArg inColor) override
	{
		JPH_UNUSED(inColor);

		JPH_RVec3 from, to;
		FromJolt(inFrom, &from);
		FromJolt(inTo, &to);

		uint32_t i0 = WriteVertex(from);
		uint32_t i1 = WriteVertex(to);
		fprintf(file, "l %u %u\n", i0, i1);
		stats.lineCount++;
	}

	void DrawTriangle(RVec3Arg inV1, RVec3Arg inV2, RVec3Arg inV3, ColorArg inColor, ECastShadow inCastShadow) override
	{
		JPH_UNUSED(inColor);
		JPH_UNUSED(inCastShadow);

		JPH_RVec3 v1, v2, v3;
		FromJolt(inV1, &v1);
		FromJolt(inV2, &v2);
		FromJolt(inV3, &v3);

		uint32_t i0 = WriteVertex(v1);
		uint32_t i1 = WriteVertex(v2);
		uint32_t i2 = WriteVertex(v3);
		fprintf(file, "f %u %u %u\n", i0, i1, i2);
		stats.triangleCount++;
	}

	void DrawText3D(RVec3Arg inPosition, const string_view& inString, ColorArg inColor, float inHeight) override
	{
		JPH_UNUSED(inColor);

		JPH_RVec3 position;
		FromJolt(inPosition, &position);
		fprintf(file, "# text %.9g %.9g %.9g %.9g %.*s\n",
			(double)position.x,
			(double)position.y,
			(double)position.z,
			(double)inHeight,
			(int)inString.size(),
			inString.data());
		stats.textCount++;
	}

	bool Flush()
	{
		return file != nullptr && fflush(file) == 0;
	}

	void GetStats(JPH_ViewerStats* outStats) const
	{
		*outStats = stats;
	}

private:
	uint32_t WriteVertex(const JPH_RVec3& vertex)
	{
		fprintf(file, "v %.9g %.9g %.9g\n", (double)vertex.x, (double)vertex.y, (double)vertex.z);
		return ++stats.vertexCount;
	}

	FILE* file = nullptr;
	JPH_ViewerStats stats = {};
};

#ifdef JPH_VIEWER_WITH_TEST_FRAMEWORK

class ViewerWindow final :
#ifdef JPH_PLATFORM_WINDOWS
	public ApplicationWindowWin
#elif defined(JPH_PLATFORM_LINUX)
	public ApplicationWindowLinux
#elif defined(JPH_PLATFORM_MACOS)
	public ApplicationWindowMacOS
#else
	public ApplicationWindow
#endif
{
public:
	void SetInitialSize(int width, int height)
	{
		if (width > 0)
			mWindowWidth = width;
		if (height > 0)
			mWindowHeight = height;
	}
};

#endif

enum class ViewerKind
{
	Obj,
	Windowed,
};

struct JPH_Viewer
{
	ViewerKind kind = ViewerKind::Obj;
	ObjDebugRenderer* objRenderer = nullptr;

#ifdef JPH_VIEWER_WITH_TEST_FRAMEWORK
	ViewerWindow* window = nullptr;
	Renderer* renderer = nullptr;
	RefConst<Font> font;
	DebugRendererImp* debugRenderer = nullptr;
	CameraState camera;
	float worldScale = 1.0f;
#endif
};

#else

struct JPH_Viewer
{
	int unused;
};

#endif

void JPH_ViewerSettings_InitDefault(JPH_ViewerSettings* settings)
{
	if (settings == nullptr)
		return;

	settings->title = "Jolt Viewer C";
	settings->backend = JPH_ViewerBackend_Auto;
	settings->width = 1280;
	settings->height = 720;
	settings->cameraPosition = { -6.0f, 5.0f, -8.0f };
	settings->cameraTarget = { 0.0f, 0.0f, 0.0f };
	settings->worldScale = 1.0f;
}

#ifdef JPH_DEBUG_RENDERER

static DebugRenderer* GetDebugRenderer(JPH_Viewer* viewer)
{
	if (viewer == nullptr)
		return nullptr;

	if (viewer->kind == ViewerKind::Obj)
		return viewer->objRenderer;

#ifdef JPH_VIEWER_WITH_TEST_FRAMEWORK
	if (viewer->kind == ViewerKind::Windowed)
		return viewer->debugRenderer;
#endif

	return nullptr;
}

#ifdef JPH_VIEWER_WITH_TEST_FRAMEWORK

static inline void SetCameraLookAt(CameraState& camera, const JPH_RVec3& position, const JPH_RVec3& target)
{
	camera.mPos = ToJolt(position);

	Vec3 forward(target.x - position.x, target.y - position.y, target.z - position.z);
	camera.mForward = forward.NormalizedOr(Vec3(0.0f, 0.0f, -1.0f));

	Vec3 up(0.0f, 1.0f, 0.0f);
	if (abs(camera.mForward.Dot(up)) > 0.99f)
		up = Vec3(0.0f, 0.0f, 1.0f);
	camera.mUp = up;
}

static Renderer* CreateGpuRenderer(JPH_ViewerBackend backend)
{
	switch (backend)
	{
	case JPH_ViewerBackend_Vulkan:
#ifdef JPH_USE_VK
		return CreateRendererVK();
#else
		return nullptr;
#endif

	case JPH_ViewerBackend_Metal:
#if defined(JPH_USE_MTL) && defined(JPH_PLATFORM_MACOS)
		return Renderer::sCreate();
#else
		return nullptr;
#endif

	case JPH_ViewerBackend_DX12:
#if defined(JPH_USE_DX12) && defined(JPH_PLATFORM_WINDOWS)
		return Renderer::sCreate();
#else
		return nullptr;
#endif

	case JPH_ViewerBackend_Auto:
	default:
		return Renderer::sCreate();
	}
}

static JPH_Viewer* CreateWindowedViewer(const JPH_ViewerSettings* settings, JPH_ViewerBackend backend)
{
	JPH_ViewerSettings default_settings;
	if (settings == nullptr)
	{
		JPH_ViewerSettings_InitDefault(&default_settings);
		settings = &default_settings;
	}

	Renderer* renderer = CreateGpuRenderer(backend == JPH_ViewerBackend_Auto ? settings->backend : backend);
	if (renderer == nullptr)
		return nullptr;

	ViewerWindow* window = new ViewerWindow;
	window->SetInitialSize(settings->width, settings->height);
	window->Initialize(settings->title != nullptr ? settings->title : "Jolt Viewer C");
#ifdef JPH_PLATFORM_MACOS
	window->SetMouseMovedCallback([](int, int) {});
#endif

	renderer->Initialize(window);

	Font* font = new Font(renderer);
	if (!font->Create("Roboto-Regular", 24))
	{
		delete font;
		delete renderer;
		delete window;
		return nullptr;
	}

	JPH_Viewer* viewer = new JPH_Viewer;
	viewer->kind = ViewerKind::Windowed;
	viewer->window = window;
	viewer->renderer = renderer;
	viewer->font = font;
	viewer->debugRenderer = new DebugRendererImp(renderer, viewer->font);
	viewer->worldScale = settings->worldScale > 0.0f ? settings->worldScale : 1.0f;
	SetCameraLookAt(viewer->camera, settings->cameraPosition, settings->cameraTarget);
	return viewer;
}

#endif

#endif

JPH_Viewer* JPH_Viewer_CreateObj(const char* path)
{
#ifdef JPH_DEBUG_RENDERER
	if (path == nullptr)
		return nullptr;

	FILE* file = fopen(path, "w");
	if (file == nullptr)
		return nullptr;

	fputs("# Generated by jolt_viewer_c\n", file);
	fputs("# Import this OBJ in a DCC/viewer to inspect Jolt debug geometry.\n", file);

	JPH_Viewer* viewer = new JPH_Viewer;
	viewer->kind = ViewerKind::Obj;
	viewer->objRenderer = new ObjDebugRenderer(file);
	return viewer;
#else
	JPH_UNUSED(path);
	return nullptr;
#endif
}

JPH_Viewer* JPH_Viewer_CreateWindowed(const JPH_ViewerSettings* settings)
{
#if defined(JPH_DEBUG_RENDERER) && defined(JPH_VIEWER_WITH_TEST_FRAMEWORK)
	return CreateWindowedViewer(settings, JPH_ViewerBackend_Auto);
#else
	JPH_UNUSED(settings);
	return nullptr;
#endif
}

JPH_Viewer* JPH_Viewer_CreateMetal(const JPH_ViewerSettings* settings)
{
#if defined(JPH_DEBUG_RENDERER) && defined(JPH_VIEWER_WITH_TEST_FRAMEWORK)
	return CreateWindowedViewer(settings, JPH_ViewerBackend_Metal);
#else
	JPH_UNUSED(settings);
	return nullptr;
#endif
}

JPH_Viewer* JPH_Viewer_CreateVulkan(const JPH_ViewerSettings* settings)
{
#if defined(JPH_DEBUG_RENDERER) && defined(JPH_VIEWER_WITH_TEST_FRAMEWORK)
	return CreateWindowedViewer(settings, JPH_ViewerBackend_Vulkan);
#else
	JPH_UNUSED(settings);
	return nullptr;
#endif
}

JPH_Viewer* JPH_Viewer_CreateDX12(const JPH_ViewerSettings* settings)
{
#if defined(JPH_DEBUG_RENDERER) && defined(JPH_VIEWER_WITH_TEST_FRAMEWORK)
	return CreateWindowedViewer(settings, JPH_ViewerBackend_DX12);
#else
	JPH_UNUSED(settings);
	return nullptr;
#endif
}

void JPH_Viewer_Destroy(JPH_Viewer* viewer)
{
#ifdef JPH_DEBUG_RENDERER
	if (viewer != nullptr)
	{
#ifdef JPH_VIEWER_WITH_TEST_FRAMEWORK
		delete viewer->debugRenderer;
		viewer->font = nullptr;
		delete viewer->renderer;
		delete viewer->window;
#endif
		delete viewer->objRenderer;
		delete viewer;
	}
#else
	JPH_UNUSED(viewer);
#endif
}

JPH_DebugRenderer* JPH_Viewer_GetDebugRenderer(JPH_Viewer* viewer)
{
#ifdef JPH_DEBUG_RENDERER
	return reinterpret_cast<JPH_DebugRenderer*>(GetDebugRenderer(viewer));
#else
	JPH_UNUSED(viewer);
	return nullptr;
#endif
}

void JPH_Viewer_SetCameraPosition(JPH_Viewer* viewer, const JPH_RVec3* position)
{
#ifdef JPH_DEBUG_RENDERER
	if (viewer == nullptr || position == nullptr)
		return;

	if (viewer->kind == ViewerKind::Obj && viewer->objRenderer != nullptr)
		viewer->objRenderer->SetCameraPos(ToJolt(position));

#ifdef JPH_VIEWER_WITH_TEST_FRAMEWORK
	if (viewer->kind == ViewerKind::Windowed)
		viewer->camera.mPos = ToJolt(position);
#endif
#else
	JPH_UNUSED(viewer);
	JPH_UNUSED(position);
#endif
}

void JPH_Viewer_SetCameraLookAt(JPH_Viewer* viewer, const JPH_RVec3* position, const JPH_RVec3* target)
{
#ifdef JPH_DEBUG_RENDERER
	if (viewer == nullptr || position == nullptr || target == nullptr)
		return;

	JPH_Viewer_SetCameraPosition(viewer, position);

#ifdef JPH_VIEWER_WITH_TEST_FRAMEWORK
	if (viewer->kind == ViewerKind::Windowed)
		SetCameraLookAt(viewer->camera, *position, *target);
#endif
#else
	JPH_UNUSED(viewer);
	JPH_UNUSED(position);
	JPH_UNUSED(target);
#endif
}

void JPH_Viewer_Clear(JPH_Viewer* viewer)
{
#ifdef JPH_DEBUG_RENDERER
	if (viewer == nullptr)
		return;

#ifdef JPH_VIEWER_WITH_TEST_FRAMEWORK
	if (viewer->kind == ViewerKind::Windowed && viewer->debugRenderer != nullptr)
		viewer->debugRenderer->Clear();
#endif
#else
	JPH_UNUSED(viewer);
#endif
}

bool JPH_Viewer_RenderFrame(JPH_Viewer* viewer, float deltaTime)
{
#if defined(JPH_DEBUG_RENDERER) && defined(JPH_VIEWER_WITH_TEST_FRAMEWORK)
	JPH_UNUSED(deltaTime);

	if (viewer == nullptr || viewer->kind != ViewerKind::Windowed || viewer->renderer == nullptr || viewer->debugRenderer == nullptr)
		return false;

	if (!viewer->renderer->BeginFrame(viewer->camera, viewer->worldScale))
		return true;

	viewer->debugRenderer->DrawShadowPass();
	viewer->renderer->EndShadowPass();
	viewer->debugRenderer->Draw();
	viewer->renderer->EndFrame();
	viewer->debugRenderer->Clear();
	return true;
#else
	JPH_UNUSED(viewer);
	JPH_UNUSED(deltaTime);
	return false;
#endif
}

void JPH_Viewer_Run(JPH_Viewer* viewer, JPH_ViewerFrameCallback callback, void* userData)
{
#if defined(JPH_DEBUG_RENDERER) && defined(JPH_VIEWER_WITH_TEST_FRAMEWORK)
	if (viewer == nullptr || viewer->kind != ViewerKind::Windowed || viewer->window == nullptr)
		return;

	auto last_time = std::chrono::high_resolution_clock::now();
	viewer->window->MainLoop([viewer, callback, userData, last_time]() mutable {
		auto now = std::chrono::high_resolution_clock::now();
		std::chrono::duration<float> delta = now - last_time;
		last_time = now;

		if (callback != nullptr && !callback(viewer, delta.count(), userData))
			return false;

		return JPH_Viewer_RenderFrame(viewer, delta.count());
	});
#else
	JPH_UNUSED(viewer);
	JPH_UNUSED(callback);
	JPH_UNUSED(userData);
#endif
}

void JPH_Viewer_NextFrame(JPH_Viewer* viewer)
{
#ifdef JPH_DEBUG_RENDERER
	if (viewer != nullptr && viewer->kind == ViewerKind::Obj && viewer->objRenderer != nullptr)
		viewer->objRenderer->NextFrame();
#else
	JPH_UNUSED(viewer);
#endif
}

bool JPH_Viewer_Flush(JPH_Viewer* viewer)
{
#ifdef JPH_DEBUG_RENDERER
	return viewer != nullptr && viewer->kind == ViewerKind::Obj && viewer->objRenderer != nullptr && viewer->objRenderer->Flush();
#else
	JPH_UNUSED(viewer);
	return false;
#endif
}

void JPH_Viewer_GetStats(const JPH_Viewer* viewer, JPH_ViewerStats* stats)
{
	if (stats == nullptr)
		return;

#ifdef JPH_DEBUG_RENDERER
	if (viewer != nullptr && viewer->kind == ViewerKind::Obj && viewer->objRenderer != nullptr)
		viewer->objRenderer->GetStats(stats);
	else
		*stats = {};
#else
	JPH_UNUSED(viewer);
	*stats = {};
#endif
}

void JPH_Viewer_DrawLine(JPH_Viewer* viewer, const JPH_RVec3* from, const JPH_RVec3* to, JPH_Color color)
{
#ifdef JPH_DEBUG_RENDERER
	DebugRenderer* renderer = GetDebugRenderer(viewer);
	if (renderer != nullptr && from != nullptr && to != nullptr)
		renderer->DrawLine(ToJolt(from), ToJolt(to), Color(color));
#else
	JPH_UNUSED(viewer);
	JPH_UNUSED(from);
	JPH_UNUSED(to);
	JPH_UNUSED(color);
#endif
}

void JPH_Viewer_DrawTriangle(JPH_Viewer* viewer, const JPH_RVec3* v1, const JPH_RVec3* v2, const JPH_RVec3* v3, JPH_Color color, JPH_DebugRenderer_CastShadow castShadow)
{
#ifdef JPH_DEBUG_RENDERER
	DebugRenderer* renderer = GetDebugRenderer(viewer);
	if (renderer != nullptr && v1 != nullptr && v2 != nullptr && v3 != nullptr)
		renderer->DrawTriangle(
			ToJolt(v1),
			ToJolt(v2),
			ToJolt(v3),
			Color(color),
			static_cast<DebugRenderer::ECastShadow>(castShadow));
#else
	JPH_UNUSED(viewer);
	JPH_UNUSED(v1);
	JPH_UNUSED(v2);
	JPH_UNUSED(v3);
	JPH_UNUSED(color);
	JPH_UNUSED(castShadow);
#endif
}

void JPH_Viewer_DrawBox(JPH_Viewer* viewer, const JPH_AABox* box, JPH_Color color, JPH_DebugRenderer_CastShadow castShadow, JPH_DebugRenderer_DrawMode drawMode)
{
#ifdef JPH_DEBUG_RENDERER
	DebugRenderer* renderer = GetDebugRenderer(viewer);
	if (renderer != nullptr && box != nullptr)
		renderer->DrawBox(
			ToJolt(*box),
			Color(color),
			static_cast<DebugRenderer::ECastShadow>(castShadow),
			static_cast<DebugRenderer::EDrawMode>(drawMode));
#else
	JPH_UNUSED(viewer);
	JPH_UNUSED(box);
	JPH_UNUSED(color);
	JPH_UNUSED(castShadow);
	JPH_UNUSED(drawMode);
#endif
}

void JPH_Viewer_DrawSphere(JPH_Viewer* viewer, const JPH_RVec3* center, float radius, JPH_Color color, JPH_DebugRenderer_CastShadow castShadow, JPH_DebugRenderer_DrawMode drawMode)
{
#ifdef JPH_DEBUG_RENDERER
	DebugRenderer* renderer = GetDebugRenderer(viewer);
	if (renderer != nullptr && center != nullptr)
		renderer->DrawSphere(
			ToJolt(center),
			radius,
			Color(color),
			static_cast<DebugRenderer::ECastShadow>(castShadow),
			static_cast<DebugRenderer::EDrawMode>(drawMode));
#else
	JPH_UNUSED(viewer);
	JPH_UNUSED(center);
	JPH_UNUSED(radius);
	JPH_UNUSED(color);
	JPH_UNUSED(castShadow);
	JPH_UNUSED(drawMode);
#endif
}

void JPH_Viewer_DrawShape(JPH_Viewer* viewer, const JPH_Shape* shape, const JPH_RVec3* position, const JPH_Quat* rotation, const JPH_Vec3* scale, JPH_Color color, bool useMaterialColors, bool drawWireframe)
{
#ifdef JPH_DEBUG_RENDERER
	DebugRenderer* renderer = GetDebugRenderer(viewer);
	if (renderer == nullptr || shape == nullptr || position == nullptr)
		return;

	Quat jolt_rotation = rotation != nullptr ? ToJolt(rotation) : Quat::sIdentity();
	Vec3 jolt_scale = scale != nullptr ? ToJolt(scale) : Vec3::sOne();
	RMat44 transform = RMat44::sRotationTranslation(jolt_rotation, ToJolt(position));
	reinterpret_cast<const Shape*>(shape)->Draw(
		renderer,
		transform,
		jolt_scale,
		Color(color),
		useMaterialColors,
		drawWireframe);
#else
	JPH_UNUSED(viewer);
	JPH_UNUSED(shape);
	JPH_UNUSED(position);
	JPH_UNUSED(rotation);
	JPH_UNUSED(scale);
	JPH_UNUSED(color);
	JPH_UNUSED(useMaterialColors);
	JPH_UNUSED(drawWireframe);
#endif
}

void JPH_Viewer_DrawBodies(JPH_Viewer* viewer, JPH_PhysicsSystem* system, const JPH_DrawSettings* settings, const JPH_BodyDrawFilter* bodyFilter)
{
#ifdef JPH_DEBUG_RENDERER
	JPH_PhysicsSystem_DrawBodies(system, settings, JPH_Viewer_GetDebugRenderer(viewer), bodyFilter);
#else
	JPH_UNUSED(viewer);
	JPH_UNUSED(system);
	JPH_UNUSED(settings);
	JPH_UNUSED(bodyFilter);
#endif
}

void JPH_Viewer_DrawConstraints(JPH_Viewer* viewer, JPH_PhysicsSystem* system)
{
#ifdef JPH_DEBUG_RENDERER
	JPH_PhysicsSystem_DrawConstraints(system, JPH_Viewer_GetDebugRenderer(viewer));
#else
	JPH_UNUSED(viewer);
	JPH_UNUSED(system);
#endif
}

void JPH_Viewer_DrawConstraintLimits(JPH_Viewer* viewer, JPH_PhysicsSystem* system)
{
#ifdef JPH_DEBUG_RENDERER
	JPH_PhysicsSystem_DrawConstraintLimits(system, JPH_Viewer_GetDebugRenderer(viewer));
#else
	JPH_UNUSED(viewer);
	JPH_UNUSED(system);
#endif
}

void JPH_Viewer_DrawConstraintReferenceFrame(JPH_Viewer* viewer, JPH_PhysicsSystem* system)
{
#ifdef JPH_DEBUG_RENDERER
	JPH_PhysicsSystem_DrawConstraintReferenceFrame(system, JPH_Viewer_GetDebugRenderer(viewer));
#else
	JPH_UNUSED(viewer);
	JPH_UNUSED(system);
#endif
}
