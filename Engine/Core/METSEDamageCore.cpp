#include "METSEDamageCore.hpp"
#include <algorithm>
#include <cmath>

namespace metse {
namespace {
constexpr double kHelmetArmorCapacityJ = 420.0;
constexpr double kTorsoArmorCapacityJ = 900.0;
constexpr double kIncapacitatedHealthThreshold = 22.0;
constexpr double kWoundedHealthThreshold = 78.0;
constexpr double kArmorStoppedResidualJ = 120.0;

bool finiteVec(Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
}

DamageCore::DamageCore() noexcept { reset(); }

void DamageCore::reset() noexcept {
    targets_ = {};
    // Training targets intentionally remain on unobstructed sight lines from the
    // default camera so Aim Truth, anatomy and ballistic ordering can be verified.
    targetCount_ = 2;
    targets_[0].id = 1;
    targets_[0].position = {-10.0,0.0,18.0};
    targets_[0].radius = 0.36;
    targets_[1].id = 2;
    targets_[1].position = {22.0,0.0,26.0};
    targets_[1].radius = 0.36;
    for(std::size_t i=0;i<targetCount_;++i){
        targets_[i].teamId=CombatantCore::kHostileTeam;
        targets_[i].factionId=CombatantCore::kHostileFaction;
        targets_[i].role=CombatantRole::AI;
    }
    playerTarget_={};
    playerTarget_.id=CombatantCore::kPlayerId;
    playerTarget_.teamId=CombatantCore::kPlayerTeam;
    playerTarget_.factionId=CombatantCore::kPlayerFaction;
    playerTarget_.role=CombatantRole::Player;
    playerTarget_.position={0.0,0.0,0.0};
    playerTarget_.radius=0.36;
    playerTargetEnabled_=false;
    metrics_ = {};
    results_ = {};
    lastResult_ = {};
    resultSequence_ = 0;
    friendlyFireDenials_=0;
}

bool DamageCore::configureAgentCount(std::size_t count) noexcept {
    if(count>kMaxTargets) return false;
    targets_={};
    targetCount_=count;
    for(std::size_t i=0;i<count;++i){
        auto& target=targets_[i];
        target.id=static_cast<CombatantId>(i+1u);
        target.teamId=CombatantCore::kHostileTeam;
        target.factionId=CombatantCore::kHostileFaction;
        target.role=CombatantRole::AI;
        target.radius=0.36;
        // Preserve the two legacy aim anchors. Additional combatants are placed by a
        // deterministic bounded ring so 32-agent stress never needs a content list.
        if(i==0) target.position={-10.0,0.0,18.0};
        else if(i==1) target.position={22.0,0.0,26.0};
        else {
            const double angle=static_cast<double>(i)*2.399963229728653;
            const double radius=18.0+static_cast<double>(i%4u)*3.0;
            target.position={std::clamp(std::cos(angle)*radius,-42.0,42.0),0.0,
                             std::clamp(std::sin(angle)*radius,-42.0,42.0)};
        }
    }
    metrics_={};
    results_={};
    lastResult_={};
    resultSequence_=0;
    friendlyFireDenials_=0;
    return true;
}

bool DamageCore::configureTargetIdentity(std::size_t index,CombatantIdentity identity) noexcept {
    if(index>=targetCount_||identity.id==0||targets_[index].id!=identity.id||identity.role!=CombatantRole::AI) return false;
    targets_[index].teamId=identity.teamId;
    targets_[index].factionId=identity.factionId;
    targets_[index].role=identity.role;
    return true;
}

bool DamageCore::configurePlayerTarget(CombatantIdentity identity) noexcept {
    if(identity.id==0||identity.role!=CombatantRole::Player||identity.teamId==0||identity.factionId==0) return false;
    const auto previousPosition=playerTarget_.position;
    playerTarget_={};
    playerTarget_.id=identity.id;
    playerTarget_.teamId=identity.teamId;
    playerTarget_.factionId=identity.factionId;
    playerTarget_.role=identity.role;
    playerTarget_.position=finiteVec(previousPosition)?previousPosition:Vec3{};
    playerTarget_.health=100.0;
    playerTarget_.helmetArmorJoules=kHelmetArmorCapacityJ;
    playerTarget_.torsoArmorJoules=kTorsoArmorCapacityJ;
    playerTarget_.alive=true;
    playerTarget_.combatState=CombatState::Effective;
    playerTarget_.bleedingPerSecond=0.0;
    playerTarget_.lastDamageCorrelationId=0;
    playerTarget_.radius=0.36;
    playerTargetEnabled_=false;
    return true;
}

bool DamageCore::setPlayerTargetEnabled(bool enabled) noexcept {
    if(enabled&&(playerTarget_.id==0||playerTarget_.role!=CombatantRole::Player)) return false;
    playerTargetEnabled_=enabled;
    return true;
}

bool DamageCore::syncTargetPosition(std::size_t index,std::uint32_t id,Vec3 position) noexcept {
    if(index>=targetCount_||id==0||targets_[index].id!=id||!finiteVec(position)) return false;
    targets_[index].position=position;
    return true;
}

bool DamageCore::syncPlayerTargetPosition(CombatantId id,Vec3 position) noexcept {
    if(!playerTargetEnabled_||id==0||playerTarget_.id!=id||!finiteVec(position)) return false;
    playerTarget_.position=position;
    return true;
}

bool DamageCore::syncPlayerTargetState(CombatantId id,bool alive,bool combatCapable,double health01) noexcept {
    if(!playerTargetEnabled_||id==0||playerTarget_.id!=id||!std::isfinite(health01)) return false;
    playerTarget_.alive=alive;
    playerTarget_.health=std::clamp(health01,0.0,1.0)*100.0;
    playerTarget_.combatState=alive?(combatCapable?classifyState(playerTarget_.health,playerTarget_.bleedingPerSecond):CombatState::Incapacitated):CombatState::Dead;
    if(!alive) playerTarget_.bleedingPerSecond=0.0;
    return true;
}

double DamageCore::segmentPointDistanceXZ(const Vec3& a,const Vec3& b,const Vec3& p,double& t) noexcept {
    const double dx=b.x-a.x, dz=b.z-a.z;
    const double denom=dx*dx+dz*dz;
    if (denom <= 1e-12) { t=0.0; return std::hypot(p.x-a.x,p.z-a.z); }
    t=std::clamp(((p.x-a.x)*dx+(p.z-a.z)*dz)/denom,0.0,1.0);
    return std::hypot(a.x+dx*t-p.x,a.z+dz*t-p.z);
}

Vec3 DamageCore::normalize(Vec3 value) noexcept {
    const double length=std::sqrt(value.x*value.x+value.y*value.y+value.z*value.z);
    if (!std::isfinite(length) || length<=1e-12) return {};
    return {value.x/length,value.y/length,value.z/length};
}

CombatState DamageCore::classifyState(double health,double bleedingPerSecond) noexcept {
    if (health<=0.0) return CombatState::Dead;
    if (health<=kIncapacitatedHealthThreshold) return CombatState::Incapacitated;
    if (health<kWoundedHealthThreshold || bleedingPerSecond>0.05) return CombatState::Wounded;
    return CombatState::Effective;
}

ArmorZone DamageCore::armorZoneFor(HitRegion region) noexcept {
    if (region==HitRegion::Head) return ArmorZone::Helmet;
    if (region==HitRegion::Thorax || region==HitRegion::Abdomen) return ArmorZone::Torso;
    return ArmorZone::None;
}

double DamageCore::regionDamageMultiplier(HitRegion region) noexcept {
    switch(region){
        case HitRegion::Head: return 1.35;
        case HitRegion::Neck: return 1.20;
        case HitRegion::Thorax: return 0.78;
        case HitRegion::Abdomen: return 0.62;
        case HitRegion::Arm: return 0.34;
        case HitRegion::Leg: return 0.38;
        case HitRegion::None: default: return 0.0;
    }
}

double DamageCore::regionBleedingScale(HitRegion region) noexcept {
    switch(region){
        case HitRegion::Head: return 0.20;
        case HitRegion::Neck: return 1.00;
        case HitRegion::Thorax: return 0.55;
        case HitRegion::Abdomen: return 0.80;
        case HitRegion::Arm: return 0.50;
        case HitRegion::Leg: return 0.60;
        case HitRegion::None: default: return 0.0;
    }
}

DamageIntersection DamageCore::traceTarget(const DamageTarget& target,
                                            std::size_t targetIndex,
                                            bool playerTarget,
                                            const Vec3& from,
                                            const Vec3& to,
                                            DamageIntersection best) noexcept {
    if(!target.alive) return best;
    double t=0.0;
    const double radialDistance=segmentPointDistanceXZ(from,to,target.position,t);
    if(radialDistance>target.radius||t>=best.t) return best;
    const Vec3 point={from.x+(to.x-from.x)*t,from.y+(to.y-from.y)*t,from.z+(to.z-from.z)*t};
    const double hitY=point.y-target.position.y;
    if(hitY<0.15||hitY>1.88) return best;

    HitRegion region=HitRegion::Leg;
    if(hitY>=1.60) region=HitRegion::Head;
    else if(hitY>=1.48) region=HitRegion::Neck;
    else if(hitY>=1.05) region=(radialDistance>0.22?HitRegion::Arm:HitRegion::Thorax);
    else if(hitY>=0.72) region=(radialDistance>0.24?HitRegion::Arm:HitRegion::Abdomen);
    best={true,t,targetIndex,target.id,playerTarget,region,point,radialDistance};
    return best;
}

DamageIntersection DamageCore::traceSegment(const Vec3& from,const Vec3& to) const noexcept {
    return traceSegment(from,to,DamageSource{});
}

DamageIntersection DamageCore::traceSegment(const Vec3& from,const Vec3& to,const DamageSource& source) const noexcept {
    DamageIntersection best{};
    if(!finiteVec(from)||!finiteVec(to)) return best;
    for (std::size_t i=0;i<targetCount_;++i) {
        const auto& target=targets_[i];
        const CombatantIdentity targetIdentity{target.id,target.teamId,target.factionId,target.role};
        const auto candidate=traceTarget(target,i,false,from,to,DamageIntersection{});
        if(!CombatantCore::canTarget(source,targetIdentity)){
            if(source.identity.id!=0&&candidate.hit) ++friendlyFireDenials_;
            continue;
        }
        if(candidate.hit&&(!best.hit||candidate.t<best.t)) best=candidate;
    }
    if(source.includePlayerTarget&&playerTargetEnabled_){
        const CombatantIdentity targetIdentity{playerTarget_.id,playerTarget_.teamId,playerTarget_.factionId,playerTarget_.role};
        const auto candidate=traceTarget(playerTarget_,kPlayerTargetIndex,true,from,to,DamageIntersection{});
        if(CombatantCore::canTarget(source,targetIdentity)){
            if(candidate.hit&&(!best.hit||candidate.t<best.t)) best=candidate;
        }else if(source.identity.id!=0&&candidate.hit) ++friendlyFireDenials_;
    }
    return best;
}

void DamageCore::applyStateTransition(DamageTarget& target,
                                      CombatState previous,
                                      CombatState next,
                                      DamageResult& result) noexcept {
    result.previousState=previous;
    result.newState=next;
    result.stateChanged=previous!=next;
    result.incapacitated=next==CombatState::Incapacitated && previous!=CombatState::Incapacitated;
    result.killed=next==CombatState::Dead && previous!=CombatState::Dead;
    if(result.incapacitated) ++metrics_.incapacitations;
    if(result.killed) ++metrics_.kills;
    target.combatState=next;
    target.alive=next!=CombatState::Dead;
    if(next==CombatState::Dead) target.bleedingPerSecond=0.0;
}

void DamageCore::queueResult(DamageResult result) noexcept {
    result.sequence=++resultSequence_;
    results_[(result.sequence-1u)%kResultCapacity]=result;
    lastResult_=result;
}

DamageResult DamageCore::applyIntersection(const DamageIntersection& hit,
                                           double energy,
                                           std::uint64_t correlationId,
                                           Vec3 impactDirection) noexcept {
    DamageResult out{};
    if (!hit.hit || (!hit.playerTarget&&hit.targetIndex>=targetCount_) ||
        (hit.playerTarget&&(!playerTargetEnabled_||hit.targetIndex!=kPlayerTargetIndex)) ||
        !std::isfinite(energy) || energy<=0.0 || correlationId==0) return out;
    auto& target=hit.playerTarget?playerTarget_:targets_[hit.targetIndex];
    if (!target.alive || target.id!=hit.targetId) return out;

    out.hit=true;
    out.targetId=target.id;
    out.region=hit.region;
    out.cause=DamageCause::Impact;
    out.correlationId=correlationId;
    out.reactionDirection=normalize(impactDirection);
    const CombatState previous=target.combatState;

    out.armorZone=armorZoneFor(hit.region);
    double effectiveEnergy=energy;
    if(out.armorZone!=ArmorZone::None){
        double* durability=out.armorZone==ArmorZone::Helmet?&target.helmetArmorJoules:&target.torsoArmorJoules;
        const double absorptionFraction=out.armorZone==ArmorZone::Helmet?0.62:0.70;
        if(*durability>0.0){
            out.armorHit=true;
            ++metrics_.armorHits;
            out.armorAbsorbedJoules=std::min(*durability,energy*absorptionFraction);
            *durability=std::max(0.0,*durability-out.armorAbsorbedJoules);
            effectiveEnergy=std::max(0.0,energy-out.armorAbsorbedJoules);
            out.armorPenetrated=effectiveEnergy>kArmorStoppedResidualJ;
        }
    }

    double damage=std::clamp(effectiveEnergy/8.0*regionDamageMultiplier(hit.region),2.0,140.0);
    if(out.armorHit && !out.armorPenetrated) damage=std::min(damage,6.0);
    target.health=std::max(0.0,target.health-damage);
    out.damage=damage;

    // Bleeding is a bounded gameplay state, not a medical simulation. Armor affects it
    // indirectly by reducing delivered damage; no hidden random roll is involved.
    if(target.health>0.0 && damage>4.0){
        const double desiredAddition=damage*regionBleedingScale(hit.region)*0.020;
        const double available=kMaxBleedingPerSecond-target.bleedingPerSecond;
        out.bleedingAddedPerSecond=std::clamp(desiredAddition,0.0,std::max(0.0,available));
        target.bleedingPerSecond+=out.bleedingAddedPerSecond;
    }

    target.lastDamageCorrelationId=correlationId;
    const CombatState next=classifyState(target.health,target.bleedingPerSecond);
    applyStateTransition(target,previous,next,out);
    out.remainingHealth=target.health;
    out.bleedingPerSecond=target.bleedingPerSecond;
    ++metrics_.hits;
    queueResult(out);
    return lastResult_;
}

DamageResult DamageCore::applySegment(const Vec3& from,const Vec3& to,double energy,std::uint64_t correlationId) noexcept {
    return applyIntersection(traceSegment(from,to),energy,correlationId,{to.x-from.x,to.y-from.y,to.z-from.z});
}

DamageResult DamageCore::applySegment(const Vec3& from,
                                      const Vec3& to,
                                      double energy,
                                      std::uint64_t correlationId,
                                      const DamageSource& source) noexcept {
    return applyIntersection(traceSegment(from,to,source),energy,correlationId,
                             {to.x-from.x,to.y-from.y,to.z-from.z});
}

void DamageCore::fixedStep(double dt) noexcept {
    if(!std::isfinite(dt)||dt<=0.0) return;
    // Engine ownership guarantees 60 Hz. The clamp is a defensive bound for direct
    // callers/tests and prevents a lifecycle-sized delta from draining a target at once.
    const double safeDt=std::min(dt,0.25);
    auto advanceBleeding=[&](DamageTarget& target){
        if(!target.alive || target.bleedingPerSecond<=0.0) return;
        const double beforeHealth=target.health;
        const CombatState previous=target.combatState;
        const double loss=std::min(target.health,target.bleedingPerSecond*safeDt);
        target.health=std::max(0.0,target.health-loss);
        target.bleedingPerSecond=std::max(0.0,target.bleedingPerSecond-0.020*safeDt);
        const CombatState next=classifyState(target.health,target.bleedingPerSecond);
        target.combatState=next;
        target.alive=next!=CombatState::Dead;

        if(previous!=next && (next==CombatState::Incapacitated || next==CombatState::Dead)){
            DamageResult result{};
            result.cause=DamageCause::Bleeding;
            result.targetId=target.id;
            result.damage=beforeHealth-target.health;
            result.remainingHealth=target.health;
            result.correlationId=target.lastDamageCorrelationId;
            applyStateTransition(target,previous,next,result);
            result.remainingHealth=target.health;
            result.bleedingPerSecond=target.bleedingPerSecond;
            ++metrics_.bleedTransitions;
            if(result.correlationId!=0) queueResult(result);
        }
    };
    for(std::size_t i=0;i<targetCount_;++i) advanceBleeding(targets_[i]);
    if(playerTargetEnabled_) advanceBleeding(playerTarget_);
}

bool DamageCore::resultBySequence(std::uint64_t sequence,DamageResult& out) const noexcept {
    if(sequence==0 || sequence>resultSequence_) return false;
    if(resultSequence_-sequence>=kResultCapacity) return false;
    const auto& candidate=results_[(sequence-1u)%kResultCapacity];
    if(candidate.sequence!=sequence) return false;
    out=candidate;
    return true;
}

bool DamageCore::combatCapable(const DamageTarget& target) noexcept {
    return target.alive && target.combatState!=CombatState::Incapacitated && target.combatState!=CombatState::Dead;
}

bool DamageCore::validate() const noexcept {
    if (targetCount_>kMaxTargets) return false;
    if(metrics_.kills>metrics_.hits+metrics_.bleedTransitions || metrics_.incapacitations>metrics_.hits+metrics_.bleedTransitions) return false;
    for (std::size_t i=0;i<targetCount_;++i) {
        const auto& t=targets_[i];
        if (t.id==0 || !finiteVec(t.position) || !std::isfinite(t.health) || t.health<0.0 || t.health>100.0001 ||
            !std::isfinite(t.radius) || t.radius<=0.0 || !std::isfinite(t.helmetArmorJoules) || t.helmetArmorJoules<0.0 || t.helmetArmorJoules>kHelmetArmorCapacityJ+1e-6 ||
            !std::isfinite(t.torsoArmorJoules) || t.torsoArmorJoules<0.0 || t.torsoArmorJoules>kTorsoArmorCapacityJ+1e-6 ||
            !std::isfinite(t.bleedingPerSecond) || t.bleedingPerSecond<0.0 || t.bleedingPerSecond>kMaxBleedingPerSecond+1e-6 ||
            t.teamId==0||t.factionId==0||t.role!=CombatantRole::AI) return false;
        if (t.alive != (t.combatState!=CombatState::Dead)) return false;
        if (t.combatState!=classifyState(t.health,t.bleedingPerSecond)) return false;
        if (t.bleedingPerSecond>0.0 && t.lastDamageCorrelationId==0) return false;
        for(std::size_t j=i+1;j<targetCount_;++j) if(targets_[j].id==t.id) return false;
    }
    if(playerTargetEnabled_){
        const auto& t=playerTarget_;
        if(t.id==0||!finiteVec(t.position)||!std::isfinite(t.health)||t.health<0.0||t.health>100.0001||
           !std::isfinite(t.radius)||t.radius<=0.0||!std::isfinite(t.helmetArmorJoules)||t.helmetArmorJoules<0.0||t.helmetArmorJoules>kHelmetArmorCapacityJ+1e-6||
           !std::isfinite(t.torsoArmorJoules)||t.torsoArmorJoules<0.0||t.torsoArmorJoules>kTorsoArmorCapacityJ+1e-6||
           !std::isfinite(t.bleedingPerSecond)||t.bleedingPerSecond<0.0||t.bleedingPerSecond>kMaxBleedingPerSecond+1e-6||
           t.teamId==0||t.factionId==0||t.role!=CombatantRole::Player||t.alive!=(t.combatState!=CombatState::Dead)||
           t.combatState!=classifyState(t.health,t.bleedingPerSecond)) return false;
        for(std::size_t i=0;i<targetCount_;++i) if(targets_[i].id==t.id) return false;
    }
    if(resultSequence_>0 && lastResult_.sequence!=resultSequence_) return false;
    const std::uint64_t retained=std::min<std::uint64_t>(resultSequence_,kResultCapacity);
    const std::uint64_t first=resultSequence_-retained+1u;
    for(std::uint64_t sequence=first;sequence<=resultSequence_;++sequence){
        DamageResult result{};
        if(!resultBySequence(sequence,result) || result.sequence!=sequence || result.targetId==0 || result.correlationId==0 ||
           !std::isfinite(result.damage) || result.damage<0.0 || !std::isfinite(result.armorAbsorbedJoules) || result.armorAbsorbedJoules<0.0 ||
           !std::isfinite(result.bleedingAddedPerSecond) || result.bleedingAddedPerSecond<0.0 || !std::isfinite(result.remainingHealth) || result.remainingHealth<0.0 ||
           !std::isfinite(result.bleedingPerSecond) || result.bleedingPerSecond<0.0 || !finiteVec(result.reactionDirection)) return false;
    }
    return true;
}

} // namespace metse
