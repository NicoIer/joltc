using System;
using System.Runtime.InteropServices;
using JoltPhysics;

// ============================================================================
// Layer definitions
// ============================================================================

class BroadPhaseLayers : JoltBroadPhaseLayerInterface
{
    public const byte NON_MOVING = 0;
    public const byte MOVING = 1;
    public const uint NUM_LAYERS = 2;

    public override uint GetNumBroadPhaseLayers()
    {
        return NUM_LAYERS;
    }

    public override byte GetBroadPhaseLayer(ushort objectLayer)
    {
        // Object layers 0 = non-moving -> broad phase 0
        // Object layers 1 = moving -> broad phase 1
        return objectLayer == 0 ? NON_MOVING : MOVING;
    }
}

class ObjectVsBroadPhaseFilter : JoltObjectVsBroadPhaseLayerFilter
{
    public override bool ShouldCollide(ushort objectLayer, byte broadPhaseLayer)
    {
        // Non-moving objects only collide with moving broad phase
        if (objectLayer == 0) // NON_MOVING
            return broadPhaseLayer == BroadPhaseLayers.MOVING;
        // Moving objects collide with everything
        return true;
    }
}

class ObjectLayerPairFilterImpl : JoltObjectLayerPairFilter
{
    public override bool ShouldCollide(ushort object1, ushort object2)
    {
        // Non-moving vs non-moving: no collision
        if (object1 == 0 && object2 == 0) return false;
        return true;
    }
}

// ============================================================================
// Contact listener - prints when contacts happen
// ============================================================================

class MyContactListener : JoltContactListener
{
    public override JoltValidateResult OnContactValidate(
        JoltBodyID body1ID, JoltBodyID body2ID, JoltVec3 baseOffset)
    {
        Console.WriteLine($"  [Contact] Validate: body {body1ID.GetIndex()} vs {body2ID.GetIndex()}");
        return JoltValidateResult.AcceptAllContactsForThisBodyPair;
    }

    public override void OnContactAdded(
        JoltBodyID body1ID, JoltBodyID body2ID,
        JoltContactManifoldInfo manifold, JoltContactSettings settings)
    {
        Console.WriteLine($"  [Contact] Added: body {body1ID.GetIndex()} vs {body2ID.GetIndex()}" +
                          $" (penetration={manifold.penetrationDepth:F4})");
    }

    public override void OnContactPersisted(
        JoltBodyID body1ID, JoltBodyID body2ID,
        JoltContactManifoldInfo manifold, JoltContactSettings settings)
    {
        // Don't print every frame to avoid spam
    }

    public override void OnContactRemoved(JoltBodyID body1ID, JoltBodyID body2ID)
    {
        Console.WriteLine($"  [Contact] Removed: body {body1ID.GetIndex()} vs {body2ID.GetIndex()}");
    }
}

// ============================================================================
// Body activation listener
// ============================================================================

class MyBodyActivationListener : JoltBodyActivationListener
{
    public override void OnBodyActivated(JoltBodyID bodyID, ulong userData)
    {
        Console.WriteLine($"  [Activation] Body {bodyID.GetIndex()} activated");
    }

    public override void OnBodyDeactivated(JoltBodyID bodyID, ulong userData)
    {
        Console.WriteLine($"  [Activation] Body {bodyID.GetIndex()} deactivated");
    }
}

// ============================================================================
// Main
// ============================================================================

class Program
{
    const ushort LAYER_NON_MOVING = 0;
    const ushort LAYER_MOVING = 1;

    static void Main(string[] args)
    {
        Console.WriteLine("=== JoltPhysics SWIG C# POC ===");
        Console.WriteLine();

        // Initialize Jolt
        JoltPhysicsModule.JoltPhysics_Init();
        Console.WriteLine("JoltPhysics initialized.");

        // Create physics system
        var physics = new JoltPhysicsSystem();

        // Create layer interfaces
        var broadPhaseLayer = new BroadPhaseLayers();
        var objectVsBroadPhase = new ObjectVsBroadPhaseFilter();
        var objectLayerPair = new ObjectLayerPairFilterImpl();

        bool ok = physics.Init(
            1024,   // maxBodies
            0,      // numBodyMutexes (0 = auto)
            1024,   // maxBodyPairs
            1024,   // maxContactConstraints
            broadPhaseLayer,
            objectVsBroadPhase,
            objectLayerPair);

        if (!ok)
        {
            Console.WriteLine("ERROR: Failed to initialize physics system!");
            return;
        }
        Console.WriteLine("Physics system initialized.");

        // Set gravity
        physics.SetGravity(new JoltVec3(0, -9.81f, 0));

        // Set up listeners
        var contactListener = new MyContactListener();
        var activationListener = new MyBodyActivationListener();
        physics.SetContactListener(contactListener);
        physics.SetBodyActivationListener(activationListener);

        // --- Create ground (static box) ---
        IntPtr groundShape = physics.CreateBoxShape(new JoltVec3(100, 1, 100), 0.0f);

        var groundSettings = new JoltBodyCreationSettings();
        groundSettings.position = new JoltVec3(0, -1, 0);
        groundSettings.rotation = new JoltQuat(0, 0, 0, 1);
        groundSettings.motionType = JoltMotionType.Static;
        groundSettings.objectLayer = LAYER_NON_MOVING;

        JoltBodyID groundID = physics.CreateAndAddBody(groundSettings, groundShape, JoltActivation.DontActivate);
        Console.WriteLine($"Ground body created: index={groundID.GetIndex()}");

        // --- Create sphere (dynamic) ---
        IntPtr sphereShape = physics.CreateSphereShape(0.5f);

        var sphereSettings = new JoltBodyCreationSettings();
        sphereSettings.position = new JoltVec3(0, 10, 0);
        sphereSettings.rotation = new JoltQuat(0, 0, 0, 1);
        sphereSettings.motionType = JoltMotionType.Dynamic;
        sphereSettings.objectLayer = LAYER_MOVING;
        sphereSettings.restitution = 0.5f;

        JoltBodyID sphereID = physics.CreateAndAddBody(sphereSettings, sphereShape, JoltActivation.Activate);
        Console.WriteLine($"Sphere body created: index={sphereID.GetIndex()}");

        // Optimize broad phase after adding all bodies
        physics.OptimizeBroadPhase();

        // --- Simulation loop ---
        Console.WriteLine();
        Console.WriteLine("Starting simulation (120 steps at 1/60s)...");
        Console.WriteLine();

        float deltaTime = 1.0f / 60.0f;
        int numSteps = 120; // 2 seconds

        for (int step = 0; step < numSteps; step++)
        {
            JoltPhysicsUpdateError err = physics.Update(deltaTime, 1);
            if (err != JoltPhysicsUpdateError.None)
            {
                Console.WriteLine($"Physics update error: {err}");
            }

            // Print sphere position every 10 steps
            if (step % 10 == 0)
            {
                JoltVec3 pos = physics.GetBodyPosition(sphereID);
                JoltVec3 vel = physics.GetBodyLinearVelocity(sphereID);
                Console.WriteLine($"Step {step,3}: pos=({pos.x,8:F3}, {pos.y,8:F3}, {pos.z,8:F3})" +
                                  $"  vel=({vel.x,7:F3}, {vel.y,7:F3}, {vel.z,7:F3})");
            }
        }

        Console.WriteLine();
        Console.WriteLine("Simulation complete.");

        // Cleanup
        physics.RemoveAndDestroyBody(sphereID);
        physics.RemoveAndDestroyBody(groundID);
        physics.ReleaseShape(sphereShape);
        physics.ReleaseShape(groundShape);

        // Dispose (prevent GC before Jolt is done)
        physics.Dispose();

        JoltPhysicsModule.JoltPhysics_Shutdown();
        Console.WriteLine("JoltPhysics shut down.");
    }
}
