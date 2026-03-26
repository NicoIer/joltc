// joltphysics_helpers.cpp
// Implementation of the SWIG-friendly wrapper around JoltPhysics.

#include "joltphysics_helpers.h"

// Jolt includes
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

#include <thread>
#include <cstdarg>

// ============================================================================
// Jolt <-> Wrapper conversion helpers
// ============================================================================

static JPH::Vec3 ToJolt(const JoltVec3& v) {
    return JPH::Vec3(v.x, v.y, v.z);
}

static JoltVec3 FromJolt(JPH::Vec3 v) {
    return JoltVec3(v.GetX(), v.GetY(), v.GetZ());
}

#ifdef JPH_DOUBLE_PRECISION
static JPH::RVec3 ToJoltR(const JoltVec3& v) {
    return JPH::RVec3(v.x, v.y, v.z);
}
static JoltVec3 FromJoltR(JPH::RVec3 v) {
    return JoltVec3((float)v.GetX(), (float)v.GetY(), (float)v.GetZ());
}
#else
static JPH::RVec3 ToJoltR(const JoltVec3& v) {
    return ToJolt(v);
}
static JoltVec3 FromJoltR(JPH::RVec3 v) {
    return FromJolt(v);
}
#endif

static JPH::Quat ToJolt(const JoltQuat& q) {
    return JPH::Quat(q.x, q.y, q.z, q.w);
}

static JoltQuat FromJolt(JPH::Quat q) {
    return JoltQuat(q.GetX(), q.GetY(), q.GetZ(), q.GetW());
}

static JPH::EMotionType ToJolt(JoltMotionType mt) {
    return static_cast<JPH::EMotionType>(mt);
}

static JPH::EActivation ToJolt(JoltActivation a) {
    return static_cast<JPH::EActivation>(a);
}

static JPH::BodyID ToJolt(JoltBodyID id) {
    return JPH::BodyID(id.GetIndexAndSequenceNumber());
}

static JoltBodyID FromJolt(JPH::BodyID id) {
    return JoltBodyID(id.GetIndexAndSequenceNumber());
}

// ============================================================================
// BodyID implementation
// ============================================================================

JoltBodyID::JoltBodyID() : mID(0xffffffff) {}
JoltBodyID::JoltBodyID(uint32_t id) : mID(id) {}

uint32_t JoltBodyID::GetIndex() const {
    return mID & 0x7fffff;
}

uint8_t JoltBodyID::GetSequenceNumber() const {
    return uint8_t(mID >> 23);
}

uint32_t JoltBodyID::GetIndexAndSequenceNumber() const {
    return mID;
}

bool JoltBodyID::IsInvalid() const {
    return mID == 0xffffffff;
}

bool JoltBodyID::operator==(const JoltBodyID& other) const {
    return mID == other.mID;
}

bool JoltBodyID::operator!=(const JoltBodyID& other) const {
    return mID != other.mID;
}

// ============================================================================
// Jolt trace/assert callbacks (required by Jolt)
// ============================================================================

static void TraceImpl(const char* inFMT, ...) {
    va_list list;
    va_start(list, inFMT);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), inFMT, list);
    va_end(list);
    // Could route to C# logger in the future
    fprintf(stderr, "[JoltPhysics] %s\n", buffer);
}

#ifdef JPH_ENABLE_ASSERTS
static bool AssertFailedImpl(const char* inExpression, const char* inMessage,
                              const char* inFile, unsigned int inLine) {
    fprintf(stderr, "[JoltPhysics] Assert failed: %s (%s) at %s:%u\n",
            inExpression, inMessage ? inMessage : "", inFile, inLine);
    return true; // break into debugger
}
#endif

// ============================================================================
// Global init/shutdown
// ============================================================================

void JoltPhysics_Init() {
    JPH::RegisterDefaultAllocator();
    JPH::Trace = TraceImpl;
    JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl;)
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
}

void JoltPhysics_Shutdown() {
    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}

// ============================================================================
// Bridge classes: translate between SWIG Director callbacks and Jolt interfaces
// ============================================================================

class JoltPhysicsSystem::BroadPhaseLayerBridge final : public JPH::BroadPhaseLayerInterface {
public:
    BroadPhaseLayerBridge(JoltBroadPhaseLayerInterface* wrapper) : mWrapper(wrapper) {}

    JPH::uint GetNumBroadPhaseLayers() const override {
        return mWrapper->GetNumBroadPhaseLayers();
    }

    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        return JPH::BroadPhaseLayer(mWrapper->GetBroadPhaseLayer(inLayer));
    }

private:
    JoltBroadPhaseLayerInterface* mWrapper;
};

class JoltPhysicsSystem::ObjectVsBroadPhaseLayerFilterBridge final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    ObjectVsBroadPhaseLayerFilterBridge(JoltObjectVsBroadPhaseLayerFilter* wrapper) : mWrapper(wrapper) {}

    bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
        return mWrapper->ShouldCollide(inLayer1, inLayer2.GetValue());
    }

private:
    JoltObjectVsBroadPhaseLayerFilter* mWrapper;
};

class JoltPhysicsSystem::ObjectLayerPairFilterBridge final : public JPH::ObjectLayerPairFilter {
public:
    ObjectLayerPairFilterBridge(JoltObjectLayerPairFilter* wrapper) : mWrapper(wrapper) {}

    bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const override {
        return mWrapper->ShouldCollide(inLayer1, inLayer2);
    }

private:
    JoltObjectLayerPairFilter* mWrapper;
};

class JoltPhysicsSystem::ContactListenerBridge final : public JPH::ContactListener {
public:
    ContactListenerBridge(JoltContactListener* wrapper) : mWrapper(wrapper) {}

    JPH::ValidateResult OnContactValidate(
        const JPH::Body& inBody1, const JPH::Body& inBody2,
        JPH::RVec3Arg inBaseOffset,
        const JPH::CollideShapeResult& inCollisionResult) override
    {
        JoltVec3 baseOffset = FromJoltR(inBaseOffset);
        JoltBodyID b1 = FromJolt(inBody1.GetID());
        JoltBodyID b2 = FromJolt(inBody2.GetID());
        JoltValidateResult result = mWrapper->OnContactValidate(b1, b2, baseOffset);
        return static_cast<JPH::ValidateResult>(result);
    }

    void OnContactAdded(
        const JPH::Body& inBody1, const JPH::Body& inBody2,
        const JPH::ContactManifold& inManifold,
        JPH::ContactSettings& ioSettings) override
    {
        JoltContactManifoldInfo manifold;
        manifold.baseOffset = FromJoltR(inManifold.mBaseOffset);
        manifold.worldSpaceNormal = FromJolt(inManifold.mWorldSpaceNormal);
        manifold.penetrationDepth = inManifold.mPenetrationDepth;

        JoltContactSettings settings;
        settings.combinedFriction = ioSettings.mCombinedFriction;
        settings.combinedRestitution = ioSettings.mCombinedRestitution;
        settings.isSensor = ioSettings.mIsSensor;

        JoltBodyID b1 = FromJolt(inBody1.GetID());
        JoltBodyID b2 = FromJolt(inBody2.GetID());
        mWrapper->OnContactAdded(b1, b2, manifold, settings);

        // Write back modified settings
        ioSettings.mCombinedFriction = settings.combinedFriction;
        ioSettings.mCombinedRestitution = settings.combinedRestitution;
        ioSettings.mIsSensor = settings.isSensor;
    }

    void OnContactPersisted(
        const JPH::Body& inBody1, const JPH::Body& inBody2,
        const JPH::ContactManifold& inManifold,
        JPH::ContactSettings& ioSettings) override
    {
        JoltContactManifoldInfo manifold;
        manifold.baseOffset = FromJoltR(inManifold.mBaseOffset);
        manifold.worldSpaceNormal = FromJolt(inManifold.mWorldSpaceNormal);
        manifold.penetrationDepth = inManifold.mPenetrationDepth;

        JoltContactSettings settings;
        settings.combinedFriction = ioSettings.mCombinedFriction;
        settings.combinedRestitution = ioSettings.mCombinedRestitution;
        settings.isSensor = ioSettings.mIsSensor;

        JoltBodyID b1 = FromJolt(inBody1.GetID());
        JoltBodyID b2 = FromJolt(inBody2.GetID());
        mWrapper->OnContactPersisted(b1, b2, manifold, settings);

        ioSettings.mCombinedFriction = settings.combinedFriction;
        ioSettings.mCombinedRestitution = settings.combinedRestitution;
        ioSettings.mIsSensor = settings.isSensor;
    }

    void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override {
        JoltBodyID b1 = FromJolt(inSubShapePair.GetBody1ID());
        JoltBodyID b2 = FromJolt(inSubShapePair.GetBody2ID());
        mWrapper->OnContactRemoved(b1, b2);
    }

private:
    JoltContactListener* mWrapper;
};

class JoltPhysicsSystem::BodyActivationListenerBridge final : public JPH::BodyActivationListener {
public:
    BodyActivationListenerBridge(JoltBodyActivationListener* wrapper) : mWrapper(wrapper) {}

    void OnBodyActivated(const JPH::BodyID& inBodyID, uint64_t inBodyUserData) override {
        mWrapper->OnBodyActivated(FromJolt(inBodyID), inBodyUserData);
    }

    void OnBodyDeactivated(const JPH::BodyID& inBodyID, uint64_t inBodyUserData) override {
        mWrapper->OnBodyDeactivated(FromJolt(inBodyID), inBodyUserData);
    }

private:
    JoltBodyActivationListener* mWrapper;
};

// ============================================================================
// JoltPhysicsSystem implementation
// ============================================================================

JoltPhysicsSystem::JoltPhysicsSystem() {}

JoltPhysicsSystem::~JoltPhysicsSystem() {
    delete mContactListenerBridge;
    delete mBodyActivationListenerBridge;
    delete mBroadPhaseBridge;
    delete mObjectVsBroadPhaseBridge;
    delete mObjectLayerPairBridge;
    delete mPhysicsSystem;
    delete mTempAllocator;
    delete mJobSystem;
}

bool JoltPhysicsSystem::Init(
    unsigned int maxBodies,
    unsigned int numBodyMutexes,
    unsigned int maxBodyPairs,
    unsigned int maxContactConstraints,
    JoltBroadPhaseLayerInterface* broadPhaseLayerInterface,
    JoltObjectVsBroadPhaseLayerFilter* objectVsBroadPhaseFilter,
    JoltObjectLayerPairFilter* objectLayerPairFilter)
{
    if (mInitialized) return false;

    // Create temp allocator (10 MB)
    mTempAllocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);

    // Create job system
    int numThreads = (int)std::thread::hardware_concurrency() - 1;
    if (numThreads < 1) numThreads = 1;
    mJobSystem = new JPH::JobSystemThreadPool(
        JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, numThreads);

    // Create bridge objects
    mBroadPhaseBridge = new BroadPhaseLayerBridge(broadPhaseLayerInterface);
    mObjectVsBroadPhaseBridge = new ObjectVsBroadPhaseLayerFilterBridge(objectVsBroadPhaseFilter);
    mObjectLayerPairBridge = new ObjectLayerPairFilterBridge(objectLayerPairFilter);

    // Create physics system
    mPhysicsSystem = new JPH::PhysicsSystem();
    mPhysicsSystem->Init(
        maxBodies, numBodyMutexes, maxBodyPairs, maxContactConstraints,
        *mBroadPhaseBridge, *mObjectVsBroadPhaseBridge, *mObjectLayerPairBridge);

    mInitialized = true;
    return true;
}

JoltPhysicsUpdateError JoltPhysicsSystem::Update(float deltaTime, int collisionSteps) {
    if (!mInitialized) return JoltPhysicsUpdateError::None;
    JPH::EPhysicsUpdateError err = mPhysicsSystem->Update(
        deltaTime, collisionSteps, mTempAllocator, mJobSystem);
    return static_cast<JoltPhysicsUpdateError>(err);
}

void JoltPhysicsSystem::OptimizeBroadPhase() {
    if (mInitialized) mPhysicsSystem->OptimizeBroadPhase();
}

void JoltPhysicsSystem::SetGravity(const JoltVec3& gravity) {
    if (mInitialized) mPhysicsSystem->SetGravity(ToJolt(gravity));
}

JoltVec3 JoltPhysicsSystem::GetGravity() const {
    if (!mInitialized) return JoltVec3(0, -9.81f, 0);
    return FromJolt(mPhysicsSystem->GetGravity());
}

void JoltPhysicsSystem::SetContactListener(JoltContactListener* listener) {
    if (!mInitialized) return;
    delete mContactListenerBridge;
    if (listener) {
        mContactListenerBridge = new ContactListenerBridge(listener);
        mPhysicsSystem->SetContactListener(mContactListenerBridge);
    } else {
        mContactListenerBridge = nullptr;
        mPhysicsSystem->SetContactListener(nullptr);
    }
}

void JoltPhysicsSystem::SetBodyActivationListener(JoltBodyActivationListener* listener) {
    if (!mInitialized) return;
    delete mBodyActivationListenerBridge;
    if (listener) {
        mBodyActivationListenerBridge = new BodyActivationListenerBridge(listener);
        mPhysicsSystem->SetBodyActivationListener(mBodyActivationListenerBridge);
    } else {
        mBodyActivationListenerBridge = nullptr;
        mPhysicsSystem->SetBodyActivationListener(nullptr);
    }
}

unsigned int JoltPhysicsSystem::GetNumBodies() const {
    return mInitialized ? mPhysicsSystem->GetNumBodies() : 0;
}

unsigned int JoltPhysicsSystem::GetMaxBodies() const {
    return mInitialized ? mPhysicsSystem->GetMaxBodies() : 0;
}

// --- Shape creation ---

void* JoltPhysicsSystem::CreateBoxShape(const JoltVec3& halfExtent, float convexRadius) {
    JPH::BoxShape* shape = new JPH::BoxShape(ToJolt(halfExtent), convexRadius);
    shape->AddRef();
    return static_cast<void*>(shape);
}

void* JoltPhysicsSystem::CreateSphereShape(float radius) {
    JPH::SphereShape* shape = new JPH::SphereShape(radius);
    shape->AddRef();
    return static_cast<void*>(shape);
}

void JoltPhysicsSystem::ReleaseShape(void* shape) {
    if (shape) {
        static_cast<JPH::Shape*>(shape)->Release();
    }
}

// --- Body management ---

JoltBodyID JoltPhysicsSystem::CreateAndAddBody(
    const JoltBodyCreationSettings& settings, void* shape, JoltActivation activation)
{
    if (!mInitialized || !shape) return JoltBodyID();

    JPH::BodyCreationSettings joltSettings(
        static_cast<const JPH::Shape*>(shape),
        ToJoltR(settings.position),
        ToJolt(settings.rotation),
        ToJolt(settings.motionType),
        settings.objectLayer);

    joltSettings.mLinearVelocity = ToJolt(settings.linearVelocity);
    joltSettings.mAngularVelocity = ToJolt(settings.angularVelocity);
    joltSettings.mUserData = settings.userData;
    joltSettings.mAllowSleeping = settings.allowSleeping;
    joltSettings.mFriction = settings.friction;
    joltSettings.mRestitution = settings.restitution;
    joltSettings.mLinearDamping = settings.linearDamping;
    joltSettings.mAngularDamping = settings.angularDamping;
    joltSettings.mGravityFactor = settings.gravityFactor;

    JPH::BodyID bodyID = mPhysicsSystem->GetBodyInterface().CreateAndAddBody(
        joltSettings, ToJolt(activation));

    return FromJolt(bodyID);
}

void JoltPhysicsSystem::RemoveBody(JoltBodyID bodyID) {
    if (mInitialized)
        mPhysicsSystem->GetBodyInterface().RemoveBody(ToJolt(bodyID));
}

void JoltPhysicsSystem::DestroyBody(JoltBodyID bodyID) {
    if (mInitialized)
        mPhysicsSystem->GetBodyInterface().DestroyBody(ToJolt(bodyID));
}

void JoltPhysicsSystem::RemoveAndDestroyBody(JoltBodyID bodyID) {
    if (!mInitialized) return;
    JPH::BodyInterface& bi = mPhysicsSystem->GetBodyInterface();
    JPH::BodyID jid = ToJolt(bodyID);
    bi.RemoveBody(jid);
    bi.DestroyBody(jid);
}

// --- Body properties ---

JoltVec3 JoltPhysicsSystem::GetBodyPosition(JoltBodyID bodyID) const {
    if (!mInitialized) return JoltVec3();
    return FromJoltR(mPhysicsSystem->GetBodyInterface().GetPosition(ToJolt(bodyID)));
}

JoltQuat JoltPhysicsSystem::GetBodyRotation(JoltBodyID bodyID) const {
    if (!mInitialized) return JoltQuat();
    return FromJolt(mPhysicsSystem->GetBodyInterface().GetRotation(ToJolt(bodyID)));
}

JoltVec3 JoltPhysicsSystem::GetBodyLinearVelocity(JoltBodyID bodyID) const {
    if (!mInitialized) return JoltVec3();
    return FromJolt(mPhysicsSystem->GetBodyInterface().GetLinearVelocity(ToJolt(bodyID)));
}

JoltVec3 JoltPhysicsSystem::GetBodyAngularVelocity(JoltBodyID bodyID) const {
    if (!mInitialized) return JoltVec3();
    return FromJolt(mPhysicsSystem->GetBodyInterface().GetAngularVelocity(ToJolt(bodyID)));
}

void JoltPhysicsSystem::SetBodyPosition(JoltBodyID bodyID, const JoltVec3& position, JoltActivation activation) {
    if (mInitialized)
        mPhysicsSystem->GetBodyInterface().SetPosition(ToJolt(bodyID), ToJoltR(position), ToJolt(activation));
}

void JoltPhysicsSystem::SetBodyRotation(JoltBodyID bodyID, const JoltQuat& rotation, JoltActivation activation) {
    if (mInitialized)
        mPhysicsSystem->GetBodyInterface().SetRotation(ToJolt(bodyID), ToJolt(rotation), ToJolt(activation));
}

void JoltPhysicsSystem::SetBodyLinearVelocity(JoltBodyID bodyID, const JoltVec3& velocity) {
    if (mInitialized)
        mPhysicsSystem->GetBodyInterface().SetLinearVelocity(ToJolt(bodyID), ToJolt(velocity));
}

void JoltPhysicsSystem::SetBodyAngularVelocity(JoltBodyID bodyID, const JoltVec3& velocity) {
    if (mInitialized)
        mPhysicsSystem->GetBodyInterface().SetAngularVelocity(ToJolt(bodyID), ToJolt(velocity));
}

void JoltPhysicsSystem::ActivateBody(JoltBodyID bodyID) {
    if (mInitialized)
        mPhysicsSystem->GetBodyInterface().ActivateBody(ToJolt(bodyID));
}

void JoltPhysicsSystem::DeactivateBody(JoltBodyID bodyID) {
    if (mInitialized)
        mPhysicsSystem->GetBodyInterface().DeactivateBody(ToJolt(bodyID));
}

bool JoltPhysicsSystem::IsBodyActive(JoltBodyID bodyID) const {
    if (!mInitialized) return false;
    return mPhysicsSystem->GetBodyInterface().IsActive(ToJolt(bodyID));
}

void JoltPhysicsSystem::AddForce(JoltBodyID bodyID, const JoltVec3& force) {
    if (mInitialized)
        mPhysicsSystem->GetBodyInterface().AddForce(ToJolt(bodyID), ToJolt(force));
}

void JoltPhysicsSystem::AddImpulse(JoltBodyID bodyID, const JoltVec3& impulse) {
    if (mInitialized)
        mPhysicsSystem->GetBodyInterface().AddImpulse(ToJolt(bodyID), ToJolt(impulse));
}

void JoltPhysicsSystem::AddTorque(JoltBodyID bodyID, const JoltVec3& torque) {
    if (mInitialized)
        mPhysicsSystem->GetBodyInterface().AddTorque(ToJolt(bodyID), ToJolt(torque));
}

void JoltPhysicsSystem::SetBodyFriction(JoltBodyID bodyID, float friction) {
    if (mInitialized)
        mPhysicsSystem->GetBodyInterface().SetFriction(ToJolt(bodyID), friction);
}

float JoltPhysicsSystem::GetBodyFriction(JoltBodyID bodyID) const {
    if (!mInitialized) return 0;
    return mPhysicsSystem->GetBodyInterface().GetFriction(ToJolt(bodyID));
}

void JoltPhysicsSystem::SetBodyRestitution(JoltBodyID bodyID, float restitution) {
    if (mInitialized)
        mPhysicsSystem->GetBodyInterface().SetRestitution(ToJolt(bodyID), restitution);
}

float JoltPhysicsSystem::GetBodyRestitution(JoltBodyID bodyID) const {
    if (!mInitialized) return 0;
    return mPhysicsSystem->GetBodyInterface().GetRestitution(ToJolt(bodyID));
}
