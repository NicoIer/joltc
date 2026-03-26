// joltphysics_helpers.h
// Thin C++ wrapper layer that SWIG can parse easily.
// Hides SIMD types, macros, and templates behind a clean C++ API.

#pragma once
#include <cstdint>
#include <cstddef>
#include <string>

// ============================================================================
// Forward declarations - these map to Jolt internal types but are only
// exposed as opaque pointers through SWIG unless explicitly wrapped.
// ============================================================================

namespace JPH {
    class PhysicsSystem;
    class BodyInterface;
    class Body;
    class Shape;
    class BoxShape;
    class SphereShape;
    class TempAllocator;
    class JobSystem;
    class BroadPhaseLayerInterface;
    class ObjectVsBroadPhaseLayerFilter;
    class ObjectLayerPairFilter;
    class ContactListener;
    class BodyActivationListener;
    class ShapeSettings;
    class BoxShapeSettings;
    class SphereShapeSettings;
}

// ============================================================================
// Simple math structs for SWIG (no SIMD)
// ============================================================================

struct JoltVec3 {
    float x, y, z;
    JoltVec3() : x(0), y(0), z(0) {}
    JoltVec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

struct JoltQuat {
    float x, y, z, w;
    JoltQuat() : x(0), y(0), z(0), w(1) {}
    JoltQuat(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
};

// ============================================================================
// Enums (mirrored from Jolt to avoid SWIG parsing Jolt headers)
// ============================================================================

enum class JoltMotionType : uint8_t {
    Static = 0,
    Kinematic = 1,
    Dynamic = 2
};

enum class JoltActivation {
    Activate = 0,
    DontActivate = 1
};

enum class JoltPhysicsUpdateError : uint32_t {
    None = 0,
    ManifoldCacheFull = 1 << 0,
    BodyPairCacheFull = 1 << 1,
    ContactConstraintsFull = 1 << 2
};

enum class JoltValidateResult {
    AcceptAllContactsForThisBodyPair = 0,
    AcceptContact = 1,
    RejectContact = 2,
    RejectAllContactsForThisBodyPair = 3
};

// ============================================================================
// BodyID wrapper
// ============================================================================

class JoltBodyID {
public:
    JoltBodyID();
    explicit JoltBodyID(uint32_t id);
    uint32_t GetIndex() const;
    uint8_t GetSequenceNumber() const;
    uint32_t GetIndexAndSequenceNumber() const;
    bool IsInvalid() const;
    bool operator==(const JoltBodyID& other) const;
    bool operator!=(const JoltBodyID& other) const;
private:
    uint32_t mID;
};

// ============================================================================
// Contact info for callbacks (simplified)
// ============================================================================

struct JoltContactManifoldInfo {
    JoltVec3 baseOffset;
    JoltVec3 worldSpaceNormal;
    float penetrationDepth;
};

struct JoltContactSettings {
    float combinedFriction;
    float combinedRestitution;
    bool isSensor;
};

// ============================================================================
// Callback interfaces (virtual classes for SWIG Director)
// ============================================================================

class JoltBroadPhaseLayerInterface {
public:
    virtual ~JoltBroadPhaseLayerInterface() {}
    virtual unsigned int GetNumBroadPhaseLayers() const = 0;
    virtual uint8_t GetBroadPhaseLayer(uint16_t objectLayer) const = 0;
};

class JoltObjectVsBroadPhaseLayerFilter {
public:
    virtual ~JoltObjectVsBroadPhaseLayerFilter() {}
    virtual bool ShouldCollide(uint16_t objectLayer, uint8_t broadPhaseLayer) const { return true; }
};

class JoltObjectLayerPairFilter {
public:
    virtual ~JoltObjectLayerPairFilter() {}
    virtual bool ShouldCollide(uint16_t object1, uint16_t object2) const { return true; }
};

class JoltContactListener {
public:
    virtual ~JoltContactListener() {}
    virtual JoltValidateResult OnContactValidate(
        JoltBodyID body1ID, JoltBodyID body2ID,
        const JoltVec3& baseOffset) {
        return JoltValidateResult::AcceptAllContactsForThisBodyPair;
    }
    virtual void OnContactAdded(
        JoltBodyID body1ID, JoltBodyID body2ID,
        const JoltContactManifoldInfo& manifold,
        JoltContactSettings& settings) {}
    virtual void OnContactPersisted(
        JoltBodyID body1ID, JoltBodyID body2ID,
        const JoltContactManifoldInfo& manifold,
        JoltContactSettings& settings) {}
    virtual void OnContactRemoved(JoltBodyID body1ID, JoltBodyID body2ID) {}
};

class JoltBodyActivationListener {
public:
    virtual ~JoltBodyActivationListener() {}
    virtual void OnBodyActivated(JoltBodyID bodyID, uint64_t userData) = 0;
    virtual void OnBodyDeactivated(JoltBodyID bodyID, uint64_t userData) = 0;
};

// ============================================================================
// Body creation settings (plain struct, SWIG-friendly)
// ============================================================================

struct JoltBodyCreationSettings {
    JoltVec3 position;
    JoltQuat rotation;
    JoltVec3 linearVelocity;
    JoltVec3 angularVelocity;
    uint64_t userData = 0;
    uint16_t objectLayer = 0;
    JoltMotionType motionType = JoltMotionType::Dynamic;
    bool allowSleeping = true;
    float friction = 0.2f;
    float restitution = 0.0f;
    float linearDamping = 0.05f;
    float angularDamping = 0.05f;
    float gravityFactor = 1.0f;

    JoltBodyCreationSettings() :
        rotation(0, 0, 0, 1) {}
};

// ============================================================================
// Main physics wrapper class
// ============================================================================

class JoltPhysicsSystem {
public:
    JoltPhysicsSystem();
    ~JoltPhysicsSystem();

    // Initialize the system
    // broadPhaseLayerInterface, objectVsBroadPhaseFilter, objectLayerPairFilter
    // must outlive this object.
    bool Init(
        unsigned int maxBodies,
        unsigned int numBodyMutexes,
        unsigned int maxBodyPairs,
        unsigned int maxContactConstraints,
        JoltBroadPhaseLayerInterface* broadPhaseLayerInterface,
        JoltObjectVsBroadPhaseLayerFilter* objectVsBroadPhaseFilter,
        JoltObjectLayerPairFilter* objectLayerPairFilter);

    // Step the simulation
    JoltPhysicsUpdateError Update(float deltaTime, int collisionSteps);

    void OptimizeBroadPhase();

    // Gravity
    void SetGravity(const JoltVec3& gravity);
    JoltVec3 GetGravity() const;

    // Listeners
    void SetContactListener(JoltContactListener* listener);
    void SetBodyActivationListener(JoltBodyActivationListener* listener);

    // Body stats
    unsigned int GetNumBodies() const;
    unsigned int GetMaxBodies() const;

    // --- Shape creation ---
    // Returns an opaque shape handle. Caller must call ReleaseShape() when done.
    void* CreateBoxShape(const JoltVec3& halfExtent, float convexRadius = 0.05f);
    void* CreateSphereShape(float radius);
    void ReleaseShape(void* shape);

    // --- Body management ---
    JoltBodyID CreateAndAddBody(const JoltBodyCreationSettings& settings, void* shape, JoltActivation activation);
    void RemoveBody(JoltBodyID bodyID);
    void DestroyBody(JoltBodyID bodyID);
    void RemoveAndDestroyBody(JoltBodyID bodyID);

    // --- Body properties (by ID) ---
    JoltVec3 GetBodyPosition(JoltBodyID bodyID) const;
    JoltQuat GetBodyRotation(JoltBodyID bodyID) const;
    JoltVec3 GetBodyLinearVelocity(JoltBodyID bodyID) const;
    JoltVec3 GetBodyAngularVelocity(JoltBodyID bodyID) const;

    void SetBodyPosition(JoltBodyID bodyID, const JoltVec3& position, JoltActivation activation);
    void SetBodyRotation(JoltBodyID bodyID, const JoltQuat& rotation, JoltActivation activation);
    void SetBodyLinearVelocity(JoltBodyID bodyID, const JoltVec3& velocity);
    void SetBodyAngularVelocity(JoltBodyID bodyID, const JoltVec3& velocity);

    void ActivateBody(JoltBodyID bodyID);
    void DeactivateBody(JoltBodyID bodyID);
    bool IsBodyActive(JoltBodyID bodyID) const;

    void AddForce(JoltBodyID bodyID, const JoltVec3& force);
    void AddImpulse(JoltBodyID bodyID, const JoltVec3& impulse);
    void AddTorque(JoltBodyID bodyID, const JoltVec3& torque);

    void SetBodyFriction(JoltBodyID bodyID, float friction);
    float GetBodyFriction(JoltBodyID bodyID) const;
    void SetBodyRestitution(JoltBodyID bodyID, float restitution);
    float GetBodyRestitution(JoltBodyID bodyID) const;

private:
    JPH::PhysicsSystem* mPhysicsSystem = nullptr;
    JPH::TempAllocator* mTempAllocator = nullptr;
    JPH::JobSystem* mJobSystem = nullptr;

    // Bridging objects (owned, created during Init)
    class BroadPhaseLayerBridge;
    class ObjectVsBroadPhaseLayerFilterBridge;
    class ObjectLayerPairFilterBridge;
    class ContactListenerBridge;
    class BodyActivationListenerBridge;

    BroadPhaseLayerBridge* mBroadPhaseBridge = nullptr;
    ObjectVsBroadPhaseLayerFilterBridge* mObjectVsBroadPhaseBridge = nullptr;
    ObjectLayerPairFilterBridge* mObjectLayerPairBridge = nullptr;
    ContactListenerBridge* mContactListenerBridge = nullptr;
    BodyActivationListenerBridge* mBodyActivationListenerBridge = nullptr;

    bool mInitialized = false;
};

// ============================================================================
// Global init / shutdown
// ============================================================================

/// Must be called before creating any JoltPhysicsSystem
void JoltPhysics_Init();

/// Must be called after all JoltPhysicsSystem objects are destroyed
void JoltPhysics_Shutdown();
