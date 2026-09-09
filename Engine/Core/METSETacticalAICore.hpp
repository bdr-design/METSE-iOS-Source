#pragma once
#include "METSEWeaponCore.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace metse {
class WorldCollisionCore;
struct ProjectileSegmentObservation;

enum class AIAlertState : std::uint8_t { Unaware=0, Suspicious=1, Investigating=2, Engaged=3 };
enum class AISquadOrder : std::uint8_t { Hold=0, Search=1, Assault=2, Defend=3 };
enum class AIPerceptionSource : std::uint8_t { None=0, Hearing=1, Vision=2, Squad=3 };
enum class AIActionState : std::uint8_t {
    Hold=0,
    MoveToCover=1,
    Peek=2,
    Reload=3,
    Suppress=4,
    Flank=5,
    Retreat=6,
    Search=7
};

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
    double decisionIntervalSeconds = 0.20;
    double moveSpeedMetersPerSecond = 2.65;
    double retreatSpeedMetersPerSecond = 3.45;
    double coverSearchRadiusMeters = 24.0;
    double coverArrivalRadiusMeters = 0.48;
    double peekOffsetMeters = 0.78;
    double flankOffsetMeters = 4.5;
    double retreatHealth01 = 0.38;
    double squadShareRangeMeters = 18.0;
    double squadShareFreshSeconds = 1.50;
    double squadShareMaxLocalizationErrorMeters = 2.25;
};

struct TacticalAgentState {
    std::uint32_t id = 0;
    Vec3 position{};
    Vec3 lastKnownPlayerPosition{};
    Vec3 actionTarget{};
    Vec3 coverPosition{};
    Vec3 peekPosition{};
    double facingYaw = 0.0;
    double memoryAgeSeconds = 0.0;
    double decisionAgeSeconds = 0.0;
    double actionAgeSeconds = 0.0;
    double confidence = 0.0;
    double threat = 0.0;
    double health01 = 1.0;
    double suppression01 = 0.0;
    std::uint64_t lastSuppressionCorrelationId = 0;
    std::uint64_t actionSequence = 0;
    std::uint32_t squadSourceAgentId = 0;
    std::uint8_t coverCandidateIndex = 0xFFu;
    AIAlertState alert = AIAlertState::Unaware;
    AIPerceptionSource perceptionSource = AIPerceptionSource::None;
    AIActionState action = AIActionState::Hold;
    bool alive = false;
    bool combatCapable = false;
    bool hasLineOfSight = false;
    bool heardPlayer = false;
    bool hasCover = false;
    bool fireAuthorized = false;
};

struct TacticalAIReport {
    std::uint32_t activeAgents = 0;
    std::uint32_t lineOfSightAgents = 0;
    std::uint32_t hearingAgents = 0;
    std::uint32_t suspiciousAgents = 0;
    std::uint32_t investigatingAgents = 0;
    std::uint32_t engagedAgents = 0;
    std::uint32_t agentsInCover = 0;
    std::uint32_t moveToCoverAgents = 0;
    std::uint32_t peekAgents = 0;
    std::uint32_t reloadAgents = 0;
    std::uint32_t suppressAgents = 0;
    std::uint32_t flankAgents = 0;
    std::uint32_t retreatAgents = 0;
    std::uint32_t searchAgents = 0;
    std::uint32_t fireAuthorizedAgents = 0;
    std::uint64_t decisionsExecuted = 0;
    std::uint64_t shotsFired = 0;
    std::uint32_t suppressedAgents = 0;
    std::uint32_t suppressionChecksThisStep = 0;
    std::uint64_t suppressionObservations = 0;
    std::uint64_t suppressionBudgetDrops = 0;
    double highestThreat = 0.0;
    AISquadOrder squadOrder = AISquadOrder::Hold;
};

class TacticalAICore final {
public:
    static constexpr std::size_t kMaxAgents = 32;
    static constexpr std::size_t kMaxDecisionsPerStep = 4;
    static constexpr std::size_t kMaxSuppressionChecksPerStep = 256;
    static constexpr double kSuppressionRadiusMeters = 1.5;
    static constexpr double kSuppressionThreshold = 0.5;
    static constexpr double kSuppressionDecayPerSecond = 0.5;
    static constexpr std::uint8_t kNoCoverCandidate = 0xFFu;

    explicit TacticalAICore(TacticalAIConfig config = {}, WeaponConfig weaponConfig = {}) noexcept;
    void reset() noexcept;

    // Full synchronization is used for spawn/reset. During normal simulation the
    // Tactical AI position remains authoritative for AI locomotion and EngineCore
    // mirrors that position into DamageCore for ballistic intersection truth.
    bool syncAgent(std::size_t index,std::uint32_t id,Vec3 position,double facingYaw,bool alive) noexcept;
    bool syncAgent(std::size_t index,std::uint32_t id,Vec3 position,double facingYaw,bool alive,bool combatCapable,double health01) noexcept;
    bool syncAgentCombatState(std::size_t index,std::uint32_t id,bool alive,bool combatCapable,double health01) noexcept;

    void fixedStep(double dt,
                   const WorldCollisionCore& world,
                   Vec3 playerPosition,
                   Vec3 playerVelocity,
                   double playerNoise01) noexcept;

    // Returns accepted AI weapon shots in a fixed caller-provided array. The method
    // consumes only agents already authorized by fresh Vision + WorldCollision and
    // never performs damage or projectile ownership itself.
    std::size_t fireAuthorizedShots(std::size_t maxShots,
                                    std::array<ShotSolution,kMaxAgents>& out) noexcept;

    // Called only for accepted ballistic traversal, after this slice's AI step.
    // Observes bounded local exposure; never grants knowledge of a shooter.
    void observeProjectileSegment(const ProjectileSegmentObservation& segment,
                                  const CombatantCore& combatants,
                                  const WorldCollisionCore& world) noexcept;

    [[nodiscard]] const TacticalAIConfig& config() const noexcept { return config_; }
    [[nodiscard]] const WeaponConfig& weaponConfig() const noexcept { return weaponConfig_; }
    [[nodiscard]] const std::array<TacticalAgentState,kMaxAgents>& agents() const noexcept { return agents_; }
    [[nodiscard]] std::size_t agentCount() const noexcept { return agentCount_; }
    [[nodiscard]] const WeaponState* agentWeaponState(std::size_t index) const noexcept;
    [[nodiscard]] TacticalAIReport report() const noexcept;
    [[nodiscard]] bool validate() const noexcept;

#ifdef METSE_TESTING
    bool testOnlyDrainAgentMagazine(std::size_t index) noexcept;
#endif

private:
    static double clamp01(double value) noexcept;
    static double distanceXZ(Vec3 a,Vec3 b) noexcept;
    static double wrapAngle(double radians) noexcept;
    static bool finiteVec(Vec3 value) noexcept;
    static Vec3 hearingEstimate(std::uint32_t agentId,Vec3 playerPosition,double confidence,double maxErrorMeters) noexcept;
    static Vec3 squadEstimate(std::uint32_t recipientId,std::uint32_t sourceId,Vec3 knownPosition,double confidence,double maxErrorMeters) noexcept;

    void clearKnowledgeAndAction(TacticalAgentState& agent) noexcept;
    void perceiveAgent(TacticalAgentState& agent,
                       const WorldCollisionCore& world,
                       Vec3 playerPosition,
                       Vec3 playerVelocity,
                       double noise,
                       double hearingRadius,
                       double dt) noexcept;
    void shareKnowledgeForAgent(std::size_t agentIndex) noexcept;
    void decideAgent(std::size_t agentIndex,const WorldCollisionCore& world) noexcept;
    void advanceAction(TacticalAgentState& agent,const WorldCollisionCore& world,double dt) noexcept;
    bool selectCover(const TacticalAgentState& agent,
                     const WorldCollisionCore& world,
                     Vec3 threatPosition,
                     bool preferRetreat,
                     Vec3& outPosition,
                     Vec3& outPeekPosition,
                     std::uint8_t& outCandidateIndex) const noexcept;
    bool computePeekPoint(const TacticalAgentState& agent,
                          const WorldCollisionCore& world,
                          std::size_t coverCandidateIndex,
                          Vec3 threatPosition,
                          Vec3& outPeek) const noexcept;
    bool directPathClear(const WorldCollisionCore& world,Vec3 from,Vec3 to) const noexcept;
    bool authorizeFire(std::size_t agentIndex,const WorldCollisionCore& world) noexcept;
    void setAction(TacticalAgentState& agent,AIActionState action,Vec3 target) noexcept;

    TacticalAIConfig config_{};
    WeaponConfig weaponConfig_{};
    std::array<TacticalAgentState,kMaxAgents> agents_{};
    // Default-initialize each WeaponCore directly. Braced aggregate initialization
    // would copy-list-initialize omitted elements and reject WeaponCore's explicit
    // canonical constructor under libc++/Clang.
    std::array<WeaponCore,kMaxAgents> weapons_;
    std::size_t agentCount_ = 0;
    std::size_t decisionCursor_ = 0;
    std::uint64_t decisionsExecuted_ = 0;
    std::uint32_t suppressionChecksThisStep_ = 0;
    std::uint64_t suppressionObservations_ = 0;
    std::uint64_t suppressionBudgetDrops_ = 0;
};

} // namespace metse
