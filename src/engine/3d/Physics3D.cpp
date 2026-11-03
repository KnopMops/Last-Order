#include "engine/3d/Physics3D.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/ShapeFilter.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include "raymath.h"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <unordered_map>

namespace engine::physics3d {

namespace {

namespace Layers {
constexpr JPH::ObjectLayer NON_MOVING = 0;
constexpr JPH::ObjectLayer MOVING = 1;
constexpr JPH::ObjectLayer DOOR = 2;
constexpr JPH::ObjectLayer CHARACTER = 3;
constexpr JPH::ObjectLayer COUNT = 4;
}

namespace BroadLayers {
constexpr JPH::BroadPhaseLayer NON_MOVING(0);
constexpr JPH::BroadPhaseLayer MOVING(1);
constexpr unsigned int COUNT = 2;
}

class BroadPhaseLayers final : public JPH::BroadPhaseLayerInterface {
public:
    unsigned int GetNumBroadPhaseLayers() const override { return BroadLayers::COUNT; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        return layer == Layers::NON_MOVING ? BroadLayers::NON_MOVING : BroadLayers::MOVING;
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
        return layer == BroadLayers::NON_MOVING ? "NON_MOVING" : "MOVING";
    }
#endif
};

class ObjectVsBroadPhase final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broad) const override {
        if (layer == Layers::NON_MOVING) return broad == BroadLayers::MOVING;
        if (layer == Layers::MOVING) return true;
        return broad == BroadLayers::MOVING;
    }
};

class ObjectPairs final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
        if (a == Layers::MOVING || b == Layers::MOVING) return true;
        if (a == Layers::NON_MOVING) return b == Layers::CHARACTER;
        if (b == Layers::NON_MOVING) return a == Layers::CHARACTER;
        if (a == Layers::DOOR) return b == Layers::CHARACTER;
        if (b == Layers::DOOR) return a == Layers::CHARACTER;
        return false;
    }
};

class StaticOnlyBroadPhase final : public JPH::BroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::BroadPhaseLayer layer) const override { return layer == BroadLayers::NON_MOVING; }
};

class StaticOnlyObjects final : public JPH::ObjectLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer layer) const override { return layer == Layers::NON_MOVING; }
};

class QueryObjects final : public JPH::ObjectLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer layer) const override { return layer != Layers::CHARACTER; }
};

class IgnoreTwoBodies final : public JPH::BodyFilter {
public:
    IgnoreTwoBodies(JPH::BodyID a, JPH::BodyID b) : m_a(a), m_b(b) {}
    bool ShouldCollide(const JPH::BodyID& id) const override { return id != m_a && id != m_b; }
    bool ShouldCollideLocked(const JPH::Body& body) const override { return ShouldCollide(body.GetID()); }

private:
    JPH::BodyID m_a;
    JPH::BodyID m_b;
};

void TraceImpl(const char* fmt, ...) {
    va_list list;
    va_start(list, fmt);
    char buffer[1024];
    std::vsnprintf(buffer, sizeof(buffer), fmt, list);
    va_end(list);
    TraceLog(LOG_DEBUG, "JOLT: %s", buffer);
}

#ifdef JPH_ENABLE_ASSERTS
bool AssertFailedImpl(const char* expression, const char* message, const char* file, JPH::uint line) {
    TraceLog(LOG_WARNING, "JOLT ASSERT %s:%u: (%s) %s", file, line, expression, message != nullptr ? message : "");
    return false;
}
#endif

void EnsureJoltRegistered() {
    static bool registered = false;
    if (registered) return;
    JPH::RegisterDefaultAllocator();
    JPH::Trace = TraceImpl;
#ifdef JPH_ENABLE_ASSERTS
    JPH::AssertFailed = AssertFailedImpl;
#endif
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
    registered = true;
}

JPH::Vec3 ToJ(Vector3 v) { return JPH::Vec3(v.x, v.y, v.z); }
Vector3 FromJ(JPH::Vec3Arg v) { return Vector3{v.GetX(), v.GetY(), v.GetZ()}; }
JPH::BodyID ToId(BodyHandle h) { return JPH::BodyID(h); }
BodyHandle FromId(JPH::BodyID id) { return id.IsInvalid() ? INVALID_BODY : id.GetIndexAndSequenceNumber(); }

JPH::RefConst<JPH::Shape> MakeCapsule(float radius, float totalHeight) {
    float halfCylinder = std::max(0.05f, (totalHeight - 2.0f * radius) * 0.5f);
    JPH::RotatedTranslatedShapeSettings settings(JPH::Vec3(0.0f, halfCylinder + radius, 0.0f), JPH::Quat::sIdentity(),
                                                 new JPH::CapsuleShape(halfCylinder, radius));
    return settings.Create().Get();
}

JPH::RefConst<JPH::Shape> MakeHull(const std::vector<Vector3>& points, Vector3 origin) {
    JPH::Array<JPH::Vec3> local;
    local.reserve(points.size());
    for (const Vector3& p : points) local.push_back(JPH::Vec3(p.x - origin.x, p.y - origin.y, p.z - origin.z));
    JPH::ConvexHullShapeSettings settings(local, 0.01f);
    auto result = settings.Create();
    if (result.HasError()) return nullptr;
    return result.Get();
}

Vector3 Centroid(const std::vector<Vector3>& points) {
    Vector3 c{0.0f, 0.0f, 0.0f};
    for (const Vector3& p : points) {
        c.x += p.x;
        c.y += p.y;
        c.z += p.z;
    }
    float inv = points.empty() ? 0.0f : 1.0f / static_cast<float>(points.size());
    return Vector3{c.x * inv, c.y * inv, c.z * inv};
}

}

struct Physics3D::Impl {
    BroadPhaseLayers broadLayers;
    ObjectVsBroadPhase objectVsBroad;
    ObjectPairs objectPairs;
    std::unique_ptr<JPH::TempAllocatorImpl> tempAllocator;
    std::unique_ptr<JPH::JobSystemSingleThreaded> jobSystem;
    std::unique_ptr<JPH::PhysicsSystem> system;
    std::unordered_map<BodyHandle, JPH::Ref<JPH::HingeConstraint>> doors;
    std::vector<JPH::BodyID> bodies;

    JPH::Ref<JPH::CharacterVirtual> character;
    JPH::RefConst<JPH::Shape> standShape;
    JPH::RefConst<JPH::Shape> crouchShape;
    CharacterConfig characterConfig;
    bool crouched = false;

    JPH::BodyInterface& Bodies() { return system->GetBodyInterface(); }
    const JPH::BodyInterface& Bodies() const { return system->GetBodyInterface(); }
};

Physics3D::Physics3D() = default;

Physics3D::~Physics3D() {
    Shutdown();
}

bool Physics3D::Init() {
    Shutdown();
    EnsureJoltRegistered();
    m_impl = std::make_unique<Impl>();
    m_impl->tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(16 * 1024 * 1024);
    m_impl->jobSystem = std::make_unique<JPH::JobSystemSingleThreaded>(JPH::cMaxPhysicsJobs);
    m_impl->system = std::make_unique<JPH::PhysicsSystem>();
    m_impl->system->Init(4096, 0, 8192, 4096, m_impl->broadLayers, m_impl->objectVsBroad, m_impl->objectPairs);
    m_impl->system->SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));
    return true;
}

void Physics3D::Shutdown() {
    if (!m_impl) return;
    m_impl->character = nullptr;
    for (auto& [handle, constraint] : m_impl->doors) {
        m_impl->system->RemoveConstraint(constraint);
    }
    m_impl->doors.clear();
    auto& bi = m_impl->Bodies();
    for (JPH::BodyID id : m_impl->bodies) {
        if (bi.IsAdded(id)) bi.RemoveBody(id);
        bi.DestroyBody(id);
    }
    m_impl->bodies.clear();
    m_impl->system.reset();
    m_impl->jobSystem.reset();
    m_impl->tempAllocator.reset();
    m_impl.reset();
}

bool Physics3D::IsReady() const {
    return m_impl != nullptr;
}

BodyHandle Physics3D::AddStaticHull(const std::vector<Vector3>& worldPoints) {
    if (!m_impl || worldPoints.size() < 4) return INVALID_BODY;
    Vector3 origin = Centroid(worldPoints);
    auto shape = MakeHull(worldPoints, origin);
    if (!shape) return INVALID_BODY;
    JPH::BodyCreationSettings settings(shape, JPH::RVec3(origin.x, origin.y, origin.z), JPH::Quat::sIdentity(),
                                       JPH::EMotionType::Static, Layers::NON_MOVING);
    settings.mFriction = 0.7f;
    JPH::BodyID id = m_impl->Bodies().CreateAndAddBody(settings, JPH::EActivation::DontActivate);
    if (id.IsInvalid()) return INVALID_BODY;
    m_impl->bodies.push_back(id);
    return FromId(id);
}

BodyHandle Physics3D::AddDynamicHull(const std::vector<Vector3>& worldPoints, float mass, Vector3& outOrigin) {
    if (!m_impl || worldPoints.size() < 4) return INVALID_BODY;
    outOrigin = Centroid(worldPoints);
    auto shape = MakeHull(worldPoints, outOrigin);
    if (!shape) return INVALID_BODY;
    JPH::BodyCreationSettings settings(shape, JPH::RVec3(outOrigin.x, outOrigin.y, outOrigin.z), JPH::Quat::sIdentity(),
                                       JPH::EMotionType::Dynamic, Layers::MOVING);
    settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    settings.mMassPropertiesOverride.mMass = std::max(0.5f, mass);
    settings.mFriction = 0.6f;
    settings.mRestitution = 0.1f;
    settings.mLinearDamping = 0.2f;
    settings.mAngularDamping = 0.4f;
    JPH::BodyID id = m_impl->Bodies().CreateAndAddBody(settings, JPH::EActivation::DontActivate);
    if (id.IsInvalid()) return INVALID_BODY;
    m_impl->bodies.push_back(id);
    return FromId(id);
}

BodyHandle Physics3D::AddDoor(const std::vector<Vector3>& worldPoints, Vector3 hingePoint, float mass,
                              float minAngle, float maxAngle, Vector3& outOrigin) {
    if (!m_impl || worldPoints.size() < 4) return INVALID_BODY;
    outOrigin = Centroid(worldPoints);
    auto shape = MakeHull(worldPoints, outOrigin);
    if (!shape) return INVALID_BODY;
    JPH::BodyCreationSettings settings(shape, JPH::RVec3(outOrigin.x, outOrigin.y, outOrigin.z), JPH::Quat::sIdentity(),
                                       JPH::EMotionType::Dynamic, Layers::DOOR);
    settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    settings.mMassPropertiesOverride.mMass = std::max(1.0f, mass);
    settings.mFriction = 0.4f;
    settings.mAngularDamping = 1.5f;
    settings.mLinearDamping = 0.5f;
    settings.mGravityFactor = 0.0f;
    JPH::Body* body = m_impl->Bodies().CreateBody(settings);
    if (body == nullptr) return INVALID_BODY;
    m_impl->Bodies().AddBody(body->GetID(), JPH::EActivation::DontActivate);
    m_impl->bodies.push_back(body->GetID());

    JPH::HingeConstraintSettings hinge;
    hinge.mSpace = JPH::EConstraintSpace::WorldSpace;
    hinge.mPoint1 = hinge.mPoint2 = JPH::RVec3(hingePoint.x, hingePoint.y, hingePoint.z);
    hinge.mHingeAxis1 = hinge.mHingeAxis2 = JPH::Vec3::sAxisY();
    hinge.mNormalAxis1 = hinge.mNormalAxis2 = JPH::Vec3::sAxisX();
    hinge.mLimitsMin = std::min(0.0f, minAngle);
    hinge.mLimitsMax = std::max(0.0f, maxAngle);
    hinge.mMaxFrictionTorque = 6.0f;
    hinge.mMotorSettings = JPH::MotorSettings(1.6f, 1.0f, 0.0f, 140.0f);
    auto* constraint = static_cast<JPH::HingeConstraint*>(hinge.Create(JPH::Body::sFixedToWorld, *body));
    m_impl->system->AddConstraint(constraint);
    constraint->SetMotorState(JPH::EMotorState::Position);
    constraint->SetTargetAngle(0.0f);
    BodyHandle handle = FromId(body->GetID());
    m_impl->doors[handle] = constraint;
    return handle;
}

void Physics3D::FinalizeStatic() {
    if (m_impl) m_impl->system->OptimizeBroadPhase();
}

void Physics3D::SetDoorLimits(BodyHandle door, float minAngle, float maxAngle) {
    if (!m_impl) return;
    auto it = m_impl->doors.find(door);
    if (it == m_impl->doors.end()) return;
    it->second->SetLimits(std::min(0.0f, minAngle), std::max(0.0f, maxAngle));
    WakeBody(door);
}

void Physics3D::SetDoorTarget(BodyHandle door, float angle) {
    if (!m_impl) return;
    auto it = m_impl->doors.find(door);
    if (it == m_impl->doors.end()) return;
    it->second->SetTargetAngle(angle);
    WakeBody(door);
}

float Physics3D::GetDoorAngle(BodyHandle door) const {
    if (!m_impl) return 0.0f;
    auto it = m_impl->doors.find(door);
    if (it == m_impl->doors.end()) return 0.0f;
    return it->second->GetCurrentAngle();
}

void Physics3D::Step(float dt) {
    if (!m_impl) return;
    m_impl->system->Update(dt, 1, m_impl->tempAllocator.get(), m_impl->jobSystem.get());
}

Matrix Physics3D::GetBodyTransform(BodyHandle body) const {
    if (!m_impl || body == INVALID_BODY) return MatrixIdentity();
    JPH::RMat44 m = m_impl->Bodies().GetWorldTransform(ToId(body));
    JPH::Vec3 c0 = m.GetColumn3(0);
    JPH::Vec3 c1 = m.GetColumn3(1);
    JPH::Vec3 c2 = m.GetColumn3(2);
    JPH::RVec3 t = m.GetTranslation();
    Matrix out{};
    out.m0 = c0.GetX(); out.m1 = c0.GetY(); out.m2 = c0.GetZ(); out.m3 = 0.0f;
    out.m4 = c1.GetX(); out.m5 = c1.GetY(); out.m6 = c1.GetZ(); out.m7 = 0.0f;
    out.m8 = c2.GetX(); out.m9 = c2.GetY(); out.m10 = c2.GetZ(); out.m11 = 0.0f;
    out.m12 = static_cast<float>(t.GetX()); out.m13 = static_cast<float>(t.GetY()); out.m14 = static_cast<float>(t.GetZ()); out.m15 = 1.0f;
    return out;
}

Vector3 Physics3D::GetBodyPosition(BodyHandle body) const {
    if (!m_impl || body == INVALID_BODY) return Vector3{0.0f, 0.0f, 0.0f};
    JPH::RVec3 p = m_impl->Bodies().GetCenterOfMassPosition(ToId(body));
    return Vector3{static_cast<float>(p.GetX()), static_cast<float>(p.GetY()), static_cast<float>(p.GetZ())};
}

Vector3 Physics3D::GetBodyVelocity(BodyHandle body) const {
    if (!m_impl || body == INVALID_BODY) return Vector3{0.0f, 0.0f, 0.0f};
    return FromJ(m_impl->Bodies().GetLinearVelocity(ToId(body)));
}

Vector3 Physics3D::GetBodyAngularVelocity(BodyHandle body) const {
    if (!m_impl || body == INVALID_BODY) return Vector3{0.0f, 0.0f, 0.0f};
    return FromJ(m_impl->Bodies().GetAngularVelocity(ToId(body)));
}

void Physics3D::SetBodyVelocity(BodyHandle body, Vector3 linear, Vector3 angular) {
    if (!m_impl || body == INVALID_BODY) return;
    m_impl->Bodies().SetLinearAndAngularVelocity(ToId(body), ToJ(linear), ToJ(angular));
    m_impl->Bodies().ActivateBody(ToId(body));
}

void Physics3D::AddImpulse(BodyHandle body, Vector3 impulse) {
    if (!m_impl || body == INVALID_BODY) return;
    m_impl->Bodies().AddImpulse(ToId(body), ToJ(impulse));
}

void Physics3D::WakeBody(BodyHandle body) {
    if (!m_impl || body == INVALID_BODY) return;
    m_impl->Bodies().ActivateBody(ToId(body));
}

RayHit Physics3D::CastRay(Vector3 origin, Vector3 direction, float maxDistance, BodyHandle ignore) const {
    RayHit out;
    if (!m_impl) return out;
    JPH::RRayCast ray(JPH::RVec3(origin.x, origin.y, origin.z), ToJ(direction) * maxDistance);
    JPH::RayCastResult result;
    JPH::BodyID innerId = m_impl->character ? m_impl->character->GetInnerBodyID() : JPH::BodyID();
    IgnoreTwoBodies bodyFilter(innerId, ignore == INVALID_BODY ? JPH::BodyID() : ToId(ignore));
    QueryObjects objectFilter;
    JPH::BroadPhaseLayerFilter broadFilter;
    if (!m_impl->system->GetNarrowPhaseQuery().CastRay(ray, result, broadFilter, objectFilter, bodyFilter)) return out;
    out.hit = true;
    out.distance = result.mFraction * maxDistance;
    JPH::RVec3 point = ray.GetPointOnRay(result.mFraction);
    out.point = Vector3{static_cast<float>(point.GetX()), static_cast<float>(point.GetY()), static_cast<float>(point.GetZ())};
    out.body = FromId(result.mBodyID);
    JPH::BodyLockRead lock(m_impl->system->GetBodyLockInterface(), result.mBodyID);
    if (lock.Succeeded()) {
        out.normal = FromJ(lock.GetBody().GetWorldSpaceSurfaceNormal(result.mSubShapeID2, point));
    }
    return out;
}

bool Physics3D::IsStaticOccluded(Vector3 from, Vector3 to) const {
    if (!m_impl) return false;
    JPH::Vec3 delta(to.x - from.x, to.y - from.y, to.z - from.z);
    JPH::RRayCast ray(JPH::RVec3(from.x, from.y, from.z), delta);
    JPH::RayCastResult result;
    result.mFraction = 0.999f;
    StaticOnlyBroadPhase broadFilter;
    StaticOnlyObjects objectFilter;
    return m_impl->system->GetNarrowPhaseQuery().CastRay(ray, result, broadFilter, objectFilter);
}

float Physics3D::CastStaticDistance(Vector3 origin, Vector3 direction, float maxDistance) const {
    if (!m_impl) return maxDistance;
    JPH::RRayCast ray(JPH::RVec3(origin.x, origin.y, origin.z), ToJ(direction) * maxDistance);
    JPH::RayCastResult result;
    StaticOnlyBroadPhase broadFilter;
    StaticOnlyObjects objectFilter;
    if (!m_impl->system->GetNarrowPhaseQuery().CastRay(ray, result, broadFilter, objectFilter)) return maxDistance;
    return result.mFraction * maxDistance;
}

void Physics3D::CreateCharacter(Vector3 feetPosition, const CharacterConfig& config) {
    if (!m_impl) return;
    m_impl->characterConfig = config;
    m_impl->standShape = MakeCapsule(config.radius, config.standHeight);
    m_impl->crouchShape = MakeCapsule(config.radius, config.crouchHeight);
    m_impl->crouched = false;

    JPH::Ref<JPH::CharacterVirtualSettings> settings = new JPH::CharacterVirtualSettings();
    settings->mShape = m_impl->standShape;
    settings->mMaxSlopeAngle = JPH::DegreesToRadians(50.0f);
    settings->mMass = config.mass;
    settings->mMaxStrength = config.pushStrength;
    settings->mCharacterPadding = 0.02f;
    settings->mPenetrationRecoverySpeed = 1.0f;
    settings->mPredictiveContactDistance = 0.1f;
    settings->mBackFaceMode = JPH::EBackFaceMode::CollideWithBackFaces;
    settings->mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -config.radius);
    settings->mInnerBodyShape = m_impl->standShape;
    settings->mInnerBodyLayer = Layers::CHARACTER;
    settings->mEnhancedInternalEdgeRemoval = true;
    m_impl->character = new JPH::CharacterVirtual(settings, JPH::RVec3(feetPosition.x, feetPosition.y, feetPosition.z),
                                                  JPH::Quat::sIdentity(), 0, m_impl->system.get());
}

void Physics3D::UpdateCharacter(float dt, Vector3 desiredHorizontalVelocity, bool wantCrouch, BodyHandle ignoreBody) {
    if (!m_impl || !m_impl->character || dt <= 0.0f) return;
    auto& ch = *m_impl->character;
    auto& sys = *m_impl->system;
    auto broadFilter = sys.GetDefaultBroadPhaseLayerFilter(Layers::MOVING);
    QueryObjects objectFilter;
    IgnoreTwoBodies bodyFilter(ch.GetInnerBodyID(), ignoreBody == INVALID_BODY ? JPH::BodyID() : ToId(ignoreBody));
    JPH::ShapeFilter shapeFilter;

    if (wantCrouch != m_impl->crouched) {
        const JPH::Shape* target = wantCrouch ? m_impl->crouchShape.GetPtr() : m_impl->standShape.GetPtr();
        float maxPenetration = 1.5f * sys.GetPhysicsSettings().mPenetrationSlop;
        if (ch.SetShape(target, maxPenetration, broadFilter, objectFilter, bodyFilter, shapeFilter, *m_impl->tempAllocator)) {
            ch.SetInnerBodyShape(target);
            m_impl->crouched = wantCrouch;
        }
    }

    ch.UpdateGroundVelocity();
    JPH::Vec3 up = JPH::Vec3::sAxisY();
    JPH::Vec3 currentVertical = ch.GetLinearVelocity().Dot(up) * up;
    JPH::Vec3 groundVelocity = ch.GetGroundVelocity();
    bool movingTowardGround = (currentVertical.GetY() - groundVelocity.GetY()) < 0.1f;
    JPH::Vec3 velocity;
    if (ch.GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround && movingTowardGround) {
        velocity = groundVelocity;
    } else {
        velocity = currentVertical;
    }
    velocity += sys.GetGravity() * dt;
    velocity += JPH::Vec3(desiredHorizontalVelocity.x, 0.0f, desiredHorizontalVelocity.z);
    ch.SetLinearVelocity(velocity);

    JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
    updateSettings.mWalkStairsStepUp = JPH::Vec3(0.0f, 0.32f, 0.0f);
    updateSettings.mStickToFloorStepDown = JPH::Vec3(0.0f, -0.35f, 0.0f);
    ch.ExtendedUpdate(dt, sys.GetGravity(), updateSettings, broadFilter, objectFilter, bodyFilter, shapeFilter, *m_impl->tempAllocator);
}

void Physics3D::TeleportCharacter(Vector3 feetPosition) {
    if (!m_impl || !m_impl->character) return;
    m_impl->character->SetPosition(JPH::RVec3(feetPosition.x, feetPosition.y, feetPosition.z));
    m_impl->character->SetLinearVelocity(JPH::Vec3::sZero());
}

Vector3 Physics3D::GetCharacterPosition() const {
    if (!m_impl || !m_impl->character) return Vector3{0.0f, 0.0f, 0.0f};
    JPH::RVec3 p = m_impl->character->GetPosition();
    return Vector3{static_cast<float>(p.GetX()), static_cast<float>(p.GetY()), static_cast<float>(p.GetZ())};
}

Vector3 Physics3D::GetCharacterVelocity() const {
    if (!m_impl || !m_impl->character) return Vector3{0.0f, 0.0f, 0.0f};
    return FromJ(m_impl->character->GetLinearVelocity());
}

bool Physics3D::IsCharacterOnGround() const {
    return m_impl && m_impl->character && m_impl->character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround;
}

bool Physics3D::IsCharacterCrouched() const {
    return m_impl && m_impl->crouched;
}

BodyHandle Physics3D::GetCharacterGroundBody() const {
    if (!m_impl || !m_impl->character) return INVALID_BODY;
    return FromId(m_impl->character->GetGroundBodyID());
}

BodyHandle Physics3D::GetCharacterInnerBody() const {
    if (!m_impl || !m_impl->character) return INVALID_BODY;
    return FromId(m_impl->character->GetInnerBodyID());
}

}
