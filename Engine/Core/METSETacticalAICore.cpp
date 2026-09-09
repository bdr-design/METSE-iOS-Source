#include "METSETacticalAICore.hpp"
#include "METSEWorldCollision.hpp"
#include <algorithm>
#include <cmath>

namespace metse {
namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kAgentRadius = 0.34;
constexpr double kAgentCapsuleHeight = 1.72;
constexpr double kCoverEyeHeight = 1.34;
constexpr double kPlayerChestHeight = 1.15;
}

TacticalAICore::TacticalAICore(TacticalAIConfig config,WeaponConfig weaponConfig) noexcept
    : config_(config), weaponConfig_(weaponConfig) {
    if(!std::isfinite(config_.maxVisionDistanceMeters)||config_.maxVisionDistanceMeters<=1.0) config_.maxVisionDistanceMeters=72.0;
    if(!std::isfinite(config_.horizontalFovRadians)||config_.horizontalFovRadians<=0.1||config_.horizontalFovRadians>=2.0*kPi) config_.horizontalFovRadians=1.91986217719;
    if(!std::isfinite(config_.memorySeconds)||config_.memorySeconds<=0.1) config_.memorySeconds=8.0;
    if(!std::isfinite(config_.hearingBaseMeters)||config_.hearingBaseMeters<0.0) config_.hearingBaseMeters=6.0;
    if(!std::isfinite(config_.hearingMaxMeters)||config_.hearingMaxMeters<config_.hearingBaseMeters) config_.hearingMaxMeters=34.0;
    if(!std::isfinite(config_.hearingMaxLocalizationErrorMeters)||config_.hearingMaxLocalizationErrorMeters<0.0) config_.hearingMaxLocalizationErrorMeters=7.0;
    if(!std::isfinite(config_.agentEyeHeight)||config_.agentEyeHeight<=0.2) config_.agentEyeHeight=1.58;
    config_.suspiciousConfidence=std::clamp(config_.suspiciousConfidence,0.05,0.8);
    config_.engagedConfidence=std::clamp(config_.engagedConfidence,config_.suspiciousConfidence+0.05,1.0);
    if(!std::isfinite(config_.decisionIntervalSeconds)||config_.decisionIntervalSeconds<1.0/60.0||config_.decisionIntervalSeconds>1.0) config_.decisionIntervalSeconds=0.20;
    if(!std::isfinite(config_.moveSpeedMetersPerSecond)||config_.moveSpeedMetersPerSecond<=0.1||config_.moveSpeedMetersPerSecond>8.0) config_.moveSpeedMetersPerSecond=2.65;
    if(!std::isfinite(config_.retreatSpeedMetersPerSecond)||config_.retreatSpeedMetersPerSecond<config_.moveSpeedMetersPerSecond||config_.retreatSpeedMetersPerSecond>9.0) config_.retreatSpeedMetersPerSecond=3.45;
    if(!std::isfinite(config_.coverSearchRadiusMeters)||config_.coverSearchRadiusMeters<2.0||config_.coverSearchRadiusMeters>48.0) config_.coverSearchRadiusMeters=24.0;
    if(!std::isfinite(config_.coverArrivalRadiusMeters)||config_.coverArrivalRadiusMeters<0.1||config_.coverArrivalRadiusMeters>1.5) config_.coverArrivalRadiusMeters=0.48;
    if(!std::isfinite(config_.peekOffsetMeters)||config_.peekOffsetMeters<0.2||config_.peekOffsetMeters>2.0) config_.peekOffsetMeters=0.78;
    if(!std::isfinite(config_.flankOffsetMeters)||config_.flankOffsetMeters<1.0||config_.flankOffsetMeters>12.0) config_.flankOffsetMeters=4.5;
    if(!std::isfinite(config_.retreatHealth01)) config_.retreatHealth01=0.38;
    config_.retreatHealth01=std::clamp(config_.retreatHealth01,0.05,0.75);
    if(!std::isfinite(config_.squadShareRangeMeters)||config_.squadShareRangeMeters<1.0||config_.squadShareRangeMeters>40.0) config_.squadShareRangeMeters=18.0;
    if(!std::isfinite(config_.squadShareFreshSeconds)||config_.squadShareFreshSeconds<=0.0||config_.squadShareFreshSeconds>config_.memorySeconds) config_.squadShareFreshSeconds=1.50;
    if(!std::isfinite(config_.squadShareMaxLocalizationErrorMeters)||config_.squadShareMaxLocalizationErrorMeters<0.0||config_.squadShareMaxLocalizationErrorMeters>8.0) config_.squadShareMaxLocalizationErrorMeters=2.25;

    const WeaponCore canonicalWeapon(weaponConfig_);
    weaponConfig_=canonicalWeapon.config();
    reset();
}

void TacticalAICore::reset() noexcept {
    agents_={};
    agentCount_=0;
    decisionCursor_=0;
    decisionsExecuted_=0;
    for(auto& weapon:weapons_) weapon=WeaponCore(weaponConfig_);
}

bool TacticalAICore::syncAgent(std::size_t index,std::uint32_t id,Vec3 position,double facingYaw,bool alive) noexcept {
    return syncAgent(index,id,position,facingYaw,alive,alive,1.0);
}

bool TacticalAICore::syncAgent(std::size_t index,std::uint32_t id,Vec3 position,double facingYaw,bool alive,bool combatCapable,double health01) noexcept {
    if(index>=kMaxAgents||id==0||!finiteVec(position)||!std::isfinite(facingYaw)||!std::isfinite(health01)) return false;
    auto& agent=agents_[index];
    const bool sameIdentity=agent.id==id&&agent.id!=0;
    if(!sameIdentity){
        agent={};
        agent.id=id;
        weapons_[index]=WeaponCore(weaponConfig_);
        // Stagger first decisions deterministically so 32 agents never all enter the
        // expensive cover-selection path on the same 60 Hz slice.
        agent.decisionAgeSeconds=config_.decisionIntervalSeconds*static_cast<double>(index%kMaxDecisionsPerStep)/static_cast<double>(kMaxDecisionsPerStep);
    }
    agent.position=position;
    agent.facingYaw=wrapAngle(facingYaw);
    agent.alive=alive;
    agent.combatCapable=alive&&combatCapable;
    agent.health01=clamp01(health01);
    if(!agent.combatCapable) clearKnowledgeAndAction(agent);
    agentCount_=std::max(agentCount_,index+1);
    return true;
}

bool TacticalAICore::syncAgentCombatState(std::size_t index,std::uint32_t id,bool alive,bool combatCapable,double health01) noexcept {
    if(index>=agentCount_||id==0||agents_[index].id!=id||!std::isfinite(health01)) return false;
    auto& agent=agents_[index];
    agent.alive=alive;
    agent.combatCapable=alive&&combatCapable;
    agent.health01=clamp01(health01);
    if(!agent.combatCapable) clearKnowledgeAndAction(agent);
    return true;
}

void TacticalAICore::clearKnowledgeAndAction(TacticalAgentState& agent) noexcept {
    agent.lastKnownPlayerPosition={};
    agent.actionTarget={};
    agent.coverPosition={};
    agent.peekPosition={};
    agent.memoryAgeSeconds=0.0;
    agent.actionAgeSeconds=0.0;
    agent.confidence=0.0;
    agent.threat=0.0;
    agent.squadSourceAgentId=0;
    agent.coverCandidateIndex=kNoCoverCandidate;
    agent.alert=AIAlertState::Unaware;
    agent.perceptionSource=AIPerceptionSource::None;
    agent.action=AIActionState::Hold;
    agent.hasLineOfSight=false;
    agent.heardPlayer=false;
    agent.hasCover=false;
    agent.fireAuthorized=false;
}

void TacticalAICore::fixedStep(double dt,
                               const WorldCollisionCore& world,
                               Vec3 playerPosition,
                               Vec3 playerVelocity,
                               double playerNoise01) noexcept {
    if(!std::isfinite(dt)||dt<=0.0||!finiteVec(playerPosition)||!finiteVec(playerVelocity)) return;
    const double noise=clamp01(playerNoise01);
    const double hearingRadius=config_.hearingBaseMeters+(config_.hearingMaxMeters-config_.hearingBaseMeters)*noise;

    for(std::size_t i=0;i<agentCount_;++i){
        auto& agent=agents_[i];
        if(agent.id==0||!agent.combatCapable) continue;
        perceiveAgent(agent,world,playerPosition,playerVelocity,noise,hearingRadius,dt);
        agent.decisionAgeSeconds+=dt;
        agent.actionAgeSeconds+=dt;
    }

    std::size_t scanned=0;
    std::size_t processed=0;
    while(agentCount_>0&&scanned<agentCount_&&processed<kMaxDecisionsPerStep){
        const std::size_t index=(decisionCursor_+scanned)%agentCount_;
        auto& agent=agents_[index];
        if(agent.id!=0&&agent.combatCapable&&agent.decisionAgeSeconds+1e-12>=config_.decisionIntervalSeconds){
            shareKnowledgeForAgent(index);
            decideAgent(index,world);
            agent.decisionAgeSeconds=0.0;
            ++processed;
            ++decisionsExecuted_;
        }
        ++scanned;
    }
    if(agentCount_>0) decisionCursor_=(decisionCursor_+scanned)%agentCount_;

    for(std::size_t i=0;i<agentCount_;++i){
        auto& agent=agents_[i];
        if(agent.id==0||!agent.combatCapable) continue;
        const Vec3 before=agent.position;
        advanceAction(agent,world,dt);
        const double speed=distanceXZ(before,agent.position)/dt;
        weapons_[i].fixedStep(dt,speed,0.0,false);

        if(agent.perceptionSource!=AIPerceptionSource::None){
            const auto block=world.raycastSegment({agent.position.x,agent.position.y+kCoverEyeHeight,agent.position.z},
                                                  {agent.lastKnownPlayerPosition.x,agent.lastKnownPlayerPosition.y+kPlayerChestHeight,agent.lastKnownPlayerPosition.z});
            agent.hasCover=block.hit;
        }else{
            agent.hasCover=false;
        }
        agent.fireAuthorized=authorizeFire(i,world);
    }
}

std::size_t TacticalAICore::fireAuthorizedShots(std::size_t maxShots,
                                                std::array<ShotSolution,kMaxAgents>& out) noexcept {
    const std::size_t boundedMax=std::min(maxShots,kMaxAgents);
    std::size_t emitted=0;
    for(std::size_t i=0;i<agentCount_&&emitted<boundedMax;++i){
        auto& agent=agents_[i];
        auto& weapon=weapons_[i];
        if(!agent.fireAuthorized||!agent.combatCapable||!agent.hasLineOfSight||
           agent.perceptionSource!=AIPerceptionSource::Vision||
           (agent.action!=AIActionState::Peek&&agent.action!=AIActionState::Suppress)) continue;

        const Vec3 firingPosition=agent.action==AIActionState::Peek?agent.peekPosition:agent.position;
        const Vec3 camera{firingPosition.x,firingPosition.y+config_.agentEyeHeight,firingPosition.z};
        const Vec3 target{agent.lastKnownPlayerPosition.x,
                          agent.lastKnownPlayerPosition.y+kPlayerChestHeight,
                          agent.lastKnownPlayerPosition.z};
        const double dx=target.x-camera.x;
        const double dz=target.z-camera.z;
        const double horizontal=std::hypot(dx,dz);
        if(horizontal<=1e-8){ agent.fireAuthorized=false; continue; }
        const double yaw=std::atan2(dx,dz);
        const double pitch=std::atan2(target.y-camera.y,horizontal);
        const std::uint64_t nextShot=weapon.state().shotSequence+1u;
        const std::uint64_t correlation=(static_cast<std::uint64_t>(agent.id)<<32u)|(nextShot&0xFFFFFFFFull);
        ShotSolution shot{};
        if(!weapon.fire(camera,yaw,pitch,correlation==0?1:correlation,shot)){
            agent.fireAuthorized=false;
            continue;
        }
        shot.sourceCombatantId=agent.id;
        out[emitted++]=shot;
        agent.fireAuthorized=false;
    }
    return emitted;
}

void TacticalAICore::perceiveAgent(TacticalAgentState& agent,
                                   const WorldCollisionCore& world,
                                   Vec3 playerPosition,
                                   Vec3 playerVelocity,
                                   double noise,
                                   double hearingRadius,
                                   double dt) noexcept {
    // A peek is a bounded alternate sensor/firing origin derived from the selected
    // cover edge. The current player position is still admitted only through this LOS
    // test; last-known memory is never silently replaced when the peek remains blocked.
    Vec3 perceptionOrigin=agent.position;
    if(agent.action==AIActionState::Peek&&distanceXZ(agent.peekPosition,agent.coverPosition)>0.10)
        perceptionOrigin=agent.peekPosition;

    const double dx=playerPosition.x-perceptionOrigin.x;
    const double dz=playerPosition.z-perceptionOrigin.z;
    const double distance=std::hypot(dx,dz);
    const double targetYaw=std::atan2(dx,dz);
    const double angularError=std::abs(wrapAngle(targetYaw-agent.facingYaw));
    const bool insideFov=angularError<=config_.horizontalFovRadians*0.5;
    const bool insideVisionDistance=distance<=config_.maxVisionDistanceMeters;

    const Vec3 eye{perceptionOrigin.x,perceptionOrigin.y+config_.agentEyeHeight,perceptionOrigin.z};
    const Vec3 playerChest{playerPosition.x,playerPosition.y+kPlayerChestHeight,playerPosition.z};
    const auto worldHit=world.raycastSegment(eye,playerChest);
    const bool clearLine=!worldHit.hit;
    agent.hasLineOfSight=insideFov&&insideVisionDistance&&clearLine;
    agent.heardPlayer=noise>0.001&&distance<=hearingRadius;

    if(agent.hasLineOfSight){
        const double distanceFactor=1.0-clamp01(distance/config_.maxVisionDistanceMeters);
        const double angleFactor=1.0-clamp01(angularError/(config_.horizontalFovRadians*0.5));
        const double visualConfidence=std::clamp(0.58+0.30*distanceFactor+0.12*angleFactor,0.0,1.0);
        agent.confidence=std::max(agent.confidence,visualConfidence);
        agent.lastKnownPlayerPosition=playerPosition;
        agent.perceptionSource=AIPerceptionSource::Vision;
        agent.squadSourceAgentId=0;
        agent.memoryAgeSeconds=0.0;
    }else if(agent.heardPlayer){
        const double hearingFactor=hearingRadius>1e-6?1.0-clamp01(distance/hearingRadius):0.0;
        const double audioConfidence=std::clamp(0.18+0.42*noise+0.20*hearingFactor,0.0,0.78);
        if(agent.perceptionSource!=AIPerceptionSource::Vision||agent.memoryAgeSeconds>0.30||audioConfidence>agent.confidence){
            agent.confidence=std::max(agent.confidence,audioConfidence);
            // Hearing creates a deterministic search estimate, never exact omniscient knowledge.
            agent.lastKnownPlayerPosition=hearingEstimate(agent.id,playerPosition,audioConfidence,config_.hearingMaxLocalizationErrorMeters);
            agent.perceptionSource=AIPerceptionSource::Hearing;
            agent.squadSourceAgentId=0;
            agent.memoryAgeSeconds=0.0;
        }
    }else if(agent.perceptionSource!=AIPerceptionSource::None){
        agent.memoryAgeSeconds+=dt;
        const double decay=dt/config_.memorySeconds;
        agent.confidence=std::max(0.0,agent.confidence-decay);
        if(agent.memoryAgeSeconds>=config_.memorySeconds||agent.confidence<=1e-9){
            agent.confidence=0.0;
            agent.memoryAgeSeconds=std::min(agent.memoryAgeSeconds,config_.memorySeconds);
            agent.perceptionSource=AIPerceptionSource::None;
            agent.squadSourceAgentId=0;
            agent.lastKnownPlayerPosition={};
        }
    }

    double knowledgeDistance=config_.maxVisionDistanceMeters;
    if(agent.perceptionSource!=AIPerceptionSource::None) knowledgeDistance=distanceXZ(agent.position,agent.lastKnownPlayerPosition);
    const double proximity=1.0-clamp01(knowledgeDistance/config_.maxVisionDistanceMeters);
    const double playerSpeed=std::hypot(playerVelocity.x,playerVelocity.z);
    const double observedMovement=agent.hasLineOfSight?clamp01(playerSpeed/6.0):(agent.heardPlayer?noise:0.0);
    agent.threat=clamp01(agent.confidence*(0.55+0.25*proximity+0.20*observedMovement));

    if(agent.hasLineOfSight&&agent.confidence>=config_.engagedConfidence) agent.alert=AIAlertState::Engaged;
    else if(agent.confidence>=config_.engagedConfidence) agent.alert=AIAlertState::Investigating;
    else if(agent.confidence>=config_.suspiciousConfidence) agent.alert=AIAlertState::Suspicious;
    else agent.alert=AIAlertState::Unaware;
}

void TacticalAICore::shareKnowledgeForAgent(std::size_t agentIndex) noexcept {
    auto& recipient=agents_[agentIndex];
    if(recipient.hasLineOfSight||!recipient.combatCapable) return;

    double bestConfidence=recipient.confidence;
    std::size_t bestSource=kMaxAgents;
    for(std::size_t i=0;i<agentCount_;++i){
        if(i==agentIndex) continue;
        const auto& source=agents_[i];
        if(!source.combatCapable||source.perceptionSource!=AIPerceptionSource::Vision||source.memoryAgeSeconds>config_.squadShareFreshSeconds) continue;
        if(distanceXZ(recipient.position,source.position)>config_.squadShareRangeMeters) continue;
        const double sharedConfidence=std::min(0.74,source.confidence*0.78);
        if(sharedConfidence>bestConfidence+0.02){
            bestConfidence=sharedConfidence;
            bestSource=i;
        }
    }
    if(bestSource==kMaxAgents) return;

    const auto& source=agents_[bestSource];
    recipient.confidence=bestConfidence;
    recipient.lastKnownPlayerPosition=squadEstimate(recipient.id,source.id,source.lastKnownPlayerPosition,bestConfidence,config_.squadShareMaxLocalizationErrorMeters);
    recipient.perceptionSource=AIPerceptionSource::Squad;
    recipient.squadSourceAgentId=source.id;
    recipient.memoryAgeSeconds=source.memoryAgeSeconds;
    recipient.threat=clamp01(std::max(recipient.threat,bestConfidence*0.72));
    recipient.alert=bestConfidence>=config_.engagedConfidence?AIAlertState::Investigating:AIAlertState::Suspicious;
}

void TacticalAICore::decideAgent(std::size_t agentIndex,const WorldCollisionCore& world) noexcept {
    auto& agent=agents_[agentIndex];
    auto& weapon=weapons_[agentIndex];
    agent.fireAuthorized=false;

    if(!agent.combatCapable){
        setAction(agent,AIActionState::Hold,agent.position);
        return;
    }
    if(weapon.state().reloading){
        setAction(agent,AIActionState::Reload,agent.position);
        return;
    }
    if(weapon.state().ammoInMagazine==0&&weapon.state().reserveAmmo>0){
        if(weapon.requestReload()) setAction(agent,AIActionState::Reload,agent.position);
        else setAction(agent,AIActionState::Hold,agent.position);
        return;
    }
    if(agent.perceptionSource==AIPerceptionSource::None||agent.alert==AIAlertState::Unaware){
        setAction(agent,AIActionState::Hold,agent.position);
        return;
    }

    const Vec3 threatPosition=agent.lastKnownPlayerPosition;
    const double threatDistance=distanceXZ(agent.position,threatPosition);

    if(agent.hasLineOfSight){
        Vec3 coverPosition{},peekPosition{};
        std::uint8_t coverIndex=kNoCoverCandidate;
        const bool retreat=agent.health01<=config_.retreatHealth01;
        const bool foundCover=selectCover(agent,world,threatPosition,retreat,coverPosition,peekPosition,coverIndex);
        if(foundCover){
            agent.coverPosition=coverPosition;
            agent.peekPosition=peekPosition;
            agent.coverCandidateIndex=coverIndex;
            if(distanceXZ(agent.position,coverPosition)>config_.coverArrivalRadiusMeters){
                setAction(agent,retreat?AIActionState::Retreat:AIActionState::MoveToCover,coverPosition);
                return;
            }
            if(distanceXZ(peekPosition,coverPosition)>0.10){
                setAction(agent,AIActionState::Peek,peekPosition);
                return;
            }
        }else{
            agent.coverPosition={};
            agent.peekPosition={};
            agent.coverCandidateIndex=kNoCoverCandidate;
        }

        if(retreat){
            const double dx=agent.position.x-threatPosition.x;
            const double dz=agent.position.z-threatPosition.z;
            const double length=std::hypot(dx,dz);
            if(length>1e-9){
                const Vec3 target{agent.position.x+dx/length*config_.flankOffsetMeters,agent.position.y,agent.position.z+dz/length*config_.flankOffsetMeters};
                setAction(agent,AIActionState::Retreat,target);
                return;
            }
        }

        if(agent.threat>=0.70&&threatDistance>=16.0){
            setAction(agent,AIActionState::Suppress,agent.position);
            return;
        }

        const double dx=threatPosition.x-agent.position.x;
        const double dz=threatPosition.z-agent.position.z;
        const double length=std::hypot(dx,dz);
        if(length>1e-9){
            const double sideSign=(agent.id&1u)?1.0:-1.0;
            const double sideX=dz/length*sideSign;
            const double sideZ=-dx/length*sideSign;
            const Vec3 flankTarget{agent.position.x+sideX*config_.flankOffsetMeters,agent.position.y,agent.position.z+sideZ*config_.flankOffsetMeters};
            if(directPathClear(world,agent.position,flankTarget)){
                setAction(agent,AIActionState::Flank,flankTarget);
                return;
            }
        }
        setAction(agent,AIActionState::Suppress,agent.position);
        return;
    }

    // Once the agent reaches previously selected cover, a peek is selected from the
    // cover edge using only the remembered/squad-estimated threat position. The peek
    // itself must reacquire Vision on a later perception slice before firing is legal.
    if(agent.coverCandidateIndex!=kNoCoverCandidate&&
       agent.coverCandidateIndex<world.coverCandidateCount()&&
       distanceXZ(agent.position,agent.coverPosition)<=config_.coverArrivalRadiusMeters*1.25){
        Vec3 peek{};
        if(computePeekPoint(agent,world,agent.coverCandidateIndex,threatPosition,peek)){
            agent.peekPosition=peek;
            setAction(agent,AIActionState::Peek,peek);
            return;
        }
    }

    if(threatDistance>0.75&&directPathClear(world,agent.position,threatPosition)){
        setAction(agent,AIActionState::Search,threatPosition);
    }else{
        setAction(agent,AIActionState::Hold,agent.position);
    }
}

void TacticalAICore::advanceAction(TacticalAgentState& agent,const WorldCollisionCore& world,double dt) noexcept {
    if(!agent.combatCapable) return;
    const bool moving=agent.action==AIActionState::MoveToCover||agent.action==AIActionState::Retreat||
                      agent.action==AIActionState::Flank||agent.action==AIActionState::Search;
    if(!moving) return;

    const double dx=agent.actionTarget.x-agent.position.x;
    const double dz=agent.actionTarget.z-agent.position.z;
    const double distance=std::hypot(dx,dz);
    if(distance<=config_.coverArrivalRadiusMeters){
        setAction(agent,AIActionState::Hold,agent.position);
        return;
    }
    const double speed=agent.action==AIActionState::Retreat?config_.retreatSpeedMetersPerSecond:config_.moveSpeedMetersPerSecond;
    const double step=std::min(distance,speed*dt);
    const double nx=dx/distance,nz=dz/distance;
    const double desiredX=agent.position.x+nx*step;
    const double desiredZ=agent.position.z+nz*step;
    const auto resolved=world.resolve(agent.position.x,agent.position.z,desiredX,desiredZ,kAgentRadius,kAgentCapsuleHeight);
    const double moved=std::hypot(resolved.x-agent.position.x,resolved.z-agent.position.z);
    agent.position.x=resolved.x;
    agent.position.z=resolved.z;
    if(moved>1e-8) agent.facingYaw=wrapAngle(std::atan2(nx,nz));
    if(moved<1e-8&&(resolved.hitX||resolved.hitZ)) setAction(agent,AIActionState::Hold,agent.position);
}

bool TacticalAICore::selectCover(const TacticalAgentState& agent,
                                 const WorldCollisionCore& world,
                                 Vec3 threatPosition,
                                 bool preferRetreat,
                                 Vec3& outPosition,
                                 Vec3& outPeekPosition,
                                 std::uint8_t& outCandidateIndex) const noexcept {
    bool found=false;
    double bestScore=0.0;
    for(std::size_t i=0;i<world.coverCandidateCount();++i){
        const auto& candidate=world.coverCandidates()[i];
        if(!candidate.valid) continue;
        const double agentDistance=distanceXZ(agent.position,candidate.position);
        if(agentDistance>config_.coverSearchRadiusMeters) continue;
        if(!directPathClear(world,agent.position,candidate.position)) continue;

        const auto block=world.raycastSegment({candidate.position.x,candidate.position.y+kCoverEyeHeight,candidate.position.z},
                                              {threatPosition.x,threatPosition.y+kPlayerChestHeight,threatPosition.z});
        if(!block.hit||block.obstacleIndex!=candidate.obstacleIndex) continue;

        Vec3 peek{};
        const bool hasPeek=computePeekPoint(agent,world,i,threatPosition,peek);
        const double threatDistance=distanceXZ(candidate.position,threatPosition);
        const double score=preferRetreat?(agentDistance-0.18*threatDistance):(agentDistance+0.015*threatDistance+(hasPeek?0.0:0.8));
        if(!found||score<bestScore-1e-9||(std::abs(score-bestScore)<=1e-9&&i<outCandidateIndex)){
            found=true;
            bestScore=score;
            outPosition=candidate.position;
            outPeekPosition=hasPeek?peek:Vec3{};
            outCandidateIndex=static_cast<std::uint8_t>(i);
        }
    }
    return found;
}

bool TacticalAICore::computePeekPoint(const TacticalAgentState& agent,
                                      const WorldCollisionCore& world,
                                      std::size_t coverCandidateIndex,
                                      Vec3 threatPosition,
                                      Vec3& outPeek) const noexcept {
    if(coverCandidateIndex>=world.coverCandidateCount()) return false;
    const auto& candidate=world.coverCandidates()[coverCandidateIndex];
    if(!candidate.valid) return false;
    const Vec3 tangent{-candidate.outwardNormal.z,0.0,candidate.outwardNormal.x};
    const double preferred=(agent.id&1u)?1.0:-1.0;
    const std::array<double,2> signs{preferred,-preferred};
    for(double sign:signs){
        const Vec3 peek{candidate.position.x+tangent.x*config_.peekOffsetMeters*sign+candidate.outwardNormal.x*0.06,
                        candidate.position.y,
                        candidate.position.z+tangent.z*config_.peekOffsetMeters*sign+candidate.outwardNormal.z*0.06};
        const auto resolved=world.resolve(peek.x,peek.z,peek.x,peek.z,kAgentRadius,kAgentCapsuleHeight);
        if(std::abs(resolved.x-peek.x)>1e-8||std::abs(resolved.z-peek.z)>1e-8) continue;
        if(!directPathClear(world,candidate.position,peek)) continue;
        const auto line=world.raycastSegment({peek.x,peek.y+config_.agentEyeHeight,peek.z},
                                             {threatPosition.x,threatPosition.y+kPlayerChestHeight,threatPosition.z});
        if(!line.hit){
            outPeek=peek;
            return true;
        }
    }
    return false;
}

bool TacticalAICore::directPathClear(const WorldCollisionCore& world,Vec3 from,Vec3 to) const noexcept {
    if(!finiteVec(from)||!finiteVec(to)) return false;
    if(distanceXZ(from,to)<=1e-8) return true;
    const Vec3 a{from.x,from.y+0.82,from.z};
    const Vec3 b{to.x,to.y+0.82,to.z};
    return !world.raycastSegment(a,b).hit;
}

bool TacticalAICore::authorizeFire(std::size_t agentIndex,const WorldCollisionCore& world) noexcept {
    if(agentIndex>=agentCount_) return false;
    auto& agent=agents_[agentIndex];
    auto& weapon=weapons_[agentIndex];
    if(!agent.combatCapable||!agent.hasLineOfSight||agent.perceptionSource!=AIPerceptionSource::Vision) return false;
    if(agent.action!=AIActionState::Peek&&agent.action!=AIActionState::Suppress) return false;

    const Vec3 firingPosition=agent.action==AIActionState::Peek?agent.peekPosition:agent.position;
    if(!finiteVec(firingPosition)) return false;
    if(agent.action==AIActionState::Peek&&distanceXZ(agent.peekPosition,agent.coverPosition)<=0.10) return false;
    const Vec3 camera{firingPosition.x,firingPosition.y+config_.agentEyeHeight,firingPosition.z};
    const Vec3 target{agent.lastKnownPlayerPosition.x,agent.lastKnownPlayerPosition.y+kPlayerChestHeight,agent.lastKnownPlayerPosition.z};
    const double dx=target.x-camera.x,dy=target.y-camera.y,dz=target.z-camera.z;
    const double horizontal=std::hypot(dx,dz);
    if(horizontal<=1e-8) return false;
    const double yaw=std::atan2(dx,dz);
    const double pitch=std::atan2(dy,horizontal);
    ShotSolution preview{};
    const std::uint64_t correlation=(static_cast<std::uint64_t>(agent.id)<<32u)|(agent.actionSequence&0xFFFFFFFFu);
    if(!weapon.previewShot(camera,yaw,pitch,correlation==0?1:correlation,preview)) return false;

    const Vec3 shortProbe{preview.origin.x+preview.direction.x*0.62,
                          preview.origin.y+preview.direction.y*0.62,
                          preview.origin.z+preview.direction.z*0.62};
    weapon.setObstructed(world.raycastSegment(preview.origin,shortProbe).hit);
    if(!weapon.canFireNow()) return false;

    const double targetDistance=std::sqrt((target.x-preview.origin.x)*(target.x-preview.origin.x)+
                                          (target.y-preview.origin.y)*(target.y-preview.origin.y)+
                                          (target.z-preview.origin.z)*(target.z-preview.origin.z));
    const Vec3 ballisticProbe{preview.origin.x+preview.direction.x*(targetDistance+0.05),
                              preview.origin.y+preview.direction.y*(targetDistance+0.05),
                              preview.origin.z+preview.direction.z*(targetDistance+0.05)};
    return !world.raycastSegment(preview.origin,ballisticProbe).hit;
}

void TacticalAICore::setAction(TacticalAgentState& agent,AIActionState action,Vec3 target) noexcept {
    if(agent.action!=action){
        agent.action=action;
        agent.actionAgeSeconds=0.0;
        ++agent.actionSequence;
    }
    agent.actionTarget=target;
    if(action!=AIActionState::Peek&&action!=AIActionState::Suppress) agent.fireAuthorized=false;
}

const WeaponState* TacticalAICore::agentWeaponState(std::size_t index) const noexcept {
    if(index>=agentCount_||agents_[index].id==0) return nullptr;
    return &weapons_[index].state();
}

TacticalAIReport TacticalAICore::report() const noexcept {
    TacticalAIReport out{};
    out.decisionsExecuted=decisionsExecuted_;
    for(std::size_t i=0;i<agentCount_;++i){
        const auto& agent=agents_[i];
        if(agent.id==0||!agent.combatCapable) continue;
        ++out.activeAgents;
        if(agent.hasLineOfSight) ++out.lineOfSightAgents;
        if(agent.heardPlayer) ++out.hearingAgents;
        if(agent.alert==AIAlertState::Suspicious) ++out.suspiciousAgents;
        else if(agent.alert==AIAlertState::Investigating) ++out.investigatingAgents;
        else if(agent.alert==AIAlertState::Engaged) ++out.engagedAgents;
        if(agent.hasCover) ++out.agentsInCover;
        if(agent.fireAuthorized) ++out.fireAuthorizedAgents;
        out.shotsFired+=weapons_[i].state().shotSequence;
        switch(agent.action){
            case AIActionState::MoveToCover: ++out.moveToCoverAgents; break;
            case AIActionState::Peek: ++out.peekAgents; break;
            case AIActionState::Reload: ++out.reloadAgents; break;
            case AIActionState::Suppress: ++out.suppressAgents; break;
            case AIActionState::Flank: ++out.flankAgents; break;
            case AIActionState::Retreat: ++out.retreatAgents; break;
            case AIActionState::Search: ++out.searchAgents; break;
            case AIActionState::Hold: break;
        }
        out.highestThreat=std::max(out.highestThreat,agent.threat);
    }
    if(out.retreatAgents>0&&out.engagedAgents>0) out.squadOrder=AISquadOrder::Defend;
    else if(out.engagedAgents>=std::max<std::uint32_t>(1,out.activeAgents/3)) out.squadOrder=AISquadOrder::Assault;
    else if(out.engagedAgents>0||out.investigatingAgents>0||out.suspiciousAgents>0) out.squadOrder=AISquadOrder::Search;
    else out.squadOrder=AISquadOrder::Hold;
    return out;
}

bool TacticalAICore::validate() const noexcept {
    if(agentCount_>kMaxAgents||(agentCount_==0?decisionCursor_!=0:decisionCursor_>=agentCount_)||
       !std::isfinite(config_.maxVisionDistanceMeters)||config_.maxVisionDistanceMeters<=0.0||
       !std::isfinite(config_.horizontalFovRadians)||config_.horizontalFovRadians<=0.0||
       !std::isfinite(config_.memorySeconds)||config_.memorySeconds<=0.0||
       !std::isfinite(config_.hearingBaseMeters)||!std::isfinite(config_.hearingMaxMeters)||config_.hearingBaseMeters<0.0||config_.hearingMaxMeters<config_.hearingBaseMeters||
       !std::isfinite(config_.hearingMaxLocalizationErrorMeters)||config_.hearingMaxLocalizationErrorMeters<0.0||
       !std::isfinite(config_.decisionIntervalSeconds)||config_.decisionIntervalSeconds<1.0/60.0||
       !std::isfinite(config_.moveSpeedMetersPerSecond)||config_.moveSpeedMetersPerSecond<=0.0||
       !std::isfinite(config_.retreatSpeedMetersPerSecond)||config_.retreatSpeedMetersPerSecond<config_.moveSpeedMetersPerSecond||
       !std::isfinite(config_.coverSearchRadiusMeters)||config_.coverSearchRadiusMeters<=0.0||
       !std::isfinite(config_.squadShareRangeMeters)||config_.squadShareRangeMeters<=0.0) return false;

    for(std::size_t i=0;i<agentCount_;++i){
        const auto& agent=agents_[i];
        if(agent.id==0) continue;
        if(!finiteVec(agent.position)||!finiteVec(agent.lastKnownPlayerPosition)||!finiteVec(agent.actionTarget)||!finiteVec(agent.coverPosition)||!finiteVec(agent.peekPosition)||
           !std::isfinite(agent.facingYaw)||!std::isfinite(agent.memoryAgeSeconds)||agent.memoryAgeSeconds<0.0||
           !std::isfinite(agent.decisionAgeSeconds)||agent.decisionAgeSeconds<0.0||!std::isfinite(agent.actionAgeSeconds)||agent.actionAgeSeconds<0.0||
           !std::isfinite(agent.confidence)||agent.confidence<0.0||agent.confidence>1.000001||
           !std::isfinite(agent.threat)||agent.threat<0.0||agent.threat>1.000001||
           !std::isfinite(agent.health01)||agent.health01<0.0||agent.health01>1.000001) return false;
        if(agent.combatCapable&&!agent.alive) return false;
        if(!agent.combatCapable&&(agent.hasLineOfSight||agent.heardPlayer||agent.fireAuthorized||agent.action!=AIActionState::Hold||agent.perceptionSource!=AIPerceptionSource::None)) return false;
        if(agent.alert==AIAlertState::Engaged&&(!agent.hasLineOfSight||agent.perceptionSource!=AIPerceptionSource::Vision)) return false;
        if(agent.perceptionSource==AIPerceptionSource::None&&(agent.confidence>1e-9||agent.squadSourceAgentId!=0)) return false;
        if(agent.perceptionSource==AIPerceptionSource::Squad&&agent.squadSourceAgentId==0) return false;
        if(agent.coverCandidateIndex!=kNoCoverCandidate&&agent.coverCandidateIndex>=WorldCollisionCore::kMaxCoverCandidates) return false;
        if(agent.action==AIActionState::Peek&&distanceXZ(agent.peekPosition,agent.coverPosition)<=0.10) return false;
        if(agent.fireAuthorized&&(!agent.combatCapable||!agent.hasLineOfSight||agent.perceptionSource!=AIPerceptionSource::Vision||
                                  (agent.action!=AIActionState::Peek&&agent.action!=AIActionState::Suppress))) return false;
        if(!weapons_[i].validate()) return false;
    }
    return true;
}

double TacticalAICore::clamp01(double value) noexcept { return std::clamp(value,0.0,1.0); }
double TacticalAICore::distanceXZ(Vec3 a,Vec3 b) noexcept { return std::hypot(a.x-b.x,a.z-b.z); }
bool TacticalAICore::finiteVec(Vec3 value) noexcept { return std::isfinite(value.x)&&std::isfinite(value.y)&&std::isfinite(value.z); }
double TacticalAICore::wrapAngle(double radians) noexcept {
    while(radians>kPi) radians-=2.0*kPi;
    while(radians<-kPi) radians+=2.0*kPi;
    return radians;
}

Vec3 TacticalAICore::hearingEstimate(std::uint32_t agentId,Vec3 playerPosition,double confidence,double maxErrorMeters) noexcept {
    if(maxErrorMeters<=0.0||confidence>=0.999) return playerPosition;
    const double phase=std::fmod(static_cast<double>(agentId)*2.399963229728653,2.0*kPi);
    const double radius=maxErrorMeters*(1.0-clamp01(confidence));
    return {playerPosition.x+std::cos(phase)*radius,playerPosition.y,playerPosition.z+std::sin(phase)*radius};
}

Vec3 TacticalAICore::squadEstimate(std::uint32_t recipientId,std::uint32_t sourceId,Vec3 knownPosition,double confidence,double maxErrorMeters) noexcept {
    if(maxErrorMeters<=0.0||confidence>=0.999) return knownPosition;
    const std::uint64_t mixed=static_cast<std::uint64_t>(recipientId)*0x9E3779B185EBCA87ull ^ static_cast<std::uint64_t>(sourceId)*0xC2B2AE3D27D4EB4Full;
    const double unit=static_cast<double>(mixed%1000003ull)/1000003.0;
    const double phase=unit*2.0*kPi;
    const double radius=maxErrorMeters*(1.0-clamp01(confidence));
    return {knownPosition.x+std::cos(phase)*radius,knownPosition.y,knownPosition.z+std::sin(phase)*radius};
}

#ifdef METSE_TESTING
bool TacticalAICore::testOnlyDrainAgentMagazine(std::size_t index) noexcept {
    if(index>=agentCount_||agents_[index].id==0) return false;
    auto& weapon=weapons_[index];
    weapon.reset();
    weapon.setObstructed(false);
    std::uint64_t correlation=(static_cast<std::uint64_t>(agents_[index].id)<<32u)|1u;
    std::size_t guard=0;
    while(weapon.state().ammoInMagazine>0&&guard<weapon.config().magazineSize*3u+8u){
        if(weapon.canFireNow()){
            ShotSolution ignored{};
            if(!weapon.fire({0.0,1.58,0.0},0.0,0.0,correlation++,ignored)) return false;
        }
        weapon.fixedStep(0.10,0.0,0.0,false);
        ++guard;
    }
    return weapon.state().ammoInMagazine==0;
}
#endif

} // namespace metse
