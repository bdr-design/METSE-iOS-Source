#pragma once
#include "METSEAudioFXCore.hpp"
#include "METSEBallisticsCore.hpp"
#include "METSECharacterMotor.hpp"
#include "METSEDamageCore.hpp"
#include "METSEInputCommandQueue.hpp"
#include "METSEIntegrityCore.hpp"
#include "METSEObservatoryCore.hpp"
#include "METSETacticalAICore.hpp"
#include "METSEVisibilityCore.hpp"
#include "METSEWeaponCore.hpp"
#include "METSEWorldCollision.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace metse {

struct EngineConfig {
    double fixedStepSeconds = 1.0/60.0;
    std::uint32_t maxCatchUpSteps = 4;
    std::uint32_t maxCombatants = 32;
    CharacterConfig character{};
    WeaponConfig weapon{};
    TacticalAIConfig tacticalAI{};
};

struct GameplayDenialMetrics {
    std::uint64_t fireCooldown = 0;
    std::uint64_t fireReloading = 0;
    std::uint64_t fireObstructed = 0;
    std::uint64_t fireEmpty = 0;
    std::uint64_t fireSprintRecovery = 0;
    std::uint64_t projectileCapacity = 0;
    std::uint64_t reloadInvalid = 0;
    std::uint64_t autoReloadStarted = 0;
};

struct EngineSnapshot {
    double simulationSeconds = 0.0;
    double interpolationAlpha = 0.0;
    double playerX = 0.0, playerY = 0.0, playerZ = 0.0;
    double velocityX = 0.0, velocityY = 0.0, velocityZ = 0.0;
    double playerBodyYaw = 0.0, playerYaw = 0.0, playerPitch = 0.0;
    double cameraHeight = 1.64, cameraRoll = 0.0, cameraLean = 0.0;
    double horizontalSpeed = 0.0;
    std::uint64_t simulationTick = 0;
    std::uint64_t shotsFired = 0;
    std::uint64_t collisionContacts = 0;
    std::uint32_t activeCombatants = 0;
    std::uint32_t ammoInMagazine = 30;
    std::uint32_t reserveAmmo = 90;
    std::uint32_t activeProjectiles = 0;
    double adsAlpha = 0.0;
    double reloadRemaining = 0.0;
    double sprintRecoveryRemaining = 0.0;
    double recoilPitch = 0.0, recoilYaw = 0.0;
    double weaponSwayX = 0.0, weaponSwayY = 0.0;
    CharacterStance stance = CharacterStance::Standing;
    CharacterGait gait = CharacterGait::Idle;
    ReloadKind reloadKind = ReloadKind::None;
    bool grounded = true;
    bool sprinting = false;
    bool reloading = false;
    bool weaponObstructed = false;
    double primaryTargetHealth = 100.0;
    double primaryTargetBleedingPerSecond = 0.0;
    double primaryTargetHelmetArmorJoules = 420.0;
    double primaryTargetTorsoArmorJoules = 900.0;
    CombatState primaryTargetCombatState = CombatState::Effective;
    std::uint64_t damageHits = 0;
    std::uint64_t damageKills = 0;
    std::uint64_t damageIncapacitations = 0;
    VisibilityReport visibility{};
    TacticalAIReport tacticalAI{};
    AudioFXReport audioFX{};
};

struct BlackBoxFrame {
    std::uint64_t simulationTick = 0;
    double realDeltaSeconds = 0.0;
    double playerX = 0.0, playerY = 0.0, playerZ = 0.0;
    double horizontalSpeed = 0.0;
    double adsAlpha = 0.0;
    double primaryTargetHealth = 0.0;
    double primaryTargetBleedingPerSecond = 0.0;
    std::uint32_t queueDepth = 0;
    std::uint32_t activeProjectiles = 0;
    std::uint32_t catchUpSteps = 0;
    std::uint32_t aiEngaged = 0;
    std::uint64_t shotsFired = 0;
    std::uint64_t damageHits = 0;
    std::uint64_t damageIncapacitations = 0;
    CharacterStance stance = CharacterStance::Standing;
    CharacterGait gait = CharacterGait::Idle;
    CombatState primaryTargetCombatState = CombatState::Effective;
    bool grounded = true;
    bool sprinting = false;
    bool reloading = false;
    bool catchUpClamped = false;
    double simulationSliceMilliseconds = 0.0;
    std::uint64_t projectileContacts = 0;
    std::uint64_t projectileTerminalContacts = 0;
    std::uint64_t projectileTargetContacts = 0;
    std::uint32_t aiActiveAgents = 0;
    std::uint32_t aiLOSAgents = 0;
    std::uint32_t aiDecisions = 0;
    // True only for a clamped/slow simulation callback, not the expected 30 FPS
    // presentation fallback (about two 60 Hz fixed steps).
    bool preSpike = false;
};

struct EngineDiagnostics {
    IntegrityMetrics integrity{};
    Sha256Digest journalHead{}, stateHash{};
    ObservatoryReport observatory{};
    InputQueueMetrics inputQueue{};
    BallisticsMetrics ballistics{};
    VisibilityReport visibility{};
    TacticalAIReport tacticalAI{};
    AudioFXReport audioFX{};
    GameplayDenialMetrics gameplayDenials{};
    std::size_t retainedEvents = 0;
    std::size_t retainedCommands = 0;
    std::size_t retainedBlackBoxFrames = 0;
    std::uint64_t preSpikeBlackBoxFrames = 0;
    std::size_t worldObstacleCount = 0;
    std::size_t inputQueueDepth = 0;
    std::uint64_t sessionCollisionContacts = 0;
    std::uint64_t simulationInvariantRollbacks = 0;
    std::uint64_t damageHits = 0;
    std::uint64_t damageKills = 0;
    std::uint64_t damageIncapacitations = 0;
    std::uint64_t damageArmorHits = 0;
    std::uint64_t damageBleedTransitions = 0;
    bool journalValid = false;
    bool worldValid = false;
    bool observatoryValid = false;
    bool inputQueueValid = false;
    bool weaponValid = false;
    bool ballisticsValid = false;
    bool damageValid = false;
    bool visibilityValid = false;
    bool tacticalAIValid = false;
    bool audioFXValid = false;
};

class EngineCore final {
public:
    static constexpr std::size_t kBlackBoxCapacity = 720;
    explicit EngineCore(EngineConfig config = {});
    void reset();
    void advance(double realDeltaSeconds);
    bool setActiveCombatants(std::uint32_t count);

    bool setMovementInput(double forward,double strafe) noexcept { return inputQueue_.pushMove(forward,strafe); }
    bool addLookInput(double yaw,double pitch) noexcept { return inputQueue_.pushLook(yaw,pitch); }
    bool setSprintHeld(bool held) noexcept { return inputQueue_.pushSprint(held); }
    bool setAimHeld(bool held) noexcept { return inputQueue_.pushAim(held); }
    bool cycleStance() noexcept { return inputQueue_.pushCycleStance(); }
    bool triggerFire() noexcept { return inputQueue_.pushFire(); }
    bool reloadWeapon() noexcept { return inputQueue_.pushReload(); }

    [[nodiscard]] const EngineSnapshot& snapshot() const noexcept { return state_; }
    [[nodiscard]] const EngineConfig& config() const noexcept { return config_; }
    [[nodiscard]] EngineDiagnostics diagnostics() const noexcept;
    [[nodiscard]] bool newestBlackBoxFrame(std::size_t offset,BlackBoxFrame& out) const noexcept;
    [[nodiscard]] const std::array<WorldObstacle,WorldCollisionCore::kMaxObstacles>& worldObstacles() const noexcept { return world_.obstacles(); }
    [[nodiscard]] std::size_t worldObstacleCount() const noexcept { return world_.obstacleCount(); }
    [[nodiscard]] const WorldCollisionCore& worldCollision() const noexcept { return world_; }
    [[nodiscard]] const std::array<Projectile,BallisticsCore::kMaxProjectiles>& projectiles() const noexcept { return ballistics_.projectiles(); }
    [[nodiscard]] const std::array<DamageTarget,DamageCore::kMaxTargets>& damageTargets() const noexcept { return damage_.targets(); }
    [[nodiscard]] std::size_t damageTargetCount() const noexcept { return damage_.targetCount(); }
    [[nodiscard]] const TacticalAICore& tacticalAI() const noexcept { return tacticalAI_; }
    [[nodiscard]] const VisibilityCore& visibilityCore() const noexcept { return visibility_; }
    [[nodiscard]] const AudioFXCore& audioFX() const noexcept { return audioFX_; }
    [[nodiscard]] Sha256Digest deterministicStateHash() const noexcept;

#ifdef METSE_TESTING
    bool testOnlyExecuteInvariantViolation();
    void testOnlySetAirborne(double h,double vy) noexcept;
    bool testOnlySpawnProjectile(const ShotSolution& shot) noexcept { return ballistics_.spawn(shot); }
    bool testOnlyNewestIntegrityEvent(std::size_t offset,EventRecord& out) const noexcept { return integrity_.newestEvent(offset,out); }
#endif

private:
    struct MutationCheckpoint {
        EngineSnapshot state{};
        CharacterMotor character{};
        WeaponCore weapon{};
        BallisticsCore ballistics{};
        DamageCore damage{};
        VisibilityCore visibility{};
        TacticalAICore tacticalAI{};
        AudioFXCore audioFX{};
        double accumulator = 0.0;
        double moveForward = 0.0;
        double moveStrafe = 0.0;
        std::uint64_t consumedDamageResultSequence = 0;
        bool sprintHeld = false;
    };

    template<class Apply>
    bool executeAtomic(CommandKind kind,bool precondition,EventKind event,Apply&& apply) {
        if(!precondition){
            if(kind==CommandKind::FireWeapon){
                const auto& w=weapon_.state();
                if(w.reloading) ++gameplayDenials_.fireReloading;
                else if(w.obstructed) ++gameplayDenials_.fireObstructed;
                else if(w.sprinting || w.sprintRecoveryRemaining>0.0) ++gameplayDenials_.fireSprintRecovery;
                else if(w.fireCooldown>0.0) ++gameplayDenials_.fireCooldown;
                else if(w.ammoInMagazine==0) ++gameplayDenials_.fireEmpty;
                else if(ballistics_.activeCount()>=BallisticsCore::kMaxProjectiles) ++gameplayDenials_.projectileCapacity;
                else ++gameplayDenials_.fireCooldown;
                return false;
            }
            if(kind==CommandKind::ReloadWeapon){ ++gameplayDenials_.reloadInvalid; return false; }
            const std::uint64_t rejectedId=integrity_.admit(kind,state_.simulationTick);
            integrity_.reject(rejectedId,state_.simulationTick);
            return false;
        }

        const std::uint64_t commandId=integrity_.admit(kind,state_.simulationTick);
        MutationCheckpoint cp{state_,character_,weapon_,ballistics_,damage_,visibility_,tacticalAI_,audioFX_,accumulatorSeconds_,moveForward_,moveStrafe_,consumedDamageResultSequence_,sprintHeld_};
        const bool applied=apply(commandId);
        syncSnapshot();
        if(!applied || !validateInvariants()){
            state_=cp.state;
            character_=cp.character;
            weapon_=cp.weapon;
            ballistics_=cp.ballistics;
            damage_=cp.damage;
            visibility_=cp.visibility;
            tacticalAI_=cp.tacticalAI;
            audioFX_=cp.audioFX;
            accumulatorSeconds_=cp.accumulator;
            moveForward_=cp.moveForward;
            moveStrafe_=cp.moveStrafe;
            consumedDamageResultSequence_=cp.consumedDamageResultSequence;
            sprintHeld_=cp.sprintHeld;
            integrity_.rollback(commandId,state_.simulationTick);
            return false;
        }
        integrity_.commit(commandId,state_.simulationTick);
        integrity_.appendDomainEvent(event,commandId,state_.simulationTick);
        return true;
    }

    void resetState() noexcept;
    void drainInputQueue() noexcept;
    void applyDiscrete(const InputCommand& cmd) noexcept;
    void fixedStep() noexcept;
    void updateWeaponObstruction() noexcept;
    void initializeTacticalAI() noexcept;
    void syncTacticalAICombatState() noexcept;
    void stepTacticalAI() noexcept;
    void mirrorTacticalPositionsToDamage() noexcept;
    void syncSnapshot() noexcept;
    bool validateInvariants() const noexcept;
    void recordBlackBox(double dt,std::uint32_t steps,bool clamped,double sliceMilliseconds) noexcept;
    void observeFrame(double dt,std::uint32_t steps,bool clamped,double sliceMilliseconds) noexcept;
    Vec3 cameraPosition() const noexcept;

    EngineConfig config_{};
    EngineSnapshot state_{};
    CharacterMotor character_{};
    WeaponCore weapon_{};
    WorldCollisionCore world_{};
    BallisticsCore ballistics_{};
    DamageCore damage_{};
    VisibilityCore visibility_{};
    TacticalAICore tacticalAI_{};
    AudioFXCore audioFX_{};
    ObservatoryCore observatory_{};
    InputCommandQueue inputQueue_{};
    IntegrityCore integrity_{};
    GameplayDenialMetrics gameplayDenials_{};
    double accumulatorSeconds_ = 0.0;
    double moveForward_ = 0.0;
    double moveStrafe_ = 0.0;
    bool sprintHeld_ = false;
    std::array<BlackBoxFrame,kBlackBoxCapacity> blackBox_{};
    std::size_t blackBoxWrite_ = 0;
    std::size_t blackBoxCount_ = 0;
    std::uint64_t preSpikeBlackBoxFrames_ = 0;
    std::uint64_t observedProjectileContacts_ = 0;
    std::uint64_t observedProjectileTerminalContacts_ = 0;
    std::uint64_t observedProjectileTargetContacts_ = 0;
    std::uint64_t sessionCollisionContacts_ = 0;
    std::uint64_t simulationInvariantRollbacks_ = 0;
    std::uint64_t consumedDamageResultSequence_ = 0;
    std::uint32_t frameCollisionContacts_ = 0;
};

} // namespace metse
