#include "METSEEngineCore.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <chrono>
#include <cmath>
#include <limits>
#include <span>

namespace metse {
namespace {

bool eq(double a,double b,double epsilon=1e-8) noexcept { return std::abs(a-b)<=epsilon; }

CombatantLifecycleState lifecycleFor(CombatState state) noexcept {
    switch(state){
        case CombatState::Wounded: return CombatantLifecycleState::Wounded;
        case CombatState::Incapacitated: return CombatantLifecycleState::Incapacitated;
        case CombatState::Dead: return CombatantLifecycleState::Dead;
        case CombatState::Effective: default: return CombatantLifecycleState::Active;
    }
}

struct AudioProjectileObserverContext {
    AudioFXCore *audio=nullptr;
    Vec3 listener{};
    TacticalAICore *tacticalAI=nullptr;
    const CombatantCore *combatants=nullptr;
    const WorldCollisionCore *world=nullptr;
};

void observeProjectileForAudio(void *rawContext,
                               const ProjectileSegmentObservation& segment) noexcept {
    auto *context=static_cast<AudioProjectileObserverContext *>(rawContext);
    if(context==nullptr||context->audio==nullptr) return;
    const auto* source=context->combatants?context->combatants->recordById(segment.sourceCombatantId):nullptr;
    const auto* listener=context->combatants?context->combatants->recordById(CombatantCore::kPlayerId):nullptr;
    const bool derivedHostile=source&&listener&&source->identity.teamId==segment.sourceTeamId&&
        source->identity.factionId==segment.sourceFactionId&&
        CombatantCore::relation(source->identity,listener->identity)==TargetRelation::Hostile;
    context->audio->observeProjectileSegment(segment,context->listener,derivedHostile);
    if(context->tacticalAI&&context->combatants&&context->world)
        context->tacticalAI->observeProjectileSegment(segment,*context->combatants,*context->world);
}

template<std::size_t N>
void put64(std::array<std::uint8_t,N>& buffer,std::size_t& cursor,std::uint64_t value) noexcept {
    assert(cursor+8<=N);
    if(cursor+8>N) return;
    for(int i=7;i>=0;--i) buffer[cursor++]=static_cast<std::uint8_t>(value>>(i*8));
}

template<std::size_t N>
void putD(std::array<std::uint8_t,N>& buffer,std::size_t& cursor,double value) noexcept {
    put64(buffer,cursor,std::bit_cast<std::uint64_t>(value));
}

} // namespace

EngineCore::EngineCore(EngineConfig config)
    : config_(config), character_(config.character), weapon_(config.weapon), tacticalAI_(config.tacticalAI,config.weapon) {
    if(!std::isfinite(config_.fixedStepSeconds) || config_.fixedStepSeconds<=0.0) config_.fixedStepSeconds=1.0/60.0;
    config_.maxCatchUpSteps=std::clamp<std::uint32_t>(config_.maxCatchUpSteps,1,8);
    config_.maxCombatants=std::clamp<std::uint32_t>(config_.maxCombatants,1,32);
    config_.character=character_.config();
    config_.weapon=weapon_.config();
    config_.tacticalAI=tacticalAI_.config();
    resetState();
    integrity_.appendSystemEvent(EventKind::EngineBoot,0);
}

void EngineCore::resetState() noexcept {
    state_={};
    accumulatorSeconds_=0.0;
    moveForward_=0.0;
    moveStrafe_=0.0;
    sprintHeld_=false;
    character_.reset();
    weapon_.reset();
    ballistics_.reset();
    damage_.reset();
    const std::size_t initialAgentCount=config_.maxCombatants>0?
        std::min<std::size_t>(2u,static_cast<std::size_t>(config_.maxCombatants-1u)):0u;
    (void)damage_.configureAgentCount(initialAgentCount);
    visibility_.reset();
    tacticalAI_.reset();
    audioFX_.reset();
    observatory_.reset();
    inputQueue_.reset();
    gameplayDenials_={};
    sessionCollisionContacts_=0;
    simulationInvariantRollbacks_=0;
    consumedDamageResultSequence_=0;
    frameCollisionContacts_=0;
    observedProjectileContacts_=0;
    observedProjectileTerminalContacts_=0;
    observedProjectileTargetContacts_=0;
    aiShotsFired_=0;
    aiTargetImpacts_=0;
    initializeCombatantAuthority();
    syncSnapshot();
}

void EngineCore::reset() {
    executeAtomic(CommandKind::ResetSession,true,EventKind::SessionReset,[&](std::uint64_t){
        resetState();
        blackBox_={};
        blackBoxWrite_=0;
        blackBoxCount_=0;
        preSpikeBlackBoxFrames_=0;
        preSpikeCallbackFrames_=0;
        preSpikeCatchUpFrames_=0;
        preSpikeSimulationFrames_=0;
        return true;
    });
}

bool EngineCore::setActiveCombatants(std::uint32_t count) {
    return executeAtomic(CommandKind::SetActiveCombatants,count>=1&&count<=config_.maxCombatants&&state_.simulationTick==0,EventKind::CombatantCountChanged,[&](std::uint64_t){
        if(!damage_.configureAgentCount(static_cast<std::size_t>(count-1u))) return false;
        initializeCombatantAuthority();
        state_.activeCombatants=count;
        return true;
    });
}

Vec3 EngineCore::cameraPosition() const noexcept {
    return {character_.state().x,character_.state().y+character_.cameraHeight(),character_.state().z};
}

void EngineCore::drainInputQueue() noexcept {
    InputCommand command{};
    std::size_t drained=0;
    while(drained<InputCommandQueue::kCapacity && inputQueue_.pop(command)){
        ++drained;
        switch(command.kind){
            case InputCommandKind::Move: moveForward_=command.a; moveStrafe_=command.b; break;
            case InputCommandKind::Look: character_.addLookInput(command.a,command.b); break;
            case InputCommandKind::Sprint: sprintHeld_=command.flag; break;
            case InputCommandKind::Aim: weapon_.setAimHeld(command.flag); break;
            case InputCommandKind::Fire:
            case InputCommandKind::Reload:
            case InputCommandKind::CycleStance: applyDiscrete(command); break;
        }
    }
    syncSnapshot();
}

void EngineCore::applyDiscrete(const InputCommand& command) noexcept {
    switch(command.kind){
        case InputCommandKind::Fire: {
            const auto& weaponState=weapon_.state();
            if(weaponState.ammoInMagazine==0 && weaponState.reserveAmmo>0 && !weaponState.reloading){
                const bool started=executeAtomic(CommandKind::ReloadWeapon,true,EventKind::ReloadStarted,[&](std::uint64_t){ return weapon_.requestReload(); });
                if(started) ++gameplayDenials_.autoReloadStarted;
                break;
            }
            const bool ready=DamageCore::combatCapable(damage_.playerTarget()) &&
                             weapon_.canFireNow() && ballistics_.activeCount()<BallisticsCore::kMaxProjectiles;
            executeAtomic(CommandKind::FireWeapon,ready,EventKind::ShotFired,[&](std::uint64_t id){
                ShotSolution shot{};
                if(!weapon_.fire(cameraPosition(),character_.cameraYaw(),character_.state().pitch,id,shot)) return false;
                const auto *player=combatants_.recordById(CombatantCore::kPlayerId);
                if(player==nullptr) return false;
                shot.sourceCombatantId=player->identity.id;
                shot.sourceTeamId=player->identity.teamId;
                shot.sourceFactionId=player->identity.factionId;
                shot.targetingPolicy=TargetingPolicy::HostileOnly;
                shot.includePlayerTarget=false;
                if(!ballistics_.spawn(shot)) return false;
                audioFX_.observeShot(shot.origin,id,world_);
                ++state_.shotsFired;
                return true;
            });
            break;
        }
        case InputCommandKind::Reload: {
            const auto& weaponState=weapon_.state();
            const bool canReload=!weaponState.reloading && weaponState.ammoInMagazine<weapon_.config().magazineSize && weaponState.reserveAmmo>0;
            executeAtomic(CommandKind::ReloadWeapon,canReload,EventKind::ReloadStarted,[&](std::uint64_t){ return weapon_.requestReload(); });
            break;
        }
        case InputCommandKind::CycleStance:
            executeAtomic(CommandKind::CycleStance,true,EventKind::StanceChanged,[&](std::uint64_t){
                CharacterMotor before=character_;
                character_.cycleStance();
                const double clearance=world_.clearanceHeightAt(character_.state().x,character_.state().z,character_.config().capsuleRadius);
                if(std::isfinite(clearance) && character_.capsuleHeight()>clearance-0.02){ character_=before; return false; }
                return true;
            });
            break;
        default: break;
    }
}

void EngineCore::advance(double realDeltaSeconds) {
    frameCollisionContacts_=0;
    drainInputQueue();
    if(!std::isfinite(realDeltaSeconds) || realDeltaSeconds<=0.0){
        state_.interpolationAlpha=std::clamp(accumulatorSeconds_/config_.fixedStepSeconds,0.0,1.0);
        recordBlackBox(0.0,0,false,0.0);
        observeFrame(0.0,0,false,0.0);
        return;
    }
    const double maxDelta=config_.fixedStepSeconds*config_.maxCatchUpSteps;
    const bool inputClamped=realDeltaSeconds>maxDelta;
    accumulatorSeconds_+=std::min(realDeltaSeconds,maxDelta);
    std::uint32_t steps=0;
    double sliceMilliseconds=0.0;
    while(accumulatorSeconds_>=config_.fixedStepSeconds && steps<config_.maxCatchUpSteps){
        const auto sliceStart=std::chrono::steady_clock::now();
        fixedStep();
        const auto sliceEnd=std::chrono::steady_clock::now();
        sliceMilliseconds=std::max(sliceMilliseconds,std::chrono::duration<double,std::milli>(sliceEnd-sliceStart).count());
        accumulatorSeconds_-=config_.fixedStepSeconds;
        ++steps;
    }
    bool backlog=false;
    if(steps==config_.maxCatchUpSteps && accumulatorSeconds_>=config_.fixedStepSeconds){
        accumulatorSeconds_=std::fmod(accumulatorSeconds_,config_.fixedStepSeconds);
        backlog=true;
    }
    const bool clamped=inputClamped||backlog;
    state_.interpolationAlpha=std::clamp(accumulatorSeconds_/config_.fixedStepSeconds,0.0,1.0);
    recordBlackBox(realDeltaSeconds,steps,clamped,sliceMilliseconds);
    observeFrame(realDeltaSeconds,steps,clamped,sliceMilliseconds);
}

void EngineCore::updateWeaponObstruction() noexcept {
    const Vec3 camera=cameraPosition();
    const double yaw=character_.cameraYaw(), pitch=character_.state().pitch;
    const double cp=std::cos(pitch), sp=std::sin(pitch), sy=std::sin(yaw), cy=std::cos(yaw);
    const Vec3 forward{sy*cp,sp,cy*cp};
    const Vec3 to{camera.x+forward.x*0.62,camera.y+forward.y*0.62-0.08,camera.z+forward.z*0.62};
    weapon_.setObstructed(world_.raycastSegment(camera,to).hit);
}

void EngineCore::initializeTacticalAI() noexcept {
    tacticalAI_.reset();
    for(std::size_t i=0;i<damage_.targetCount() && i<TacticalAICore::kMaxAgents;++i){
        const auto& target=damage_.targets()[i];
        const double fixedFacing=(i%2==0)?3.14159265358979323846:-2.35;
        tacticalAI_.syncAgent(i,target.id,target.position,fixedFacing,target.alive,DamageCore::combatCapable(target),target.health/100.0);
    }
}

void EngineCore::initializeCombatantAuthority() noexcept {
    combatants_.reset();
    const CombatantIdentity playerIdentity{CombatantCore::kPlayerId,
                                          CombatantCore::kPlayerTeam,
                                          CombatantCore::kPlayerFaction,
                                          CombatantRole::Player};
    (void)combatants_.configure(0,playerIdentity,true,true,true);
    (void)damage_.configurePlayerTarget(playerIdentity);
    (void)damage_.setPlayerTargetEnabled(true);
    const auto& playerState=character_.state();
    (void)damage_.syncPlayerTargetPosition(playerIdentity.id,{playerState.x,playerState.y,playerState.z});

    for(std::size_t i=0;i<damage_.targetCount();++i){
        const auto& target=damage_.targets()[i];
        const CombatantIdentity identity{target.id,
                                         CombatantCore::kHostileTeam,
                                         CombatantCore::kHostileFaction,
                                         CombatantRole::AI};
        (void)damage_.configureTargetIdentity(i,identity);
        (void)combatants_.configure(i+1u,identity,target.alive,DamageCore::combatCapable(target),true);
    }
    state_.activeCombatants=static_cast<std::uint32_t>(combatants_.count());
    initializeTacticalAI();
    visibility_.reset();
    for(std::size_t i=0;i<damage_.targetCount();++i){
        const auto& target=damage_.targets()[i];
        visibility_.syncTarget(i,target.id,target.position,target.alive);
    }
    visibility_.update(cameraPosition(),character_.cameraYaw(),world_);
}

void EngineCore::syncTacticalAICombatState() noexcept {
    const auto& player=damage_.playerTarget();
    (void)combatants_.syncLifecycle(0,player.id,lifecycleFor(player.combatState),player.alive);
    const std::size_t count=std::min(damage_.targetCount(),tacticalAI_.agentCount());
    for(std::size_t i=0;i<count;++i){
        const auto& target=damage_.targets()[i];
        tacticalAI_.syncAgentCombatState(i,target.id,target.alive,DamageCore::combatCapable(target),target.health/100.0);
        (void)combatants_.syncLifecycle(i+1u,target.id,lifecycleFor(target.combatState),target.alive);
    }
}

void EngineCore::mirrorTacticalPositionsToDamage() noexcept {
    const std::size_t count=std::min(damage_.targetCount(),tacticalAI_.agentCount());
    for(std::size_t i=0;i<count;++i){
        const auto& target=damage_.targets()[i];
        const auto& agent=tacticalAI_.agents()[i];
        if(agent.id==target.id) damage_.syncTargetPosition(i,target.id,agent.position);
    }
}

void EngineCore::stepTacticalAI() noexcept {
    syncTacticalAICombatState();
    const auto& characterState=character_.state();
    const double movementNoise=std::clamp(character_.horizontalSpeed()/6.0*0.52+(characterState.sprinting?0.26:0.0),0.0,0.78);
    tacticalAI_.fixedStep(config_.fixedStepSeconds,world_,
                          {characterState.x,characterState.y,characterState.z},
                          {characterState.velocityX,characterState.velocityY,characterState.velocityZ},
                          movementNoise);
    mirrorTacticalPositionsToDamage();
}

bool EngineCore::spawnAuthorizedAIShots() noexcept {
    if(!DamageCore::combatCapable(damage_.playerTarget())) return true;
    const std::size_t freeSlots=BallisticsCore::kMaxProjectiles-ballistics_.activeCount();
    if(freeSlots==0) return true;
    std::array<ShotSolution,TacticalAICore::kMaxAgents> shots{};
    const std::size_t shotCount=tacticalAI_.fireAuthorizedShots(freeSlots,shots);
    for(std::size_t i=0;i<shotCount;++i){
        auto& shot=shots[i];
        const auto *source=combatants_.recordById(shot.sourceCombatantId);
        if(source==nullptr||source->identity.role!=CombatantRole::AI||!source->alive||!source->combatCapable) return false;
        shot.sourceTeamId=source->identity.teamId;
        shot.sourceFactionId=source->identity.factionId;
        shot.targetingPolicy=TargetingPolicy::HostileOnly;
        shot.includePlayerTarget=true;
        if(!ballistics_.spawn(shot)) return false;
        audioFX_.observeShot(shot.origin,shot.correlationId,world_);
        ++aiShotsFired_;
    }
    return true;
}

void EngineCore::fixedStep() noexcept {
    const EngineSnapshot stateCheckpoint=state_;
    const CharacterMotor characterCheckpoint=character_;
    const WeaponCore weaponCheckpoint=weapon_;
    const BallisticsCore ballisticsCheckpoint=ballistics_;
    const DamageCore damageCheckpoint=damage_;
    const VisibilityCore visibilityCheckpoint=visibility_;
    const TacticalAICore aiCheckpoint=tacticalAI_;
    const CombatantCore combatantsCheckpoint=combatants_;
    const AudioFXCore audioFXCheckpoint=audioFX_;
    const auto contactsCheckpoint=sessionCollisionContacts_;
    const auto frameContactsCheckpoint=frameCollisionContacts_;
    const auto damageSequenceCheckpoint=consumedDamageResultSequence_;
    const auto aiShotsCheckpoint=aiShotsFired_;
    const auto aiTargetImpactsCheckpoint=aiTargetImpacts_;

    const Vec3 previousPosition{character_.state().x,character_.state().y,character_.state().z};
    const double previousX=character_.state().x;
    const double previousZ=character_.state().z;
    const bool playerCombatCapable=DamageCore::combatCapable(damage_.playerTarget());
    const CharacterInput input=playerCombatCapable?CharacterInput{moveForward_,moveStrafe_,sprintHeld_}:CharacterInput{0.0,0.0,false};
    character_.fixedStep(config_.fixedStepSeconds,input);

    const auto collision=world_.resolve(previousX,previousZ,character_.state().x,character_.state().z,
                                        character_.config().capsuleRadius,character_.capsuleHeight());
    character_.applyHorizontalCollision(collision.x,collision.z,collision.hitX,collision.hitZ);
    frameCollisionContacts_+=collision.contacts;
    sessionCollisionContacts_+=collision.contacts;

    audioFX_.fixedStep(config_.fixedStepSeconds);
    const auto& acceptedCharacterState=character_.state();
    (void)damage_.syncPlayerTargetPosition(CombatantCore::kPlayerId,
                                           {acceptedCharacterState.x,acceptedCharacterState.y,acceptedCharacterState.z});
    audioFX_.observeMovement(previousPosition,
                             {acceptedCharacterState.x,acceptedCharacterState.y,acceptedCharacterState.z},
                             character_.horizontalSpeed(),acceptedCharacterState.gait,
                             acceptedCharacterState.grounded,world_);

    updateWeaponObstruction();
    const bool wasReloading=weapon_.state().reloading;
    weapon_.fixedStep(config_.fixedStepSeconds,character_.horizontalSpeed(),moveStrafe_,character_.state().sprinting);
    const bool reloadCompleted=wasReloading && !weapon_.state().reloading;

    // Segment observation consumes projectile provenance. Player shots remain
    // non-hostile to the player listener; AI shots derive hostile status from their
    // CombatantCore identity in the bounded observer callback.
    AudioProjectileObserverContext projectileAudio{&audioFX_,cameraPosition(),&tacticalAI_,&combatants_,&world_};
    // Tactical locomotion is advanced before projectile tracing. Its accepted position
    // is mirrored one-way into DamageCore, keeping ballistic target truth aligned with
    // the simulation-owned AI position without introducing a second movement owner.
    stepTacticalAI();
    const bool aiShotMutationValid=spawnAuthorizedAIShots();
    ballistics_.fixedStep(config_.fixedStepSeconds,world_,damage_,
                          &observeProjectileForAudio,&projectileAudio);
    damage_.fixedStep(config_.fixedStepSeconds);
    // A same-slice injury can immediately remove an AI combatant. Clear action/fire
    // authorization before invariant validation and before the snapshot is published.
    syncTacticalAICombatState();

    const auto pendingDamageSequence=damage_.resultSequence();
    std::uint64_t playerHitsThisSlice=0;
    bool damageLedgerValid=pendingDamageSequence>=damageSequenceCheckpoint &&
                           pendingDamageSequence-damageSequenceCheckpoint<=DamageCore::kResultCapacity;
    if(damageLedgerValid){
        for(std::uint64_t sequence=damageSequenceCheckpoint+1;sequence<=pendingDamageSequence;++sequence){
            DamageResult result{};
            if(!damage_.resultBySequence(sequence,result)){ damageLedgerValid=false; break; }
            if(!tacticalAI_.observeDamageResult(result,combatants_,world_)){ damageLedgerValid=false; break; }
            if(result.hit&&result.targetId==damage_.playerTarget().id) ++playerHitsThisSlice;
        }
    }

    for(std::size_t i=0;i<damage_.targetCount();++i){
        const auto& target=damage_.targets()[i];
        visibility_.syncTarget(i,target.id,target.position,target.alive);
    }
    visibility_.update(cameraPosition(),character_.cameraYaw(),world_);

    aiTargetImpacts_+=playerHitsThisSlice;
    state_.simulationSeconds+=config_.fixedStepSeconds;
    ++state_.simulationTick;
    syncSnapshot();

#ifdef METSE_TESTING
    if(failNextSimulationSlice_){
        failNextSimulationSlice_=false;
        damageLedgerValid=false;
    }
#endif
    if(!aiShotMutationValid || !damageLedgerValid || !validateInvariants()){
        state_=stateCheckpoint;
        character_=characterCheckpoint;
        weapon_=weaponCheckpoint;
        ballistics_=ballisticsCheckpoint;
        damage_=damageCheckpoint;
        visibility_=visibilityCheckpoint;
        tacticalAI_=aiCheckpoint;
        combatants_=combatantsCheckpoint;
        audioFX_=audioFXCheckpoint;
        sessionCollisionContacts_=contactsCheckpoint;
        frameCollisionContacts_=frameContactsCheckpoint;
        consumedDamageResultSequence_=damageSequenceCheckpoint;
        aiShotsFired_=aiShotsCheckpoint;
        aiTargetImpacts_=aiTargetImpactsCheckpoint;
        ++simulationInvariantRollbacks_;
        integrity_.appendSystemEvent(EventKind::SimulationInvariantRolledBack,state_.simulationTick);
        return;
    }

    // State and the damage-result ledger are validated before publishing any domain
    // events. Multiple hits in one fixed slice therefore remain All-or-Nothing and
    // each keeps its original correlation id.
    if(reloadCompleted) integrity_.appendSystemEvent(EventKind::ReloadCompleted,state_.simulationTick);
    for(std::uint64_t sequence=damageSequenceCheckpoint+1;sequence<=pendingDamageSequence;++sequence){
        DamageResult result{};
        if(!damage_.resultBySequence(sequence,result)) continue; // guarded above
        if(result.hit || result.damage>0.0)
            integrity_.appendCorrelatedSystemEvent(EventKind::DamageApplied,result.correlationId,state_.simulationTick);
        if(result.incapacitated)
            integrity_.appendCorrelatedSystemEvent(EventKind::TargetIncapacitated,result.correlationId,state_.simulationTick);
        if(result.killed)
            integrity_.appendCorrelatedSystemEvent(EventKind::TargetKilled,result.correlationId,state_.simulationTick);
    }
    consumedDamageResultSequence_=pendingDamageSequence;
}

void EngineCore::syncSnapshot() noexcept {
    const auto& characterState=character_.state();
    const auto& weaponState=weapon_.state();
    state_.playerX=characterState.x;
    state_.playerY=characterState.y;
    state_.playerZ=characterState.z;
    state_.velocityX=characterState.velocityX;
    state_.velocityY=characterState.velocityY;
    state_.velocityZ=characterState.velocityZ;
    state_.playerBodyYaw=characterState.bodyYaw;
    state_.playerYaw=character_.cameraYaw();
    state_.playerPitch=characterState.pitch;
    state_.cameraHeight=character_.cameraHeight();
    state_.cameraRoll=characterState.cameraRoll;
    state_.cameraLean=characterState.cameraLean;
    state_.horizontalSpeed=character_.horizontalSpeed();
    state_.stance=characterState.stance;
    state_.gait=characterState.gait;
    state_.grounded=characterState.grounded;
    state_.sprinting=characterState.sprinting;
    state_.collisionContacts=sessionCollisionContacts_;
    state_.ammoInMagazine=weaponState.ammoInMagazine;
    state_.reserveAmmo=weaponState.reserveAmmo;
    state_.adsAlpha=weaponState.adsAlpha;
    state_.reloadRemaining=weaponState.reloadRemaining;
    state_.sprintRecoveryRemaining=weaponState.sprintRecoveryRemaining;
    state_.reloadKind=weaponState.reloadKind;
    state_.reloading=weaponState.reloading;
    state_.weaponObstructed=weaponState.obstructed;
    state_.recoilPitch=weaponState.recoilPitch;
    state_.recoilYaw=weaponState.recoilYaw;
    state_.weaponSwayX=weaponState.swayX;
    state_.weaponSwayY=weaponState.swayY;
    state_.activeProjectiles=static_cast<std::uint32_t>(ballistics_.activeCount());
    state_.damageHits=damage_.totalHits();
    state_.damageKills=damage_.totalKills();
    state_.damageIncapacitations=damage_.totalIncapacitations();
    const auto& playerTarget=damage_.playerTarget();
    state_.playerHealth=playerTarget.health;
    state_.playerBleedingPerSecond=playerTarget.bleedingPerSecond;
    state_.playerCombatState=playerTarget.combatState;
    state_.aiShotsFired=aiShotsFired_;
    state_.aiTargetImpacts=aiTargetImpacts_;
    state_.friendlyFireDenials=damage_.friendlyFireDenials();
    state_.combatantCount=static_cast<std::uint32_t>(combatants_.count());
    state_.visibility=visibility_.report();
    state_.tacticalAI=tacticalAI_.report();
    state_.audioFX=audioFX_.report();
    if(damage_.targetCount()>0){
        const auto& target=damage_.targets()[0];
        state_.primaryTargetHealth=target.health;
        state_.primaryTargetBleedingPerSecond=target.bleedingPerSecond;
        state_.primaryTargetHelmetArmorJoules=target.helmetArmorJoules;
        state_.primaryTargetTorsoArmorJoules=target.torsoArmorJoules;
        state_.primaryTargetCombatState=target.combatState;
    } else {
        state_.primaryTargetHealth=0.0;
        state_.primaryTargetBleedingPerSecond=0.0;
        state_.primaryTargetHelmetArmorJoules=0.0;
        state_.primaryTargetTorsoArmorJoules=0.0;
        state_.primaryTargetCombatState=CombatState::Dead;
    }
}

bool EngineCore::validateInvariants() const noexcept {
    if(!std::isfinite(config_.fixedStepSeconds) || config_.fixedStepSeconds<=0.0 || config_.maxCatchUpSteps<1 || config_.maxCatchUpSteps>8 ||
       config_.maxCombatants<1 || config_.maxCombatants>32 || state_.activeCombatants>config_.maxCombatants ||
       !std::isfinite(state_.simulationSeconds) || state_.simulationSeconds<0.0 || !std::isfinite(state_.interpolationAlpha) ||
       state_.interpolationAlpha<0.0 || state_.interpolationAlpha>1.0 || !std::isfinite(moveForward_) || !std::isfinite(moveStrafe_) ||
       std::hypot(moveForward_,moveStrafe_)>1.000001) return false;
    if(!character_.validate() || !weapon_.validate() || !world_.validate() || !ballistics_.validate() || !damage_.validate() ||
       !visibility_.validate() || !tacticalAI_.validate() || !audioFX_.validate() || !inputQueue_.validate() || !combatants_.validate()) return false;
    if(consumedDamageResultSequence_>damage_.resultSequence() || damage_.resultSequence()-consumedDamageResultSequence_>DamageCore::kResultCapacity) return false;

    const double clearance=world_.clearanceHeightAt(character_.state().x,character_.state().z,character_.config().capsuleRadius);
    if(std::isfinite(clearance) && character_.capsuleHeight()>clearance-0.005) return false;

    const auto& characterState=character_.state();
    const auto& weaponState=weapon_.state();
    const auto ai=tacticalAI_.report();
    const auto audio=audioFX_.report();
    const auto visibility=visibility_.report();
    if(!eq(state_.playerX,characterState.x) || !eq(state_.playerY,characterState.y) || !eq(state_.playerZ,characterState.z) ||
       !eq(state_.playerYaw,character_.cameraYaw()) || !eq(state_.cameraHeight,character_.cameraHeight()) || state_.stance!=characterState.stance ||
       state_.gait!=characterState.gait || state_.grounded!=characterState.grounded || state_.ammoInMagazine!=weaponState.ammoInMagazine ||
       state_.reserveAmmo!=weaponState.reserveAmmo || !eq(state_.adsAlpha,weaponState.adsAlpha) ||
       !eq(state_.sprintRecoveryRemaining,weaponState.sprintRecoveryRemaining) || state_.reloadKind!=weaponState.reloadKind ||
       state_.activeProjectiles!=ballistics_.activeCount() || state_.damageHits!=damage_.totalHits() || state_.damageKills!=damage_.totalKills() ||
       state_.damageIncapacitations!=damage_.totalIncapacitations() || state_.tacticalAI.activeAgents!=ai.activeAgents ||
       state_.tacticalAI.engagedAgents!=ai.engagedAgents || state_.tacticalAI.decisionsExecuted!=ai.decisionsExecuted ||
       state_.tacticalAI.suppressedAgents!=ai.suppressedAgents ||
       state_.tacticalAI.suppressionChecksThisStep!=ai.suppressionChecksThisStep ||
       state_.tacticalAI.suppressionObservations!=ai.suppressionObservations ||
       state_.tacticalAI.suppressionBudgetDrops!=ai.suppressionBudgetDrops ||
       ai.lastObservedDamageSequence!=damage_.resultSequence() ||
       state_.tacticalAI.recoveringAgents!=ai.recoveringAgents ||
       state_.tacticalAI.concernedAgents!=ai.concernedAgents ||
       state_.tacticalAI.lossChecksThisStep!=ai.lossChecksThisStep ||
       state_.tacticalAI.injuryReactions!=ai.injuryReactions ||
       state_.tacticalAI.witnessedLosses!=ai.witnessedLosses ||
       state_.tacticalAI.lossBudgetDrops!=ai.lossBudgetDrops ||
       state_.visibility.full!=visibility.full || state_.visibility.reduced!=visibility.reduced ||
       state_.visibility.minimal!=visibility.minimal || state_.visibility.dormant!=visibility.dormant ||
       state_.visibility.evaluated!=visibility.evaluated || state_.visibility.occluded!=visibility.occluded ||
       state_.visibility.budgetDemotions!=visibility.budgetDemotions ||
       state_.audioFX.cuesEmitted!=audio.cuesEmitted || state_.audioFX.footsteps!=audio.footsteps ||
       state_.audioFX.outdoorShots!=audio.outdoorShots || state_.audioFX.indoorShots!=audio.indoorShots ||
       state_.audioFX.bulletCracks!=audio.bulletCracks || state_.audioFX.nearMisses!=audio.nearMisses ||
       state_.audioFX.fxSpawnRequests!=audio.fxSpawnRequests || state_.audioFX.fxSpawned!=audio.fxSpawned ||
       state_.audioFX.fxDropped!=audio.fxDropped || state_.audioFX.activeFX!=audio.activeFX ||
       state_.audioFX.retainedCues!=audio.retainedCues) return false;

    if(state_.activeCombatants!=combatants_.count()||state_.combatantCount!=combatants_.count()||combatants_.count()!=damage_.targetCount()+1u||
       !damage_.playerTargetEnabled()) return false;
    const auto& playerTarget=damage_.playerTarget();
    const auto *playerRecord=combatants_.recordById(CombatantCore::kPlayerId);
    if(playerRecord==nullptr||playerRecord->identity.role!=CombatantRole::Player||
       playerTarget.id!=playerRecord->identity.id||playerTarget.teamId!=playerRecord->identity.teamId||
       playerTarget.factionId!=playerRecord->identity.factionId||playerTarget.alive!=playerRecord->alive||
       DamageCore::combatCapable(playerTarget)!=playerRecord->combatCapable||
       playerRecord->lifecycle!=lifecycleFor(playerTarget.combatState)||
       !eq(state_.playerHealth,playerTarget.health)||!eq(state_.playerBleedingPerSecond,playerTarget.bleedingPerSecond)||
       state_.playerCombatState!=playerTarget.combatState) return false;
    const std::size_t synchronizedCount=std::min(damage_.targetCount(),tacticalAI_.agentCount());
    for(std::size_t i=0;i<synchronizedCount;++i){
        const auto& target=damage_.targets()[i];
        const auto& agent=tacticalAI_.agents()[i];
        const auto *record=combatants_.recordById(target.id);
        if(target.id!=agent.id || !eq(target.position.x,agent.position.x) || !eq(target.position.y,agent.position.y) || !eq(target.position.z,agent.position.z) ||
           target.alive!=agent.alive || DamageCore::combatCapable(target)!=agent.combatCapable || !eq(std::clamp(target.health/100.0,0.0,1.0),agent.health01) ||
           record==nullptr||record->identity.teamId!=target.teamId||record->identity.factionId!=target.factionId||
           record->alive!=target.alive||record->combatCapable!=DamageCore::combatCapable(target)||
           record->lifecycle!=lifecycleFor(target.combatState)) return false;
    }

    if(damage_.targetCount()>0){
        const auto& target=damage_.targets()[0];
        if(!eq(state_.primaryTargetHealth,target.health) || !eq(state_.primaryTargetBleedingPerSecond,target.bleedingPerSecond) ||
           !eq(state_.primaryTargetHelmetArmorJoules,target.helmetArmorJoules) || !eq(state_.primaryTargetTorsoArmorJoules,target.torsoArmorJoules) ||
           state_.primaryTargetCombatState!=target.combatState) return false;
    }
    return true;
}

void EngineCore::recordBlackBox(double dt,std::uint32_t steps,bool clamped,double sliceMilliseconds) noexcept {
    BlackBoxFrame frame{};
    frame.simulationTick=state_.simulationTick;
    frame.realDeltaSeconds=dt;
    frame.playerX=state_.playerX;
    frame.playerY=state_.playerY;
    frame.playerZ=state_.playerZ;
    frame.horizontalSpeed=state_.horizontalSpeed;
    frame.adsAlpha=state_.adsAlpha;
    frame.primaryTargetHealth=state_.primaryTargetHealth;
    frame.primaryTargetBleedingPerSecond=state_.primaryTargetBleedingPerSecond;
    frame.queueDepth=static_cast<std::uint32_t>(inputQueue_.size());
    frame.activeProjectiles=state_.activeProjectiles;
    frame.catchUpSteps=steps;
    frame.aiEngaged=state_.tacticalAI.engagedAgents;
    frame.shotsFired=state_.shotsFired;
    frame.damageHits=state_.damageHits;
    frame.damageIncapacitations=state_.damageIncapacitations;
    frame.stance=state_.stance;
    frame.gait=state_.gait;
    frame.primaryTargetCombatState=state_.primaryTargetCombatState;
    frame.grounded=state_.grounded;
    frame.sprinting=state_.sprinting;
    frame.reloading=state_.reloading;
    frame.catchUpClamped=clamped;
    frame.simulationSliceMilliseconds=sliceMilliseconds;
    frame.projectileContacts=ballistics_.metrics().worldImpacts+ballistics_.metrics().targetImpacts;
    frame.projectileTerminalContacts=ballistics_.metrics().terminalWorldImpacts;
    frame.projectileTargetContacts=ballistics_.metrics().targetImpacts;
    frame.aiActiveAgents=state_.tacticalAI.activeAgents;
    frame.aiLOSAgents=state_.tacticalAI.lineOfSightAgents;
    frame.aiDecisions=static_cast<std::uint32_t>(std::min<std::uint64_t>(state_.tacticalAI.decisionsExecuted,0xFFFFFFFFull));
    // The expected 30 FPS thermal fallback naturally produces ~33.33 ms render deltas and
    // two fixed simulation steps. Treat only a materially slower callback as a
    // pre-spike signal; catch-up clamp and slow fixed slices remain authoritative.
    const double preSpikeDeltaThreshold=config_.fixedStepSeconds*3.0;
    std::uint8_t reasonMask=0;
    if(dt>preSpikeDeltaThreshold) reasonMask|=kPreSpikeCallbackDelta;
    if(clamped) reasonMask|=kPreSpikeCatchUpClamp;
    if(sliceMilliseconds>20.0) reasonMask|=kPreSpikeSimulationSlice;
    frame.preSpikeReasonMask=reasonMask;
    frame.preSpike=reasonMask!=0;
    if (frame.preSpike) ++preSpikeBlackBoxFrames_;
    if ((reasonMask&kPreSpikeCallbackDelta)!=0) ++preSpikeCallbackFrames_;
    if ((reasonMask&kPreSpikeCatchUpClamp)!=0) ++preSpikeCatchUpFrames_;
    if ((reasonMask&kPreSpikeSimulationSlice)!=0) ++preSpikeSimulationFrames_;
    blackBox_[blackBoxWrite_]=frame;
    blackBoxWrite_=(blackBoxWrite_+1)%kBlackBoxCapacity;
    blackBoxCount_=std::min(blackBoxCount_+1,kBlackBoxCapacity);
}

void EngineCore::observeFrame(double dt,std::uint32_t steps,bool clamped,double sliceMilliseconds) noexcept {
    ObservatoryFrameInput input{};
    input.simulationTick=state_.simulationTick;
    input.realDeltaSeconds=dt;
    input.simulationSliceMilliseconds=sliceMilliseconds;
    input.playerX=state_.playerX;
    input.playerZ=state_.playerZ;
    input.horizontalSpeed=state_.horizontalSpeed;
    input.stance=state_.stance;
    input.gait=state_.gait;
    input.grounded=state_.grounded;
    input.sprinting=state_.sprinting;
    input.catchUpSteps=steps;
    input.catchUpClamped=clamped;
    input.collisionContacts=frameCollisionContacts_;
    input.inputQueueDepth=static_cast<std::uint32_t>(inputQueue_.size());
    input.activeProjectiles=state_.activeProjectiles;
    input.visibilityFull=state_.visibility.full;
    input.visibilityReduced=state_.visibility.reduced;
    input.visibilityMinimal=state_.visibility.minimal;
    input.visibilityDormant=state_.visibility.dormant;
    input.aiActiveAgents=state_.tacticalAI.activeAgents;
    input.aiLOSAgents=state_.tacticalAI.lineOfSightAgents;
    input.aiDecisions=static_cast<std::uint32_t>(std::min<std::uint64_t>(state_.tacticalAI.decisionsExecuted,0xFFFFFFFFull));
    const auto metrics=ballistics_.metrics();
    const std::uint64_t projectileContacts=metrics.worldImpacts+metrics.targetImpacts;
    input.projectileContacts=projectileContacts>=observedProjectileContacts_ ? projectileContacts-observedProjectileContacts_ : projectileContacts;
    input.projectileTerminalContacts=metrics.terminalWorldImpacts>=observedProjectileTerminalContacts_ ? metrics.terminalWorldImpacts-observedProjectileTerminalContacts_ : metrics.terminalWorldImpacts;
    input.projectileTargetContacts=metrics.targetImpacts>=observedProjectileTargetContacts_ ? metrics.targetImpacts-observedProjectileTargetContacts_ : metrics.targetImpacts;
    observedProjectileContacts_=projectileContacts;
    observedProjectileTerminalContacts_=metrics.terminalWorldImpacts;
    observedProjectileTargetContacts_=metrics.targetImpacts;
    input.ammoInMagazine=state_.ammoInMagazine;
    input.reserveAmmo=state_.reserveAmmo;
    input.adsAlpha=state_.adsAlpha;
    input.reloading=state_.reloading;
    input.weaponObstructed=state_.weaponObstructed;
    observatory_.observe(input);
}

Sha256Digest EngineCore::deterministicStateHash() const noexcept {
    // The hash buffer is fixed and deliberately oversized for the complete bounded
    // 32-agent tactical state. It performs no allocation and makes the deterministic
    // regression sensitive to action/memory/locomotion drift, not just player state.
    // Conservative word budgets per bounded pool, including maximum projectile
    // occupancy. The old 16 KiB capacity silently omitted the tail at saturation.
    constexpr std::size_t hashWords=128+4*VisibilityCore::kMaxEntities+
        16*DamageCore::kMaxTargets+20*BallisticsCore::kMaxProjectiles+
        64*TacticalAICore::kMaxAgents+8*CombatantCore::kMaxCombatants;
    std::array<std::uint8_t,hashWords*8> buffer{};
    std::size_t cursor=0;
    put64(buffer,cursor,state_.simulationTick);
    putD(buffer,cursor,state_.simulationSeconds);
    putD(buffer,cursor,state_.playerX); putD(buffer,cursor,state_.playerY); putD(buffer,cursor,state_.playerZ);
    putD(buffer,cursor,state_.velocityX); putD(buffer,cursor,state_.velocityY); putD(buffer,cursor,state_.velocityZ);
    putD(buffer,cursor,state_.playerYaw); putD(buffer,cursor,state_.playerPitch);
    put64(buffer,cursor,state_.shotsFired);
    put64(buffer,cursor,state_.ammoInMagazine); put64(buffer,cursor,state_.reserveAmmo);
    putD(buffer,cursor,state_.adsAlpha); putD(buffer,cursor,state_.sprintRecoveryRemaining);
    put64(buffer,cursor,state_.damageHits); put64(buffer,cursor,state_.damageKills); put64(buffer,cursor,state_.damageIncapacitations);
    putD(buffer,cursor,state_.playerHealth); putD(buffer,cursor,state_.playerBleedingPerSecond);
    put64(buffer,cursor,static_cast<std::uint64_t>(state_.playerCombatState));
    put64(buffer,cursor,state_.aiShotsFired); put64(buffer,cursor,state_.aiTargetImpacts);
    put64(buffer,cursor,state_.friendlyFireDenials); put64(buffer,cursor,combatants_.count());
    put64(buffer,cursor,state_.activeProjectiles); put64(buffer,cursor,state_.tacticalAI.engagedAgents);
    put64(buffer,cursor,state_.tacticalAI.decisionsExecuted);
    put64(buffer,cursor,state_.visibility.full); put64(buffer,cursor,state_.visibility.reduced);
    put64(buffer,cursor,state_.visibility.minimal); put64(buffer,cursor,state_.visibility.dormant);
    put64(buffer,cursor,state_.visibility.evaluated); put64(buffer,cursor,state_.visibility.occluded);
    put64(buffer,cursor,state_.visibility.budgetDemotions);
    put64(buffer,cursor,audioFX_.deterministicFingerprint());
    put64(buffer,cursor,damage_.resultSequence());
    for(std::size_t i=0;i<visibility_.count();++i){
        const auto& entity=visibility_.entities()[i];
        put64(buffer,cursor,entity.id);
        put64(buffer,cursor,static_cast<std::uint64_t>(entity.tier));
        put64(buffer,cursor,entity.alive?1u:0u);
        put64(buffer,cursor,entity.lineOfSight?1u:0u);
    }
    for(std::size_t i=0;i<damage_.targetCount();++i){
        const auto& target=damage_.targets()[i];
        put64(buffer,cursor,target.id);
        put64(buffer,cursor,target.teamId); put64(buffer,cursor,target.factionId);
        put64(buffer,cursor,static_cast<std::uint64_t>(target.role));
        putD(buffer,cursor,target.position.x); putD(buffer,cursor,target.position.y); putD(buffer,cursor,target.position.z);
        putD(buffer,cursor,target.health);
        putD(buffer,cursor,target.helmetArmorJoules);
        putD(buffer,cursor,target.torsoArmorJoules);
        putD(buffer,cursor,target.bleedingPerSecond);
        put64(buffer,cursor,target.lastDamageCorrelationId);
        put64(buffer,cursor,static_cast<std::uint64_t>(target.combatState));
        put64(buffer,cursor,target.alive?1u:0u);
    }
    for(const auto& projectile:ballistics_.projectiles()){
        if(!projectile.active) continue;
        put64(buffer,cursor,projectile.correlationId);
        put64(buffer,cursor,projectile.sourceCombatantId);
        put64(buffer,cursor,projectile.sourceTeamId); put64(buffer,cursor,projectile.sourceFactionId);
        put64(buffer,cursor,static_cast<std::uint64_t>(projectile.targetingPolicy));
        put64(buffer,cursor,projectile.includePlayerTarget?1u:0u);
        putD(buffer,cursor,projectile.position.x); putD(buffer,cursor,projectile.position.y); putD(buffer,cursor,projectile.position.z);
        putD(buffer,cursor,projectile.velocity.x); putD(buffer,cursor,projectile.velocity.y); putD(buffer,cursor,projectile.velocity.z);
        putD(buffer,cursor,projectile.massKg); putD(buffer,cursor,projectile.ageSeconds);
        put64(buffer,cursor,projectile.penetrations); put64(buffer,cursor,projectile.ricochets);
    }
    put64(buffer,cursor,tacticalAI_.agentCount());
    const auto suppressionReport=tacticalAI_.report();
    put64(buffer,cursor,suppressionReport.suppressionChecksThisStep);
    put64(buffer,cursor,suppressionReport.suppressionObservations);
    put64(buffer,cursor,suppressionReport.suppressionBudgetDrops);
    put64(buffer,cursor,suppressionReport.lossChecksThisStep);
    put64(buffer,cursor,suppressionReport.injuryReactions);
    put64(buffer,cursor,suppressionReport.witnessedLosses);
    put64(buffer,cursor,suppressionReport.lossBudgetDrops);
    put64(buffer,cursor,suppressionReport.lastObservedDamageSequence);
    for(std::size_t i=0;i<tacticalAI_.agentCount();++i){
        const auto& agent=tacticalAI_.agents()[i];
        put64(buffer,cursor,agent.id);
        putD(buffer,cursor,agent.position.x); putD(buffer,cursor,agent.position.y); putD(buffer,cursor,agent.position.z);
        putD(buffer,cursor,agent.lastKnownPlayerPosition.x); putD(buffer,cursor,agent.lastKnownPlayerPosition.y); putD(buffer,cursor,agent.lastKnownPlayerPosition.z);
        putD(buffer,cursor,agent.actionTarget.x); putD(buffer,cursor,agent.actionTarget.y); putD(buffer,cursor,agent.actionTarget.z);
        putD(buffer,cursor,agent.coverPosition.x); putD(buffer,cursor,agent.coverPosition.y); putD(buffer,cursor,agent.coverPosition.z);
        putD(buffer,cursor,agent.peekPosition.x); putD(buffer,cursor,agent.peekPosition.y); putD(buffer,cursor,agent.peekPosition.z);
        putD(buffer,cursor,agent.facingYaw);
        putD(buffer,cursor,agent.memoryAgeSeconds);
        putD(buffer,cursor,agent.decisionAgeSeconds);
        putD(buffer,cursor,agent.actionAgeSeconds);
        putD(buffer,cursor,agent.confidence);
        putD(buffer,cursor,agent.threat);
        putD(buffer,cursor,agent.health01);
        putD(buffer,cursor,agent.suppression01);
        put64(buffer,cursor,agent.lastSuppressionCorrelationId);
        putD(buffer,cursor,agent.injuryRecoveryRemaining);
        putD(buffer,cursor,agent.teamLossConcernRemaining);
        put64(buffer,cursor,agent.lastInjuryCorrelationId);
        put64(buffer,cursor,agent.lastWitnessedLossId);
        put64(buffer,cursor,agent.lastWitnessedLossCorrelationId);
        put64(buffer,cursor,agent.actionSequence);
        put64(buffer,cursor,agent.squadSourceAgentId);
        put64(buffer,cursor,agent.coverCandidateIndex);
        put64(buffer,cursor,static_cast<std::uint64_t>(agent.alert));
        put64(buffer,cursor,static_cast<std::uint64_t>(agent.perceptionSource));
        put64(buffer,cursor,static_cast<std::uint64_t>(agent.action));
        put64(buffer,cursor,agent.alive?1u:0u);
        put64(buffer,cursor,agent.combatCapable?1u:0u);
        put64(buffer,cursor,agent.hasLineOfSight?1u:0u);
        put64(buffer,cursor,agent.heardPlayer?1u:0u);
        put64(buffer,cursor,agent.hasCover?1u:0u);
        put64(buffer,cursor,agent.fireAuthorized?1u:0u);
        const WeaponState* aiWeapon=tacticalAI_.agentWeaponState(i);
        if(aiWeapon!=nullptr){
            put64(buffer,cursor,aiWeapon->ammoInMagazine);
            put64(buffer,cursor,aiWeapon->reserveAmmo);
            put64(buffer,cursor,aiWeapon->shotSequence);
            putD(buffer,cursor,aiWeapon->fireCooldown);
            putD(buffer,cursor,aiWeapon->reloadRemaining);
            put64(buffer,cursor,aiWeapon->reloading?1u:0u);
            put64(buffer,cursor,static_cast<std::uint64_t>(aiWeapon->reloadKind));
        }
    }
    const auto& playerTarget=damage_.playerTarget();
    put64(buffer,cursor,playerTarget.id);
    put64(buffer,cursor,playerTarget.teamId); put64(buffer,cursor,playerTarget.factionId);
    putD(buffer,cursor,playerTarget.position.x); putD(buffer,cursor,playerTarget.position.y); putD(buffer,cursor,playerTarget.position.z);
    putD(buffer,cursor,playerTarget.health); putD(buffer,cursor,playerTarget.bleedingPerSecond);
    putD(buffer,cursor,playerTarget.helmetArmorJoules); putD(buffer,cursor,playerTarget.torsoArmorJoules);
    put64(buffer,cursor,playerTarget.lastDamageCorrelationId);
    put64(buffer,cursor,static_cast<std::uint64_t>(playerTarget.combatState));
    put64(buffer,cursor,playerTarget.alive?1u:0u);
    for(std::size_t i=0;i<combatants_.count();++i){
        const auto& combatant=combatants_.record(i);
        put64(buffer,cursor,combatant.identity.id);
        put64(buffer,cursor,combatant.identity.teamId); put64(buffer,cursor,combatant.identity.factionId);
        put64(buffer,cursor,static_cast<std::uint64_t>(combatant.identity.role));
        put64(buffer,cursor,combatant.alive?1u:0u); put64(buffer,cursor,combatant.combatCapable?1u:0u);
        put64(buffer,cursor,combatant.targetable?1u:0u);
        put64(buffer,cursor,static_cast<std::uint64_t>(combatant.lifecycle));
    }
    return sha256(std::span<const std::uint8_t>(buffer.data(),cursor));
}

EngineDiagnostics EngineCore::diagnostics() const noexcept {
    EngineDiagnostics diagnostics{};
    diagnostics.integrity=integrity_.metrics();
    diagnostics.journalHead=integrity_.journalHead();
    diagnostics.stateHash=deterministicStateHash();
    diagnostics.observatory=observatory_.report();
    diagnostics.inputQueue=inputQueue_.metrics();
    diagnostics.ballistics=ballistics_.metrics();
    diagnostics.visibility=visibility_.report();
    diagnostics.tacticalAI=tacticalAI_.report();
    diagnostics.audioFX=audioFX_.report();
    diagnostics.gameplayDenials=gameplayDenials_;
    diagnostics.retainedEvents=integrity_.eventCount();
    diagnostics.retainedCommands=integrity_.commandCount();
    diagnostics.retainedBlackBoxFrames=blackBoxCount_;
    diagnostics.preSpikeBlackBoxFrames=preSpikeBlackBoxFrames_;
    diagnostics.preSpikeCallbackFrames=preSpikeCallbackFrames_;
    diagnostics.preSpikeCatchUpFrames=preSpikeCatchUpFrames_;
    diagnostics.preSpikeSimulationFrames=preSpikeSimulationFrames_;
    for(std::size_t i=0;i<blackBoxCount_;++i){
        const auto& frame=blackBox_[(blackBoxWrite_+kBlackBoxCapacity-blackBoxCount_+i)%kBlackBoxCapacity];
        if((frame.preSpikeReasonMask&kPreSpikeCallbackDelta)!=0) ++diagnostics.retainedPreSpikeCallbackFrames;
        if((frame.preSpikeReasonMask&kPreSpikeCatchUpClamp)!=0) ++diagnostics.retainedPreSpikeCatchUpFrames;
        if((frame.preSpikeReasonMask&kPreSpikeSimulationSlice)!=0) ++diagnostics.retainedPreSpikeSimulationFrames;
    }
    diagnostics.worldObstacleCount=world_.obstacleCount();
    diagnostics.inputQueueDepth=inputQueue_.size();
    diagnostics.sessionCollisionContacts=sessionCollisionContacts_;
    diagnostics.simulationInvariantRollbacks=simulationInvariantRollbacks_;
    diagnostics.damageHits=damage_.totalHits();
    diagnostics.damageKills=damage_.totalKills();
    diagnostics.damageIncapacitations=damage_.totalIncapacitations();
    diagnostics.damageArmorHits=damage_.metrics().armorHits;
    diagnostics.damageBleedTransitions=damage_.metrics().bleedTransitions;
    diagnostics.aiShotsFired=aiShotsFired_;
    diagnostics.aiTargetImpacts=aiTargetImpacts_;
    diagnostics.friendlyFireDenials=damage_.friendlyFireDenials();
    diagnostics.combatants=combatants_;
    diagnostics.combatantLifecycle=combatants_.report();
    diagnostics.journalValid=integrity_.verifyJournal();
    diagnostics.worldValid=world_.validate();
    diagnostics.observatoryValid=observatory_.validate();
    diagnostics.inputQueueValid=inputQueue_.validate();
    diagnostics.weaponValid=weapon_.validate();
    diagnostics.ballisticsValid=ballistics_.validate();
    diagnostics.damageValid=damage_.validate();
    diagnostics.visibilityValid=visibility_.validate();
    diagnostics.tacticalAIValid=tacticalAI_.validate();
    diagnostics.audioFXValid=audioFX_.validate();
    return diagnostics;
}

bool EngineCore::newestBlackBoxFrame(std::size_t offset,BlackBoxFrame& out) const noexcept {
    if(offset>=blackBoxCount_) return false;
    out=blackBox_[(blackBoxWrite_+kBlackBoxCapacity-1-offset)%kBlackBoxCapacity];
    return true;
}

#ifdef METSE_TESTING
bool EngineCore::testOnlyExecuteInvariantViolation() {
    return executeAtomic(CommandKind::AddLookIntent,true,EventKind::LookIntentChanged,[&](std::uint64_t){
        moveForward_=std::numeric_limits<double>::quiet_NaN();
        return true;
    });
}

void EngineCore::testOnlySetAirborne(double height,double velocityY) noexcept {
    character_.testOnlySetAirborne(height,velocityY);
    syncSnapshot();
}
#endif

} // namespace metse
