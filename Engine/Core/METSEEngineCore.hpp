#pragma once
#include "METSEBallisticsCore.hpp"
#include "METSECharacterMotor.hpp"
#include "METSEDamageCore.hpp"
#include "METSEInputCommandQueue.hpp"
#include "METSEIntegrityCore.hpp"
#include "METSEObservatoryCore.hpp"
#include "METSEVisibilityCore.hpp"
#include "METSEWeaponCore.hpp"
#include "METSEWorldCollision.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace metse {
struct EngineConfig{double fixedStepSeconds=1.0/60.0;std::uint32_t maxCatchUpSteps=4,maxCombatants=32;CharacterConfig character{};WeaponConfig weapon{};};
struct GameplayDenialMetrics{std::uint64_t fireCooldown=0,fireReloading=0,fireObstructed=0,fireEmpty=0,fireSprintRecovery=0,projectileCapacity=0,reloadInvalid=0,autoReloadStarted=0;};
struct EngineSnapshot{double simulationSeconds=0,interpolationAlpha=0,playerX=0,playerY=0,playerZ=0,velocityX=0,velocityY=0,velocityZ=0,playerBodyYaw=0,playerYaw=0,playerPitch=0,cameraHeight=1.64,cameraRoll=0,cameraLean=0,horizontalSpeed=0;std::uint64_t simulationTick=0,shotsFired=0,collisionContacts=0;std::uint32_t activeCombatants=0,ammoInMagazine=30,reserveAmmo=90,activeProjectiles=0;double adsAlpha=0,reloadRemaining=0,sprintRecoveryRemaining=0,recoilPitch=0,recoilYaw=0,weaponSwayX=0,weaponSwayY=0;CharacterStance stance=CharacterStance::Standing;CharacterGait gait=CharacterGait::Idle;ReloadKind reloadKind=ReloadKind::None;bool grounded=true,sprinting=false,reloading=false,weaponObstructed=false;double primaryTargetHealth=100.0;std::uint64_t damageHits=0,damageKills=0;VisibilityReport visibility{};};
struct BlackBoxFrame{std::uint64_t simulationTick=0;double realDeltaSeconds=0,playerX=0,playerY=0,playerZ=0,horizontalSpeed=0,adsAlpha=0;std::uint32_t queueDepth=0,activeProjectiles=0,catchUpSteps=0;std::uint64_t shotsFired=0,damageHits=0;CharacterStance stance=CharacterStance::Standing;CharacterGait gait=CharacterGait::Idle;bool grounded=true,sprinting=false,reloading=false,catchUpClamped=false;};
struct EngineDiagnostics{IntegrityMetrics integrity{};Sha256Digest journalHead{},stateHash{};ObservatoryReport observatory{};InputQueueMetrics inputQueue{};BallisticsMetrics ballistics{};VisibilityReport visibility{};GameplayDenialMetrics gameplayDenials{};std::size_t retainedEvents=0,retainedCommands=0,retainedBlackBoxFrames=0,worldObstacleCount=0,inputQueueDepth=0;std::uint64_t sessionCollisionContacts=0,simulationInvariantRollbacks=0,damageHits=0,damageKills=0;bool journalValid=false,worldValid=false,observatoryValid=false,inputQueueValid=false,weaponValid=false,ballisticsValid=false,damageValid=false,visibilityValid=false;};

class EngineCore final{
public:static constexpr std::size_t kBlackBoxCapacity=720;explicit EngineCore(EngineConfig config={});
void reset();void advance(double realDeltaSeconds);bool setActiveCombatants(std::uint32_t count);
bool setMovementInput(double forward,double strafe)noexcept{return inputQueue_.pushMove(forward,strafe);}bool addLookInput(double yaw,double pitch)noexcept{return inputQueue_.pushLook(yaw,pitch);}bool setSprintHeld(bool held)noexcept{return inputQueue_.pushSprint(held);}bool setAimHeld(bool held)noexcept{return inputQueue_.pushAim(held);}bool cycleStance()noexcept{return inputQueue_.pushCycleStance();}bool triggerFire()noexcept{return inputQueue_.pushFire();}bool reloadWeapon()noexcept{return inputQueue_.pushReload();}
[[nodiscard]]const EngineSnapshot& snapshot()const noexcept{return state_;}[[nodiscard]]const EngineConfig& config()const noexcept{return config_;}[[nodiscard]]EngineDiagnostics diagnostics()const noexcept;[[nodiscard]]bool newestBlackBoxFrame(std::size_t offset,BlackBoxFrame&out)const noexcept;[[nodiscard]]const std::array<WorldObstacle,WorldCollisionCore::kMaxObstacles>& worldObstacles()const noexcept{return world_.obstacles();}[[nodiscard]]std::size_t worldObstacleCount()const noexcept{return world_.obstacleCount();}[[nodiscard]]const WorldCollisionCore& worldCollision()const noexcept{return world_;}[[nodiscard]]const std::array<Projectile,BallisticsCore::kMaxProjectiles>& projectiles()const noexcept{return ballistics_.projectiles();}[[nodiscard]]const std::array<DamageTarget,DamageCore::kMaxTargets>& damageTargets()const noexcept{return damage_.targets();}[[nodiscard]]std::size_t damageTargetCount()const noexcept{return damage_.targetCount();}[[nodiscard]]Sha256Digest deterministicStateHash()const noexcept;
#ifdef METSE_TESTING
bool testOnlyExecuteInvariantViolation();void testOnlySetAirborne(double h,double vy)noexcept;
#endif
private:
struct MutationCheckpoint{EngineSnapshot state{};CharacterMotor character{};WeaponCore weapon{};BallisticsCore ballistics{};DamageCore damage{};VisibilityCore visibility{};double accumulator=0,moveForward=0,moveStrafe=0;bool sprintHeld=false;};
template<class Apply>bool executeAtomic(CommandKind kind,bool precondition,EventKind event,Apply&&apply){
if(!precondition){
    if(kind==CommandKind::FireWeapon){const auto&w=weapon_.state();if(w.reloading)++gameplayDenials_.fireReloading;else if(w.obstructed)++gameplayDenials_.fireObstructed;else if(w.sprinting||w.sprintRecoveryRemaining>0.0)++gameplayDenials_.fireSprintRecovery;else if(w.fireCooldown>0.0)++gameplayDenials_.fireCooldown;else if(w.ammoInMagazine==0)++gameplayDenials_.fireEmpty;else if(ballistics_.activeCount()>=BallisticsCore::kMaxProjectiles)++gameplayDenials_.projectileCapacity;else ++gameplayDenials_.fireCooldown;return false;}
    if(kind==CommandKind::ReloadWeapon){++gameplayDenials_.reloadInvalid;return false;}
    std::uint64_t rejectedId=integrity_.admit(kind,state_.simulationTick);integrity_.reject(rejectedId,state_.simulationTick);return false;
}
std::uint64_t commandId=integrity_.admit(kind,state_.simulationTick);MutationCheckpoint cp{state_,character_,weapon_,ballistics_,damage_,visibility_,accumulatorSeconds_,moveForward_,moveStrafe_,sprintHeld_};bool applied=apply(commandId);syncSnapshot();if(!applied||!validateInvariants()){state_=cp.state;character_=cp.character;weapon_=cp.weapon;ballistics_=cp.ballistics;damage_=cp.damage;visibility_=cp.visibility;accumulatorSeconds_=cp.accumulator;moveForward_=cp.moveForward;moveStrafe_=cp.moveStrafe;sprintHeld_=cp.sprintHeld;integrity_.rollback(commandId,state_.simulationTick);return false;}integrity_.commit(commandId,state_.simulationTick);integrity_.appendDomainEvent(event,commandId,state_.simulationTick);return true;}
void resetState()noexcept;void drainInputQueue()noexcept;void applyDiscrete(const InputCommand&cmd)noexcept;void fixedStep()noexcept;void updateWeaponObstruction()noexcept;void syncSnapshot()noexcept;bool validateInvariants()const noexcept;void recordBlackBox(double dt,std::uint32_t steps,bool clamped)noexcept;void observeFrame(double dt,std::uint32_t steps,bool clamped)noexcept;Vec3 cameraPosition()const noexcept;
EngineConfig config_{};EngineSnapshot state_{};CharacterMotor character_{};WeaponCore weapon_{};WorldCollisionCore world_{};BallisticsCore ballistics_{};DamageCore damage_{};VisibilityCore visibility_{};ObservatoryCore observatory_{};InputCommandQueue inputQueue_{};IntegrityCore integrity_{};GameplayDenialMetrics gameplayDenials_{};double accumulatorSeconds_=0,moveForward_=0,moveStrafe_=0;bool sprintHeld_=false;std::array<BlackBoxFrame,kBlackBoxCapacity>blackBox_{};std::size_t blackBoxWrite_=0,blackBoxCount_=0;std::uint64_t sessionCollisionContacts_=0,simulationInvariantRollbacks_=0,lastDamageResultSequence_=0;std::uint32_t frameCollisionContacts_=0;};
} // namespace metse
