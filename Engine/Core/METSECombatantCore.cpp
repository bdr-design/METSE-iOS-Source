#include "METSECombatantCore.hpp"

namespace metse {

void CombatantCore::reset() noexcept {
    records_ = {};
    count_ = 0;
}

bool CombatantCore::configure(std::size_t index,
                              CombatantIdentity identity,
                              bool alive,
                              bool combatCapable,
                              bool targetable) noexcept {
    if(index>=kMaxCombatants||index>count_||identity.id==0||identity.teamId==0||identity.factionId==0||
       (identity.role!=CombatantRole::Player&&identity.role!=CombatantRole::AI)) return false;
    for(std::size_t i=0;i<count_;++i){
        if(i!=index&&records_[i].identity.id==identity.id) return false;
    }
    if(!alive) combatCapable=false;
    const auto lifecycle=!alive?CombatantLifecycleState::Dead:
        (combatCapable?CombatantLifecycleState::Active:CombatantLifecycleState::Incapacitated);
    records_[index]={identity,alive,combatCapable,targetable&&alive,lifecycle};
    count_=count_>index?count_:index+1;
    return true;
}

bool CombatantCore::syncState(std::size_t index,
                              CombatantId id,
                              bool alive,
                              bool combatCapable,
                              bool targetable) noexcept {
    const auto lifecycle=!alive?CombatantLifecycleState::Dead:
        (combatCapable?CombatantLifecycleState::Active:CombatantLifecycleState::Incapacitated);
    return syncLifecycle(index,id,lifecycle,targetable);
}

bool CombatantCore::syncLifecycle(std::size_t index,
                                  CombatantId id,
                                  CombatantLifecycleState lifecycle,
                                  bool targetable) noexcept {
    if(index>=count_||id==0||records_[index].identity.id!=id) return false;
    if(lifecycle>CombatantLifecycleState::Removed) return false;
    auto& record=records_[index];
    // Runtime mirroring cannot resurrect a terminal identity. Spawn/reset must
    // explicitly use configure; Removed does not imply reusable slots.
    if(record.lifecycle==CombatantLifecycleState::Removed&&lifecycle!=record.lifecycle) return false;
    if(record.lifecycle==CombatantLifecycleState::Dead&&
       lifecycle!=CombatantLifecycleState::Dead&&lifecycle!=CombatantLifecycleState::Removed) return false;
    if(lifecycle==CombatantLifecycleState::Removed||lifecycle==CombatantLifecycleState::Dead){
        record.alive=false;
        record.combatCapable=false;
        record.targetable=false;
    }else if(lifecycle==CombatantLifecycleState::Incapacitated){
        record.alive=true;
        record.combatCapable=false;
        record.targetable=targetable;
    }else{
        record.alive=true;
        record.combatCapable=true;
        record.targetable=targetable;
    }
    record.lifecycle=lifecycle;
    return true;
}

TargetRelation CombatantCore::relation(CombatantIdentity source,CombatantIdentity target) noexcept {
    if(source.id==0||target.id==0||source.teamId==0||target.teamId==0||source.factionId==0||target.factionId==0)
        return TargetRelation::Invalid;
    if(source.id==target.id) return TargetRelation::Self;
    if(source.teamId==target.teamId) return TargetRelation::Friendly;
    if(source.factionId==target.factionId) return TargetRelation::Neutral;
    return TargetRelation::Hostile;
}

bool CombatantCore::canTarget(const DamageSource& source,CombatantIdentity target) noexcept {
    // Legacy/test shots without provenance remain valid for the pre-010 DamageCore
    // contract. Production EngineCore always supplies a complete source identity.
    if(source.identity.id==0) return true;
    const auto targetRelation=relation(source.identity,target);
    if(targetRelation==TargetRelation::Hostile) return true;
    if(targetRelation==TargetRelation::Friendly&&source.policy==TargetingPolicy::AllowFriendlyFire) return true;
    return false;
}

const CombatantRecord* CombatantCore::recordById(CombatantId id) const noexcept {
    if(id==0) return nullptr;
    for(std::size_t i=0;i<count_;++i){
        if(records_[i].identity.id==id) return &records_[i];
    }
    return nullptr;
}

CombatantLifecycleReport CombatantCore::report() const noexcept {
    CombatantLifecycleReport out{};
    for(std::size_t i=0;i<count_;++i){
        switch(records_[i].lifecycle){
            case CombatantLifecycleState::Active: ++out.active; break;
            case CombatantLifecycleState::Wounded: ++out.wounded; break;
            case CombatantLifecycleState::Incapacitated: ++out.incapacitated; break;
            case CombatantLifecycleState::Dead: ++out.dead; break;
            case CombatantLifecycleState::Removed: ++out.removed; break;
        }
    }
    return out;
}

bool CombatantCore::validate() const noexcept {
    if(count_>kMaxCombatants) return false;
    for(std::size_t i=0;i<count_;++i){
        const auto& record=records_[i];
        if(record.identity.id==0||record.identity.teamId==0||record.identity.factionId==0) return false;
        if((record.identity.role!=CombatantRole::Player&&record.identity.role!=CombatantRole::AI)||
           record.lifecycle>CombatantLifecycleState::Removed) return false;
        if(!record.alive&&(record.combatCapable||record.targetable)) return false;
        if((record.lifecycle==CombatantLifecycleState::Dead||record.lifecycle==CombatantLifecycleState::Removed) &&
           (record.alive||record.combatCapable||record.targetable)) return false;
        if(record.lifecycle==CombatantLifecycleState::Incapacitated &&
           (!record.alive||record.combatCapable)) return false;
        if((record.lifecycle==CombatantLifecycleState::Active||record.lifecycle==CombatantLifecycleState::Wounded) &&
           (!record.alive||!record.combatCapable)) return false;
        for(std::size_t j=i+1;j<count_;++j){
            if(records_[j].identity.id==record.identity.id) return false;
        }
    }
    for(std::size_t i=count_;i<kMaxCombatants;++i){
        if(records_[i].identity.id!=0||records_[i].alive||records_[i].combatCapable||records_[i].targetable||
           records_[i].lifecycle!=CombatantLifecycleState::Active) return false;
    }
    return true;
}

} // namespace metse
