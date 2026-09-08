#pragma once
#include "METSEWeaponCore.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace metse {
class WorldCollisionCore;

enum class AIAlertState : std::uint8_t { Unaware=0, Suspicious=1, Investigating=2, Engaged=3 };
enum class AISquadOrder : std::uint8_t { Hold=0, Search=1, Assault=2, Defend=3 };
enum class AIPerceptionSource : std::uint8_t { None=0, Hearing=1, Vision=2 };

struct TacticalAIConfig {
    double maxVisionDistanceMeters = 72.0;
    double horizontalFovRadians = 1.91986217719; // 110 degrees, full horizontal FOV.
    double memorySeconds = 8.0;
    double hearingBaseMeters = 6.0;
    double hearingMaxMeters = 34.0;
    double hearingMaxLocalizationErrorMeters = 7.0;
    double agentEyeHeight = 1.58;
    double suspiciousConfidence = 0.20;
    double engagedConfidence = 0.62;
};

struct TacticalAgentState {
    std::uint32_t id = 0;
    Vec3 position{};
    Vec3 lastKnownPlayerPosition{};
    double facingYaw = 0.0;
    double memoryAgeSeconds = 0.0;
    double confidence = 0.0;
    double threat = 0.0;
    AIAlertState alert = AIAlertState::Unaware;
    AIPerceptionSource perceptionSource = AIPerceptionSource::None;
    bool alive = false;
    bool hasLineOfSight = false;
    bool heardPlayer = false;
};

struct TacticalAIReport {
    std::uint32_t activeAgents = 0;
    std::uint32_t lineOfSightAgents = 0;
    std::uint32_t hearingAgents = 0;
    std::uint32_t suspiciousAgents = 0;
    std::uint32_t investigatingAgents = 0;
    std::uint32_t engagedAgents = 0;
    double highestThreat = 0.0;
    AISquadOrder squadOrder = AISquadOrder::Hold;
};

class TacticalAICore final {
public:
    static constexpr std::size_t kMaxAgents = 32;
    explicit TacticalAICore(TacticalAIConfig config = {}) noexcept;
    void reset() noexcept;
    bool syncAgent(std::size_t index, std::uint32_t id, Vec3 position, double facingYaw, bool alive) noexcept;
    void fixedStep(double dt,
                   const WorldCollisionCore& world,
                   Vec3 playerPosition,
                   Vec3 playerVelocity,
                   double playerNoise01) noexcept;
    [[nodiscard]] const TacticalAIConfig& config() const noexcept { return config_; }
    [[nodiscard]] const std::array<TacticalAgentState,kMaxAgents>& agents() const noexcept { return agents_; }
    [[nodiscard]] std::size_t agentCount() const noexcept { return agentCount_; }
    [[nodiscard]] TacticalAIReport report() const noexcept;
    [[nodiscard]] bool validate() const noexcept;

private:
    static double clamp01(double value) noexcept;
    static double lengthXZ(Vec3 value) noexcept;
    static double wrapAngle(double radians) noexcept;
    static Vec3 hearingEstimate(std::uint32_t agentId, Vec3 playerPosition, double confidence, double maxErrorMeters) noexcept;
    TacticalAIConfig config_{};
    std::array<TacticalAgentState,kMaxAgents> agents_{};
    std::size_t agentCount_ = 0;
};

} // namespace metse
