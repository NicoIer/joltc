// Test: RemoveBody after OnContactAdded and verify OnContactRemoved is triggered.
//
// Scenario:
//   1. A sphere falls onto a static floor
//   2. When OnContactAdded fires, we flag the sphere for removal
//   3. After the physics step, we call RemoveBody on the flagged sphere
//   4. On the next physics step, we check if OnContactRemoved fires

#include "joltc.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

static void TraceImpl(const char* message)
{
	std::cout << message << std::endl;
}

namespace Layers
{
	static constexpr JPH_ObjectLayer NON_MOVING = 0;
	static constexpr JPH_ObjectLayer MOVING = 1;
	static constexpr JPH_ObjectLayer NUM_LAYERS = 2;
};

namespace BroadPhaseLayers
{
	static constexpr JPH_BroadPhaseLayer NON_MOVING(0);
	static constexpr JPH_BroadPhaseLayer MOVING(1);
	static constexpr uint32_t NUM_LAYERS(2);
};

struct TestContext {
	JPH_PhysicsSystem* system = nullptr;
	JPH_BodyInterface* bodyInterface = nullptr;
	JPH_BodyID sphereId = {};
	bool contactAdded = false;
	bool contactRemoved = false;
	bool pendingRemove = false; // Flag: remove sphere after this physics step
};

static JPH_ValidateResult JPH_API_CALL OnContactValidate(
	void* userData,
	const JPH_Body* body1,
	const JPH_Body* body2,
	const JPH_RVec3* baseOffset,
	const JPH_CollideShapeResult* collisionResult)
{
	return JPH_ValidateResult_AcceptAllContactsForThisBodyPair;
}

static void JPH_API_CALL OnContactAdded(
	void* userData,
	const JPH_Body* body1,
	const JPH_Body* body2,
	const JPH_ContactManifold* manifold,
	JPH_ContactSettings* settings)
{
	auto* ctx = static_cast<TestContext*>(userData);
	JPH_BodyID id1 = JPH_Body_GetID(body1);
	JPH_BodyID id2 = JPH_Body_GetID(body2);

	std::cout << "[OnContactAdded] Body1=" << id1 << " Body2=" << id2 << std::endl;

	bool isSphere = (id1 == ctx->sphereId) || (id2 == ctx->sphereId);
	if (isSphere)
	{
		ctx->contactAdded = true;
		ctx->pendingRemove = true;
		std::cout << "[OnContactAdded] Flagged sphere (ID=" << ctx->sphereId << ") for removal" << std::endl;
	}
}

static void JPH_API_CALL OnContactPersisted(
	void* userData,
	const JPH_Body* body1,
	const JPH_Body* body2,
	const JPH_ContactManifold* manifold,
	JPH_ContactSettings* settings)
{
	JPH_BodyID id1 = JPH_Body_GetID(body1);
	JPH_BodyID id2 = JPH_Body_GetID(body2);
	std::cout << "[OnContactPersisted] Body1=" << id1 << " Body2=" << id2 << std::endl;
}

static void JPH_API_CALL OnContactRemoved(
	void* userData,
	const JPH_SubShapeIDPair* subShapePair)
{
	auto* ctx = static_cast<TestContext*>(userData);
	std::cout << "[OnContactRemoved] Body1=" << subShapePair->Body1ID
	          << " Body2=" << subShapePair->Body2ID << std::endl;

	bool isSphere = (subShapePair->Body1ID == ctx->sphereId) || (subShapePair->Body2ID == ctx->sphereId);
	if (isSphere)
	{
		ctx->contactRemoved = true;
		std::cout << "[OnContactRemoved] Sphere contact exit detected!" << std::endl;
	}
}

int main(void)
{
	if (!JPH_Init())
		return 1;

	JPH_SetTraceHandler(TraceImpl);

	JPH_JobSystem* jobSystem = JPH_JobSystemThreadPool_Create(nullptr);
	JPH_TempAllocator* tempAllocator = JPH_TempAllocator_Create(8 * 1024 * 1024);

	// Layer setup
	JPH_ObjectLayerPairFilter* objectLayerPairFilter = JPH_ObjectLayerPairFilterTable_Create(2);
	JPH_ObjectLayerPairFilterTable_EnableCollision(objectLayerPairFilter, Layers::NON_MOVING, Layers::MOVING);
	JPH_ObjectLayerPairFilterTable_EnableCollision(objectLayerPairFilter, Layers::MOVING, Layers::MOVING);

	JPH_BroadPhaseLayerInterface* broadPhaseLayerInterface = JPH_BroadPhaseLayerInterfaceTable_Create(2, 2);
	JPH_BroadPhaseLayerInterfaceTable_MapObjectToBroadPhaseLayer(broadPhaseLayerInterface, Layers::NON_MOVING, BroadPhaseLayers::NON_MOVING);
	JPH_BroadPhaseLayerInterfaceTable_MapObjectToBroadPhaseLayer(broadPhaseLayerInterface, Layers::MOVING, BroadPhaseLayers::MOVING);

	JPH_ObjectVsBroadPhaseLayerFilter* objectVsBroadPhaseLayerFilter = JPH_ObjectVsBroadPhaseLayerFilterTable_Create(broadPhaseLayerInterface, 2, objectLayerPairFilter, 2);

	JPH_PhysicsSystemSettings settings = {};
	settings.maxBodies = 1024;
	settings.numBodyMutexes = 0;
	settings.maxBodyPairs = 1024;
	settings.maxContactConstraints = 1024;
	settings.broadPhaseLayerInterface = broadPhaseLayerInterface;
	settings.objectLayerPairFilter = objectLayerPairFilter;
	settings.objectVsBroadPhaseLayerFilter = objectVsBroadPhaseLayerFilter;
	JPH_PhysicsSystem* system = JPH_PhysicsSystem_Create(&settings);
	JPH_BodyInterface* bodyInterface = JPH_PhysicsSystem_GetBodyInterface(system);

	// Setup test context
	TestContext ctx;
	ctx.system = system;
	ctx.bodyInterface = bodyInterface;

	// Setup contact listener
	JPH_ContactListener_Procs procs = {};
	procs.OnContactValidate = OnContactValidate;
	procs.OnContactAdded = OnContactAdded;
	procs.OnContactPersisted = OnContactPersisted;
	procs.OnContactRemoved = OnContactRemoved;
	JPH_ContactListener_SetProcs(&procs);
	JPH_ContactListener* contactListener = JPH_ContactListener_Create(&ctx);
	JPH_PhysicsSystem_SetContactListener(system, contactListener);

	// Create floor (static)
	JPH_BodyID floorId;
	{
		JPH_Vec3 boxHalfExtents = { 100.0f, 1.0f, 100.0f };
		JPH_BoxShape* floorShape = JPH_BoxShape_Create(&boxHalfExtents, JPH_DEFAULT_CONVEX_RADIUS);
		JPH_Vec3 floorPosition = { 0.0f, -1.0f, 0.0f };
		JPH_BodyCreationSettings* floorSettings = JPH_BodyCreationSettings_Create3(
			(const JPH_Shape*)floorShape,
			&floorPosition,
			nullptr,
			JPH_MotionType_Static,
			Layers::NON_MOVING);
		floorId = JPH_BodyInterface_CreateAndAddBody(bodyInterface, floorSettings, JPH_Activation_DontActivate);
		JPH_BodyCreationSettings_Destroy(floorSettings);
	}

	// Create sphere (dynamic) - starts above the floor, will fall and collide
	{
		JPH_SphereShape* sphereShape = JPH_SphereShape_Create(0.5f);
		JPH_Vec3 spherePosition = { 0.0f, 2.0f, 0.0f };
		JPH_BodyCreationSettings* sphereSettings = JPH_BodyCreationSettings_Create3(
			(const JPH_Shape*)sphereShape,
			&spherePosition,
			nullptr,
			JPH_MotionType_Dynamic,
			Layers::MOVING);
		ctx.sphereId = JPH_BodyInterface_CreateAndAddBody(bodyInterface, sphereSettings, JPH_Activation_Activate);
		JPH_BodyCreationSettings_Destroy(sphereSettings);
	}

	std::cout << "=== Sphere ID: " << ctx.sphereId << " ===" << std::endl;
	std::cout << "=== Floor ID: " << floorId << " ===" << std::endl;
	std::cout << "=== Simulating: sphere falls onto floor ===" << std::endl;
	std::cout << "=== On contact, sphere will be flagged for removal ===" << std::endl;
	std::cout << "=== After physics step, RemoveBody is called ===" << std::endl;
	std::cout << "=== Then we check if OnContactRemoved fires on subsequent steps ===" << std::endl;
	std::cout << std::endl;

	JPH_PhysicsSystem_OptimizeBroadPhase(system);

	const float cDeltaTime = 1.0f / 60.0f;
	const int maxSteps = 300;
	bool removed = false;
	int stepsAfterRemove = 0;

	for (int step = 0; step < maxSteps; ++step)
	{
		JPH_PhysicsSystem_Update(system, cDeltaTime, 1, tempAllocator, jobSystem);

		// After physics step: if flagged, remove the sphere now
		if (ctx.pendingRemove && !removed)
		{
			std::cout << "[Step " << step << "] Removing sphere body after physics step..." << std::endl;
			JPH_BodyInterface_RemoveBody(bodyInterface, ctx.sphereId);
			removed = true;
			ctx.pendingRemove = false;
		}

		// Run a few more steps after removal to allow OnContactRemoved to fire
		if (removed)
		{
			stepsAfterRemove++;
			if (stepsAfterRemove >= 5)
			{
				std::cout << std::endl;
				std::cout << "=== Simulation ended at step " << step << " ===" << std::endl;
				break;
			}
		}
	}

	// Report results
	std::cout << std::endl;
	std::cout << "========== RESULTS ==========" << std::endl;
	std::cout << "OnContactAdded fired:   " << (ctx.contactAdded ? "YES" : "NO") << std::endl;
	std::cout << "OnContactRemoved fired: " << (ctx.contactRemoved ? "YES" : "NO") << std::endl;

	if (ctx.contactAdded && ctx.contactRemoved)
		std::cout << "PASS: RemoveBody after OnContactAdded triggers OnContactRemoved" << std::endl;
	else if (ctx.contactAdded && !ctx.contactRemoved)
		std::cout << "FAIL: RemoveBody after OnContactAdded does NOT trigger OnContactRemoved" << std::endl;
	else
		std::cout << "FAIL: OnContactAdded was never triggered (sphere may not have reached the floor)" << std::endl;

	std::cout << "========== END ==========" << std::endl;

	// Cleanup
	if (removed)
		JPH_BodyInterface_DestroyBody(bodyInterface, ctx.sphereId);
	else
		JPH_BodyInterface_RemoveAndDestroyBody(bodyInterface, ctx.sphereId);
	JPH_BodyInterface_RemoveAndDestroyBody(bodyInterface, floorId);

	JPH_ContactListener_Destroy(contactListener);
	JPH_TempAllocator_Destroy(tempAllocator);
	JPH_JobSystem_Destroy(jobSystem);
	JPH_PhysicsSystem_Destroy(system);
	JPH_Shutdown();
	return 0;
}
