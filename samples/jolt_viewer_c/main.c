// Copyright (c) Amer Koleci and Contributors.
// Distributed under the MIT license. See the LICENSE file in the project root for more information.

#include "jolt_viewer.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum
{
	LAYER_NON_MOVING = 0,
	LAYER_MOVING = 1,
	NUM_OBJECT_LAYERS = 2,
};

enum
{
	BP_LAYER_NON_MOVING = 0,
	BP_LAYER_MOVING = 1,
	NUM_BROAD_PHASE_LAYERS = 2,
};

static void trace_impl(const char* message)
{
	puts(message);
}

static bool setup_layers(
	JPH_BroadPhaseLayerInterface** out_broad_phase_layer_interface,
	JPH_ObjectLayerPairFilter** out_object_layer_pair_filter,
	JPH_ObjectVsBroadPhaseLayerFilter** out_object_vs_broad_phase_layer_filter)
{
	JPH_ObjectLayerPairFilter* object_layer_pair_filter = JPH_ObjectLayerPairFilterTable_Create(NUM_OBJECT_LAYERS);
	JPH_ObjectLayerPairFilterTable_EnableCollision(object_layer_pair_filter, LAYER_NON_MOVING, LAYER_MOVING);
	JPH_ObjectLayerPairFilterTable_EnableCollision(object_layer_pair_filter, LAYER_MOVING, LAYER_NON_MOVING);
	JPH_ObjectLayerPairFilterTable_EnableCollision(object_layer_pair_filter, LAYER_MOVING, LAYER_MOVING);

	JPH_BroadPhaseLayerInterface* broad_phase_layer_interface =
		JPH_BroadPhaseLayerInterfaceTable_Create(NUM_OBJECT_LAYERS, NUM_BROAD_PHASE_LAYERS);
	JPH_BroadPhaseLayerInterfaceTable_MapObjectToBroadPhaseLayer(
		broad_phase_layer_interface,
		LAYER_NON_MOVING,
		BP_LAYER_NON_MOVING);
	JPH_BroadPhaseLayerInterfaceTable_MapObjectToBroadPhaseLayer(
		broad_phase_layer_interface,
		LAYER_MOVING,
		BP_LAYER_MOVING);

	JPH_ObjectVsBroadPhaseLayerFilter* object_vs_broad_phase_layer_filter =
		JPH_ObjectVsBroadPhaseLayerFilterTable_Create(
			broad_phase_layer_interface,
			NUM_BROAD_PHASE_LAYERS,
			object_layer_pair_filter,
			NUM_OBJECT_LAYERS);

	*out_broad_phase_layer_interface = broad_phase_layer_interface;
	*out_object_layer_pair_filter = object_layer_pair_filter;
	*out_object_vs_broad_phase_layer_filter = object_vs_broad_phase_layer_filter;
	return true;
}

static JPH_BodyID create_floor(JPH_BodyInterface* body_interface)
{
	JPH_Vec3 half_extents = { 10.0f, 0.5f, 10.0f };
	JPH_BoxShape* shape = JPH_BoxShape_Create(&half_extents, JPH_DEFAULT_CONVEX_RADIUS);
	JPH_RVec3 position = { 0.0f, -0.5f, 0.0f };
	JPH_BodyCreationSettings* settings = JPH_BodyCreationSettings_Create3(
		(const JPH_Shape*)shape,
		&position,
		NULL,
		JPH_MotionType_Static,
		LAYER_NON_MOVING);

	JPH_BodyID body_id = JPH_BodyInterface_CreateAndAddBody(body_interface, settings, JPH_Activation_DontActivate);
	JPH_BodyCreationSettings_Destroy(settings);
	return body_id;
}

static JPH_BodyID create_sphere(JPH_BodyInterface* body_interface)
{
	JPH_SphereShape* shape = JPH_SphereShape_Create(0.5f);
	JPH_RVec3 position = { 0.0f, 3.0f, 0.0f };
	JPH_BodyCreationSettings* settings = JPH_BodyCreationSettings_Create3(
		(const JPH_Shape*)shape,
		&position,
		NULL,
		JPH_MotionType_Dynamic,
		LAYER_MOVING);

	JPH_BodyID body_id = JPH_BodyInterface_CreateAndAddBody(body_interface, settings, JPH_Activation_Activate);
	JPH_BodyCreationSettings_Destroy(settings);
	return body_id;
}

typedef struct viewer_sample_state {
	JPH_PhysicsSystem* system;
	JPH_TempAllocator* temp_allocator;
	JPH_JobSystem* job_system;
	JPH_DrawSettings draw_settings;
} viewer_sample_state;

static void draw_immediate_debug(JPH_Viewer* viewer)
{
	JPH_RVec3 line_from = { -2.0f, 0.05f, 0.0f };
	JPH_RVec3 line_to = { 2.0f, 0.05f, 0.0f };
	JPH_Viewer_DrawLine(viewer, &line_from, &line_to, 0xff0000ff);

	JPH_AABox box = { { -1.0f, 0.0f, -1.0f }, { 1.0f, 1.0f, 1.0f } };
	JPH_Viewer_DrawBox(viewer, &box, 0xff00ffff, JPH_DebugRenderer_CastShadow_Off, JPH_DebugRenderer_DrawMode_Wireframe);

	JPH_RVec3 sphere_center = { 0.0f, 1.5f, 2.0f };
	JPH_Viewer_DrawSphere(viewer, &sphere_center, 0.5f, 0xff00ff00, JPH_DebugRenderer_CastShadow_Off, JPH_DebugRenderer_DrawMode_Wireframe);

	JPH_Vec3 half_extents = { 0.35f, 0.35f, 0.35f };
	JPH_BoxShape* shape = JPH_BoxShape_Create(&half_extents, JPH_DEFAULT_CONVEX_RADIUS);
	JPH_RVec3 shape_position = { 2.0f, 1.0f, 0.0f };
	JPH_Quat shape_rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
	JPH_Vec3 shape_scale = { 1.0f, 1.0f, 1.0f };
	JPH_Viewer_DrawShape(viewer, (const JPH_Shape*)shape, &shape_position, &shape_rotation, &shape_scale, 0xffffff00, false, true);
	JPH_Shape_Destroy((JPH_Shape*)shape);
}

static bool draw_viewer_frame(JPH_Viewer* viewer, float delta_time, void* user_data)
{
	viewer_sample_state* state = (viewer_sample_state*)user_data;
	if (delta_time > 1.0f / 30.0f)
		delta_time = 1.0f / 30.0f;

	JPH_PhysicsSystem_Update(state->system, delta_time, 1, state->temp_allocator, state->job_system);
	JPH_Viewer_DrawBodies(viewer, state->system, &state->draw_settings, NULL);
	draw_immediate_debug(viewer);
	return true;
}

int main(int argc, char** argv)
{
	if (!JPH_Init())
		return 1;

	JPH_SetTraceHandler(trace_impl);

	JPH_JobSystem* job_system = JPH_JobSystemThreadPool_Create(NULL);
	JPH_TempAllocator* temp_allocator = JPH_TempAllocator_Create(8 * 1024 * 1024);

	JPH_BroadPhaseLayerInterface* broad_phase_layer_interface = NULL;
	JPH_ObjectLayerPairFilter* object_layer_pair_filter = NULL;
	JPH_ObjectVsBroadPhaseLayerFilter* object_vs_broad_phase_layer_filter = NULL;
	setup_layers(&broad_phase_layer_interface, &object_layer_pair_filter, &object_vs_broad_phase_layer_filter);

	JPH_PhysicsSystemSettings physics_settings = { 0 };
	physics_settings.maxBodies = 1024;
	physics_settings.numBodyMutexes = 0;
	physics_settings.maxBodyPairs = 1024;
	physics_settings.maxContactConstraints = 1024;
	physics_settings.broadPhaseLayerInterface = broad_phase_layer_interface;
	physics_settings.objectLayerPairFilter = object_layer_pair_filter;
	physics_settings.objectVsBroadPhaseLayerFilter = object_vs_broad_phase_layer_filter;

	JPH_PhysicsSystem* system = JPH_PhysicsSystem_Create(&physics_settings);
	JPH_BodyInterface* body_interface = JPH_PhysicsSystem_GetBodyInterface(system);

	JPH_BodyID floor_id = create_floor(body_interface);
	JPH_BodyID sphere_id = create_sphere(body_interface);

	JPH_Vec3 initial_velocity = { 1.5f, 0.0f, 0.0f };
	JPH_BodyInterface_SetLinearVelocity(body_interface, sphere_id, &initial_velocity);

	JPH_PhysicsSystem_OptimizeBroadPhase(system);

	JPH_DrawSettings draw_settings;
	JPH_DrawSettings_InitDefault(&draw_settings);
	draw_settings.drawShape = true;
	draw_settings.drawShapeWireframe = false;
	draw_settings.drawBoundingBox = true;
	draw_settings.drawCenterOfMassTransform = true;
	draw_settings.drawWorldTransform = true;
	draw_settings.drawVelocity = true;

	bool use_metal = argc > 1 && strcmp(argv[1], "--metal") == 0;
	bool use_vulkan = argc > 1 && strcmp(argv[1], "--vulkan") == 0;
	bool use_dx12 = argc > 1 && strcmp(argv[1], "--dx12") == 0;

	if (use_metal || use_vulkan || use_dx12)
	{
		JPH_ViewerSettings viewer_settings;
		JPH_ViewerSettings_InitDefault(&viewer_settings);
		const char* backend_name = use_metal ? "Metal" : (use_vulkan ? "Vulkan" : "DX12");
		viewer_settings.title = use_metal ? "Jolt Viewer C - Metal" : (use_vulkan ? "Jolt Viewer C - Vulkan" : "Jolt Viewer C - DX12");
		viewer_settings.backend = use_metal ? JPH_ViewerBackend_Metal : (use_vulkan ? JPH_ViewerBackend_Vulkan : JPH_ViewerBackend_DX12);

		JPH_Viewer* viewer = use_metal ? JPH_Viewer_CreateMetal(&viewer_settings) : (use_vulkan ? JPH_Viewer_CreateVulkan(&viewer_settings) : JPH_Viewer_CreateDX12(&viewer_settings));
		if (viewer == NULL)
		{
			fprintf(stderr, "Failed to create %s viewer\n", backend_name);
			JPH_BodyInterface_RemoveAndDestroyBody(body_interface, sphere_id);
			JPH_BodyInterface_RemoveAndDestroyBody(body_interface, floor_id);
			JPH_TempAllocator_Destroy(temp_allocator);
			JPH_JobSystem_Destroy(job_system);
			JPH_PhysicsSystem_Destroy(system);
			JPH_Shutdown();
			return 1;
		}

		viewer_sample_state state;
		state.system = system;
		state.temp_allocator = temp_allocator;
		state.job_system = job_system;
		state.draw_settings = draw_settings;
		JPH_Viewer_Run(viewer, draw_viewer_frame, &state);
		JPH_Viewer_Destroy(viewer);
	}
	else
	{
		for (int step = 0; step < 60; ++step)
			JPH_PhysicsSystem_Update(system, 1.0f / 60.0f, 1, temp_allocator, job_system);

		JPH_Viewer* viewer = JPH_Viewer_CreateObj("jolt_viewer_c_frame.obj");
		if (viewer == NULL)
		{
			fputs("Failed to create jolt_viewer_c_frame.obj\n", stderr);
			JPH_BodyInterface_RemoveAndDestroyBody(body_interface, sphere_id);
			JPH_BodyInterface_RemoveAndDestroyBody(body_interface, floor_id);
			JPH_TempAllocator_Destroy(temp_allocator);
			JPH_JobSystem_Destroy(job_system);
			JPH_PhysicsSystem_Destroy(system);
			JPH_Shutdown();
			return 1;
		}

		JPH_RVec3 camera_position = { -6.0f, 5.0f, -8.0f };
		JPH_Viewer_SetCameraPosition(viewer, &camera_position);

		JPH_Viewer_DrawBodies(viewer, system, &draw_settings, NULL);
		draw_immediate_debug(viewer);
		JPH_Viewer_NextFrame(viewer);
		JPH_Viewer_Flush(viewer);

		JPH_ViewerStats stats;
		JPH_Viewer_GetStats(viewer, &stats);
		JPH_Viewer_Destroy(viewer);

		printf("Wrote jolt_viewer_c_frame.obj: %u vertices, %u lines, %u triangles, %u text labels\n",
			stats.vertexCount,
			stats.lineCount,
			stats.triangleCount,
			stats.textCount);
	}

	JPH_BodyInterface_RemoveAndDestroyBody(body_interface, sphere_id);
	JPH_BodyInterface_RemoveAndDestroyBody(body_interface, floor_id);
	JPH_TempAllocator_Destroy(temp_allocator);
	JPH_JobSystem_Destroy(job_system);
	JPH_PhysicsSystem_Destroy(system);
	JPH_Shutdown();
	return 0;
}
