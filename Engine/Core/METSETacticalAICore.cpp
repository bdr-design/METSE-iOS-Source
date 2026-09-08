#include "METSETacticalAICore.hpp"
#include "METSEWorldCollision.hpp"
#include <algorithm>
#include <cmath>

namespace metse {
namespace {
constexpr double kPi = 3.14159265358979323846;
}

TacticalAICore::TacticalAICore(TacticalAIConfig config) noexcept : config_(config) {
    if (!std::isfinite(config_.maxVisionDistanceMeters) || config_.maxVisionDistanceMeters <= 1.0) config_.maxVisionDistanceMeters = 72.0;
    if (!std::isfinite(config_.horizontalFovRadians) || config_.horizontalFovRadians <= 0.1 || config_.horizontalFovRadians >= 2.0*kPi) config_.horizontalFovRadians = 1.91986217719;
    if (!std::isfinite(config_.memorySeconds) || config_.memorySeconds <= 0.1) config_.memorySeconds = 8.0;
    if (!std::isfinite(config_.hearingBaseMeters) || config_.hearingBaseMeters < 0.0) config_.hearingBaseMeters = 6.0;
    if (!std::isfinite(config_.hearingMaxMeters) || config_.hearingMaxMeters < config_.hearingBaseMeters) config_.hearingMaxMeters = 34.0;
    if (!std::isfinite(config_.hearingMaxLocalizationErrorMeters) || config_.hearingMaxLocalizationErrorMeters < 0.0) config_.hearingMaxLocalizationErrorMeters = 7.0;
    if (!std::isfinite(config_.agentEyeHeight) || config_.agentEyeHeight <= 0.2) config_.agentEyeHeight = 1.58;
    config_.suspiciousConfidence = std::clamp(config_.suspiciousConfidence,0.05,0.8);
    config_.engagedConfidence = std::clamp(config_.engagedConfidence,config_.suspiciousConfidence+0.05,1.0);
    reset();
}

void TacticalAICore::reset() noexcept {
    agents_ = {};
    agentCount_ = 0;
}

bool TacticalAICore::syncAgent(std::size_t index, std::uint32_t id, Vec3 position, double facingYaw, bool alive) noexcept {
    if (index >= kMaxAgents || id == 0 || !std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z) || !std::isfinite(facingYaw)) return false;
    auto& agent = agents_[index];
    const bool sameIdentity = agent.id == id && agent.id != 0;
    if (!sameIdentity) {
        agent = {};
        agent.id = id;
    }
    agent.position = position;
    agent.facingYaw = wrapAngle(facingYaw);
    agent.alive = alive;
    if (!alive) {
        agent.hasLineOfSight = false;
        agent.heardPlayer = false;
        agent.confidence = 0.0;
        agent.threat = 0.0;
        agent.alert = AIAlertState::Unaware;
        agent.perceptionSource = AIPerceptionSource::None;
        agent.memoryAgeSeconds = 0.0;
    }
    agentCount_ = std::max(agentCount_, index + 1);
    return true;
}

void TacticalAICore::fixedStep(double dt,
                               const WorldCollisionCore& world,
                               Vec3 playerPosition,
                               Vec3 playerVelocity,
                               double playerNoise01) noexcept {
    if (!std::isfinite(dt) || dt <= 0.0 ||
        !std::isfinite(playerPosition.x) || !std::isfinite(playerPosition.y) || !std::isfinite(playerPosition.z) ||
        !std::isfinite(playerVelocity.x) || !std::isfinite(playerVelocity.y) || !std::isfinite(playerVelocity.z)) return;
    const double noise = clamp01(playerNoise01);
    const double hearingRadius = config_.hearingBaseMeters + (config_.hearingMaxMeters-config_.hearingBaseMeters)*noise;
    const double playerSpeed = std::sqrt(playerVelocity.x*playerVelocity.x + playerVelocity.z*playerVelocity.z);

    for (std::size_t i=0;i<agentCount_;++i) {
        auto& agent = agents_[i];
        if (agent.id==0 || !agent.alive) continue;

        const double dx = playerPosition.x-agent.position.x;
        const double dz = playerPosition.z-agent.position.z;
        const double distanceXZ = std::hypot(dx,dz);
        const double targetYaw = std::atan2(dx,dz);
        const double angularError = std::abs(wrapAngle(targetYaw-agent.facingYaw));
        const bool insideFov = angularError <= config_.horizontalFovRadians*0.5;
        const bool insideVisionDistance = distanceXZ <= config_.maxVisionDistanceMeters;

        Vec3 eye{agent.position.x,agent.position.y+config_.agentEyeHeight,agent.position.z};
        Vec3 playerChest{playerPosition.x,playerPosition.y+1.15,playerPosition.z};
        const auto worldHit = world.raycastSegment(eye,playerChest);
        const bool clearLine = !worldHit.hit;
        agent.hasLineOfSight = insideFov && insideVisionDistance && clearLine;
        agent.heardPlayer = noise > 0.001 && distanceXZ <= hearingRadius;

        if (agent.hasLineOfSight) {
            const double distanceFactor = 1.0-clamp01(distanceXZ/config_.maxVisionDistanceMeters);
            const double angleFactor = 1.0-clamp01(angularError/(config_.horizontalFovRadians*0.5));
            const double visualConfidence = std::clamp(0.58 + 0.30*distanceFactor + 0.12*angleFactor,0.0,1.0);
            agent.confidence = std::max(agent.confidence,visualConfidence);
            agent.lastKnownPlayerPosition = playerPosition;
            agent.perceptionSource = AIPerceptionSource::Vision;
            agent.memoryAgeSeconds = 0.0;
        } else if (agent.heardPlayer) {
            const double hearingFactor = hearingRadius > 1e-6 ? 1.0-clamp01(distanceXZ/hearingRadius) : 0.0;
            const double audioConfidence = std::clamp(0.18 + 0.42*noise + 0.20*hearingFactor,0.0,0.78);
            agent.confidence = std::max(agent.confidence,audioConfidence);
            // Hearing creates a deterministic search estimate, never exact omniscient knowledge.
            agent.lastKnownPlayerPosition = hearingEstimate(agent.id,playerPosition,audioConfidence,config_.hearingMaxLocalizationErrorMeters);
            agent.perceptionSource = AIPerceptionSource::Hearing;
            agent.memoryAgeSeconds = 0.0;
        } else if (agent.confidence > 0.0) {
            agent.memoryAgeSeconds += dt;
            const double decay = dt/config_.memorySeconds;
            agent.confidence = std::max(0.0,agent.confidence-decay);
            if (agent.memoryAgeSeconds >= config_.memorySeconds) {
                agent.confidence = 0.0;
                agent.memoryAgeSeconds = config_.memorySeconds;
                agent.perceptionSource = AIPerceptionSource::None;
            }
        }

        const double proximity = 1.0-clamp01(distanceXZ/config_.maxVisionDistanceMeters);
        const double movementThreat = clamp01(playerSpeed/6.0);
        agent.threat = clamp01(agent.confidence*(0.55 + 0.25*proximity + 0.20*movementThreat));

        if (agent.hasLineOfSight && agent.confidence >= config_.engagedConfidence) agent.alert = AIAlertState::Engaged;
        else if (agent.confidence >= config_.engagedConfidence) agent.alert = AIAlertState::Investigating;
        else if (agent.confidence >= config_.suspiciousConfidence) agent.alert = AIAlertState::Suspicious;
        else agent.alert = AIAlertState::Unaware;
    }
}

TacticalAIReport TacticalAICore::report() const noexcept {
    TacticalAIReport out{};
    for (std::size_t i=0;i<agentCount_;++i) {
        const auto& a=agents_[i];
        if (a.id==0 || !a.alive) continue;
        ++out.activeAgents;
        if (a.hasLineOfSight) ++out.lineOfSightAgents;
        if (a.heardPlayer) ++out.hearingAgents;
        if (a.alert==AIAlertState::Suspicious) ++out.suspiciousAgents;
        else if (a.alert==AIAlertState::Investigating) ++out.investigatingAgents;
        else if (a.alert==AIAlertState::Engaged) ++out.engagedAgents;
        out.highestThreat = std::max(out.highestThreat,a.threat);
    }
    if (out.engagedAgents >= std::max<std::uint32_t>(1,out.activeAgents/3)) out.squadOrder = AISquadOrder::Assault;
    else if (out.engagedAgents>0 || out.investigatingAgents>0 || out.suspiciousAgents>0) out.squadOrder = AISquadOrder::Search;
    else out.squadOrder = AISquadOrder::Hold;
    return out;
}

bool TacticalAICore::validate() const noexcept {
    if (agentCount_>kMaxAgents || !std::isfinite(config_.maxVisionDistanceMeters) || config_.maxVisionDistanceMeters<=0.0 ||
        !std::isfinite(config_.horizontalFovRadians) || config_.horizontalFovRadians<=0.0 ||
        !std::isfinite(config_.memorySeconds) || config_.memorySeconds<=0.0 ||
        !std::isfinite(config_.hearingBaseMeters) || !std::isfinite(config_.hearingMaxMeters) || config_.hearingBaseMeters<0.0 || config_.hearingMaxMeters<config_.hearingBaseMeters ||
        !std::isfinite(config_.hearingMaxLocalizationErrorMeters) || config_.hearingMaxLocalizationErrorMeters<0.0) return false;
    for (std::size_t i=0;i<agentCount_;++i) {
        const auto& a=agents_[i];
        if (a.id==0) continue;
        if (!std::isfinite(a.position.x)||!std::isfinite(a.position.y)||!std::isfinite(a.position.z)||!std::isfinite(a.facingYaw)||
            !std::isfinite(a.lastKnownPlayerPosition.x)||!std::isfinite(a.lastKnownPlayerPosition.y)||!std::isfinite(a.lastKnownPlayerPosition.z)||
            !std::isfinite(a.memoryAgeSeconds)||a.memoryAgeSeconds<0.0||!std::isfinite(a.confidence)||a.confidence<0.0||a.confidence>1.000001||
            !std::isfinite(a.threat)||a.threat<0.0||a.threat>1.000001) return false;
        if (a.alert==AIAlertState::Engaged && (!a.hasLineOfSight || a.perceptionSource!=AIPerceptionSource::Vision)) return false;
    }
    return true;
}

double TacticalAICore::clamp01(double value) noexcept { return std::clamp(value,0.0,1.0); }
double TacticalAICore::lengthXZ(Vec3 value) noexcept { return std::hypot(value.x,value.z); }
double TacticalAICore::wrapAngle(double radians) noexcept {
    while (radians>kPi) radians-=2.0*kPi;
    while (radians<-kPi) radians+=2.0*kPi;
    return radians;
}
Vec3 TacticalAICore::hearingEstimate(std::uint32_t agentId, Vec3 playerPosition, double confidence, double maxErrorMeters) noexcept {
    if (maxErrorMeters<=0.0 || confidence>=0.999) return playerPosition;
    // Stable per-agent phase avoids frame-to-frame jitter and remains deterministic.
    const double phase = std::fmod(static_cast<double>(agentId)*2.399963229728653,2.0*kPi);
    const double radius = maxErrorMeters*(1.0-clamp01(confidence));
    return {playerPosition.x+std::cos(phase)*radius,playerPosition.y,playerPosition.z+std::sin(phase)*radius};
}

} // namespace metse
