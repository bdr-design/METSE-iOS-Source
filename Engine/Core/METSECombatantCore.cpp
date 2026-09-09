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
    if(index>=kMaxCombatants||identity.id==0||identity.teamId==0||identity.factionId==0) return false;
    for(std::size_t i=0;i<count_;++i){
        if(i!=index&&records_[i].identity.id==identity.id) return false;
    }
    if(!alive) combatCapable=false;
    records_[index]={identity,alive,combatCapable,targetable&&alive};
    count_=count_>index?count_:index+1;
    return true;
}

bool CombatantCore::syncState(std::size_t index,
                              CombatantId id,
                              bool alive,
                              bool combatCapable,
                              bool targetable) noexcept {
    if(index>=count_||id==0||records_[index].identity.id!=id) return false;
    if(!alive) combatCapable=false;
    records_[index].alive=alive;
    records_[index].combatCapable=combatCapable;
    records_[index].targetable=targetable&&alive;
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

bool CombatantCore::validate() const noexcept {
    if(count_>kMaxCombatants) return false;
    for(std::size_t i=0;i<count_;++i){
        const auto& record=records_[i];
        if(record.identity.id==0||record.identity.teamId==0||record.identity.factionId==0) return false;
        if(!record.alive&&(record.combatCapable||record.targetable)) return false;
        for(std::size_t j=i+1;j<count_;++j){
            if(records_[j].identity.id==record.identity.id) return false;
        }
    }
    for(std::size_t i=count_;i<kMaxCombatants;++i){
        if(records_[i].identity.id!=0||records_[i].alive||records_[i].combatCapable||records_[i].targetable) return false;
    }
    return true;
}

} // namespace metse
