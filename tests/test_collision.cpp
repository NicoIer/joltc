// Copyright (c) Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#include <gtest/gtest.h>
#include "joltc.h"

namespace BroadPhaseLayers {
    static constexpr JPH_BroadPhaseLayer NON_MOVING = 0;
    static constexpr JPH_BroadPhaseLayer MOVING = 1;
    static constexpr uint32_t NUM_LAYERS = 2;
}

namespace ObjectLayers {
    static constexpr JPH_ObjectLayer NON_MOVING = 0;
    static constexpr JPH_ObjectLayer MOVING = 1;
    static constexpr uint32_t NUM_LAYERS = 2;
}

class CollisionTest : public ::testing::Test {
protected:
    JPH_PhysicsSystem* physicsSystem = nullptr;
    JPH_BodyInterface* bodyInterface = nullptr;
    JPH_BroadPhaseLayerInterface* bpLayer = nullptr;
    JPH_ObjectLayerPairFilter* objPairFilter = nullptr;
    JPH_ObjectVsBroadPhaseLayerFilter* objVsBpFilter = nullptr;

    void SetUp() override {
        ASSERT_TRUE(JPH_Init());

        bpLayer = JPH_BroadPhaseLayerInterfaceTable_Create(ObjectLayers::NUM_LAYERS, BroadPhaseLayers::NUM_LAYERS);
        JPH_BroadPhaseLayerInterfaceTable_MapObjectToBroadPhaseLayer(bpLayer, ObjectLayers::NON_MOVING, BroadPhaseLayers::NON_MOVING);
        JPH_BroadPhaseLayerInterfaceTable_MapObjectToBroadPhaseLayer(bpLayer, ObjectLayers::MOVING, BroadPhaseLayers::MOVING);

        objPairFilter = JPH_ObjectLayerPairFilterTable_Create(ObjectLayers::NUM_LAYERS);
        JPH_ObjectLayerPairFilterTable_EnableCollision(objPairFilter, ObjectLayers::NON_MOVING, ObjectLayers::MOVING);
        JPH_ObjectLayerPairFilterTable_EnableCollision(objPairFilter, ObjectLayers::MOVING, ObjectLayers::MOVING);

        objVsBpFilter = JPH_ObjectVsBroadPhaseLayerFilterTable_Create(
            bpLayer, BroadPhaseLayers::NUM_LAYERS,
            objPairFilter, ObjectLayers::NUM_LAYERS);

        JPH_PhysicsSystemSettings settings = {};
        settings.maxBodies = 1024;
        settings.maxBodyPairs = 1024;
        settings.maxContactConstraints = 1024;
        settings.broadPhaseLayerInterface = bpLayer;
        settings.objectVsBroadPhaseLayerFilter = objVsBpFilter;
        settings.objectLayerPairFilter = objPairFilter;

        physicsSystem = JPH_PhysicsSystem_Create(&settings);
        bodyInterface = JPH_PhysicsSystem_GetBodyInterface(physicsSystem);
    }

    void TearDown() override {
        if (physicsSystem) JPH_PhysicsSystem_Destroy(physicsSystem);
        JPH_Shutdown();
    }

    JPH_BodyID CreateSphereBody(float x, float y, float z, float radius, JPH_MotionType motionType, JPH_ObjectLayer layer) {
        JPH_SphereShape* shape = JPH_SphereShape_Create(radius);
        JPH_Vec3 position = {x, y, z};
        JPH_Quat rotation = {0.0f, 0.0f, 0.0f, 1.0f};

        JPH_BodyCreationSettings* bs = JPH_BodyCreationSettings_Create3((JPH_Shape*)shape, &position, &rotation, motionType, layer);
        JPH_Body* body = JPH_BodyInterface_CreateBody(bodyInterface, bs);
        JPH_BodyID bodyId = JPH_Body_GetID(body);
        JPH_BodyInterface_AddBody(bodyInterface, bodyId, JPH_Activation_Activate);

        JPH_BodyCreationSettings_Destroy(bs);
        JPH_Shape_Destroy((JPH_Shape*)shape);
        return bodyId;
    }

    void RemoveAndDestroyBody(JPH_BodyID bodyId) {
        JPH_BodyInterface_RemoveBody(bodyInterface, bodyId);
        JPH_BodyInterface_DestroyBody(bodyInterface, bodyId);
    }
};

TEST_F(CollisionTest, GetNarrowPhaseQuery) {
    const JPH_NarrowPhaseQuery* query = JPH_PhysicsSystem_GetNarrowPhaseQuery(physicsSystem);
    EXPECT_NE(query, nullptr);
}

TEST_F(CollisionTest, GetNarrowPhaseQueryNoLock) {
    const JPH_NarrowPhaseQuery* query = JPH_PhysicsSystem_GetNarrowPhaseQueryNoLock(physicsSystem);
    EXPECT_NE(query, nullptr);
}

TEST_F(CollisionTest, RayCast_Hit) {
    JPH_BodyID sphereId = CreateSphereBody(0.0f, 0.0f, 0.0f, 1.0f, JPH_MotionType_Static, ObjectLayers::NON_MOVING);
    JPH_PhysicsSystem_OptimizeBroadPhase(physicsSystem);

    const JPH_NarrowPhaseQuery* query = JPH_PhysicsSystem_GetNarrowPhaseQuery(physicsSystem);

    JPH_RVec3 origin = {-5.0f, 0.0f, 0.0f};
    JPH_Vec3 direction = {10.0f, 0.0f, 0.0f};
    JPH_RayCastResult hit;

    bool hasHit = JPH_NarrowPhaseQuery_CastRay(query, &origin, &direction, &hit, nullptr, nullptr, nullptr);
    EXPECT_TRUE(hasHit);
    EXPECT_NEAR(hit.fraction, 0.4f, 0.01f);
    EXPECT_EQ(hit.bodyID, sphereId);

    RemoveAndDestroyBody(sphereId);
}

TEST_F(CollisionTest, RayCast_Miss) {
    JPH_BodyID sphereId = CreateSphereBody(0.0f, 0.0f, 0.0f, 1.0f, JPH_MotionType_Static, ObjectLayers::NON_MOVING);
    JPH_PhysicsSystem_OptimizeBroadPhase(physicsSystem);

    const JPH_NarrowPhaseQuery* query = JPH_PhysicsSystem_GetNarrowPhaseQuery(physicsSystem);

    JPH_RVec3 origin = {0.0f, 10.0f, 0.0f};
    JPH_Vec3 direction = {10.0f, 0.0f, 0.0f};
    JPH_RayCastResult hit;

    bool hasHit = JPH_NarrowPhaseQuery_CastRay(query, &origin, &direction, &hit, nullptr, nullptr, nullptr);
    EXPECT_FALSE(hasHit);

    RemoveAndDestroyBody(sphereId);
}

TEST_F(CollisionTest, RayCast_ClosestHit) {
    JPH_BodyID sphere1Id = CreateSphereBody(-3.0f, 0.0f, 0.0f, 1.0f, JPH_MotionType_Static, ObjectLayers::NON_MOVING);
    JPH_BodyID sphere2Id = CreateSphereBody(3.0f, 0.0f, 0.0f, 1.0f, JPH_MotionType_Dynamic, ObjectLayers::MOVING);
    JPH_PhysicsSystem_OptimizeBroadPhase(physicsSystem);

    const JPH_NarrowPhaseQuery* query = JPH_PhysicsSystem_GetNarrowPhaseQuery(physicsSystem);

    JPH_RVec3 origin = {-10.0f, 0.0f, 0.0f};
    JPH_Vec3 direction = {20.0f, 0.0f, 0.0f};
    JPH_RayCastResult hit;

    bool hasHit = JPH_NarrowPhaseQuery_CastRay(query, &origin, &direction, &hit, nullptr, nullptr, nullptr);
    EXPECT_TRUE(hasHit);
    EXPECT_EQ(hit.bodyID, sphere1Id);

    RemoveAndDestroyBody(sphere1Id);
    RemoveAndDestroyBody(sphere2Id);
}

TEST_F(CollisionTest, CastShape_DetectsHit) {
    JPH_BodyID sphereId = CreateSphereBody(0.0f, 0.0f, 0.0f, 1.0f, JPH_MotionType_Static, ObjectLayers::NON_MOVING);
    JPH_PhysicsSystem_OptimizeBroadPhase(physicsSystem);

    const JPH_NarrowPhaseQuery* query = JPH_PhysicsSystem_GetNarrowPhaseQuery(physicsSystem);

    JPH_SphereShape* castShape = JPH_SphereShape_Create(0.5f);
    JPH_RVec3 start = {-5.0f, 0.0f, 0.0f};
    JPH_RMat4 worldTransform;
#if defined(JPH_DOUBLE_PRECISION)
    JPH_RMat4_Translation(&worldTransform, &start);
#else
    JPH_Mat4_Translation(&worldTransform, &start);
#endif

    JPH_Vec3 direction = {10.0f, 0.0f, 0.0f};
    JPH_ShapeCastSettings settings;
    JPH_ShapeCastSettings_Init(&settings);
    JPH_RVec3 baseOffset = {0.0f, 0.0f, 0.0f};

    struct CastShapeHitContext {
        int hitCount = 0;
        JPH_ShapeCastResult firstHit = {};
    } context;

    auto callback = [](void* userData, const JPH_ShapeCastResult* result) -> float {
        auto* hitContext = static_cast<CastShapeHitContext*>(userData);
        if (hitContext->hitCount == 0)
            hitContext->firstHit = *result;
        ++hitContext->hitCount;
        return result->fraction;
    };

    bool hasHit = JPH_NarrowPhaseQuery_CastShape(query,
        (const JPH_Shape*)castShape,
        &worldTransform,
        &direction,
        &settings,
        &baseOffset,
        callback,
        &context,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    EXPECT_TRUE(hasHit);
    ASSERT_GE(context.hitCount, 1);
    EXPECT_EQ(context.firstHit.bodyID2, sphereId);
    EXPECT_NEAR(context.firstHit.fraction, 0.35f, 0.01f);

    JPH_Shape_Destroy((JPH_Shape*)castShape);
    RemoveAndDestroyBody(sphereId);
}

TEST_F(CollisionTest, CastShape2_DetectsHit) {
    JPH_BodyID sphereId = CreateSphereBody(0.0f, 0.0f, 0.0f, 1.0f, JPH_MotionType_Static, ObjectLayers::NON_MOVING);
    JPH_PhysicsSystem_OptimizeBroadPhase(physicsSystem);

    const JPH_NarrowPhaseQuery* query = JPH_PhysicsSystem_GetNarrowPhaseQuery(physicsSystem);

    JPH_SphereShape* castShape = JPH_SphereShape_Create(0.5f);
    JPH_RVec3 start = {-5.0f, 0.0f, 0.0f};
    JPH_RMat4 worldTransform;
#if defined(JPH_DOUBLE_PRECISION)
    JPH_RMat4_Translation(&worldTransform, &start);
#else
    JPH_Mat4_Translation(&worldTransform, &start);
#endif

    JPH_Vec3 direction = {10.0f, 0.0f, 0.0f};
    JPH_ShapeCastSettings settings;
    JPH_ShapeCastSettings_Init(&settings);
    JPH_RVec3 baseOffset = {0.0f, 0.0f, 0.0f};

    struct CastShapeHitContext {
        int hitCount = 0;
        JPH_ShapeCastResult firstHit = {};
    } context;

    auto callback = [](void* userData, const JPH_ShapeCastResult* result) {
        auto* hitContext = static_cast<CastShapeHitContext*>(userData);
        if (hitContext->hitCount == 0)
            hitContext->firstHit = *result;
        ++hitContext->hitCount;
    };

    bool hasHit = JPH_NarrowPhaseQuery_CastShape2(query,
        (const JPH_Shape*)castShape,
        &worldTransform,
        &direction,
        &settings,
        &baseOffset,
        JPH_CollisionCollectorType_AllHit,
        callback,
        &context,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    EXPECT_TRUE(hasHit);
    ASSERT_GE(context.hitCount, 1);
    EXPECT_EQ(context.firstHit.bodyID2, sphereId);
    EXPECT_NEAR(context.firstHit.fraction, 0.35f, 0.01f);

    JPH_Shape_Destroy((JPH_Shape*)castShape);
    RemoveAndDestroyBody(sphereId);
}

TEST_F(CollisionTest, GetBroadPhaseQuery) {
    const JPH_BroadPhaseQuery* query = JPH_PhysicsSystem_GetBroadPhaseQuery(physicsSystem);
    EXPECT_NE(query, nullptr);
}
