#include "METSEVisibilityCore.hpp"
#include "METSEWorldCollision.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace metse {
namespace {

struct VisibilityCandidate {
    std::size_t index=0;
    VisibilityTier desired=VisibilityTier::Dormant;
    double score=0.0;
};

bool finiteVec(Vec3 value) noexcept {
    return std::isfinite(value.x)&&std::isfinite(value.y)&&std::isfinite(value.z);
}

} // namespace

void VisibilityCore::reset() noexcept {
    entities_={};
    count_=0;
    lastEvaluated_=0;
    lastOccluded_=0;
    lastBudgetDemotions_=0;
}

void VisibilityCore::syncTarget(std::size_t index,
                                std::uint32_t id,
                                Vec3 position,
                                bool alive) noexcept {
    if(index>=kMaxEntities||id==0) return;
    entities_[index]={id,position,VisibilityTier::Dormant,alive,false};
    count_=std::max(count_,index+1);
}

void VisibilityCore::update(Vec3 camera,
                            double yaw,
                            const WorldCollisionCore& world) noexcept {
    lastEvaluated_=0;
    lastOccluded_=0;
    lastBudgetDemotions_=0;
    for(std::size_t i=0;i<count_;++i){
        entities_[i].tier=VisibilityTier::Dormant;
        entities_[i].lineOfSight=false;
    }
    if(!finiteVec(camera)||!std::isfinite(yaw)) return;

    std::array<VisibilityCandidate,kMaxEntities> candidates{};
    std::size_t candidateCount=0;
    const double sinYaw=std::sin(yaw);
    const double cosYaw=std::cos(yaw);
    for(std::size_t i=0;i<count_;++i){
        auto& entity=entities_[i];
        if(!entity.alive||!finiteVec(entity.position)) continue;
        const double dx=entity.position.x-camera.x;
        const double dz=entity.position.z-camera.z;
        const double distance=std::hypot(dx,dz);
        const double forward=(distance>1e-6)?(dx*sinYaw+dz*cosYaw)/distance:1.0;
        VisibilityTier desired=VisibilityTier::Dormant;
        if(distance<35.0&&forward>-0.25) desired=VisibilityTier::Full;
        else if(distance<80.0&&forward>-0.55) desired=VisibilityTier::Reduced;
        else if(distance<150.0) desired=VisibilityTier::Minimal;
        if(desired==VisibilityTier::Dormant) continue;

        ++lastEvaluated_;
        const Vec3 presentationCenter{entity.position.x,entity.position.y+1.015,entity.position.z};
        const auto worldHit=world.raycastSegment(camera,presentationCenter);
        if(worldHit.hit&&worldHit.t<1.0-1e-9){
            ++lastOccluded_;
            continue;
        }
        entity.lineOfSight=true;
        const double score=distance+(1.0-forward)*6.0;
        candidates[candidateCount++]={i,desired,score};
    }

    // Fixed-capacity insertion sort: lower score wins, then lower stable entity id.
    for(std::size_t i=1;i<candidateCount;++i){
        const VisibilityCandidate key=candidates[i];
        std::size_t j=i;
        while(j>0){
            const auto& previous=candidates[j-1];
            const auto previousId=entities_[previous.index].id;
            const auto keyId=entities_[key.index].id;
            const bool keyBefore=(key.score<previous.score-1e-9)||
                                 (std::abs(key.score-previous.score)<=1e-9&&keyId<previousId);
            if(!keyBefore) break;
            candidates[j]=candidates[j-1];
            --j;
        }
        candidates[j]=key;
    }

    std::uint32_t full=0;
    std::uint32_t reduced=0;
    std::uint32_t minimal=0;
    for(std::size_t candidateIndex=0;candidateIndex<candidateCount;++candidateIndex){
        const auto candidate=candidates[candidateIndex];
        VisibilityTier assigned=VisibilityTier::Dormant;
        if(candidate.desired==VisibilityTier::Full&&full<kFullBudget){
            assigned=VisibilityTier::Full;
            ++full;
        }else if((candidate.desired==VisibilityTier::Full||candidate.desired==VisibilityTier::Reduced)&&
                 reduced<kReducedBudget){
            assigned=VisibilityTier::Reduced;
            ++reduced;
            if(candidate.desired==VisibilityTier::Full) ++lastBudgetDemotions_;
        }else if(minimal<kMinimalBudget){
            assigned=VisibilityTier::Minimal;
            ++minimal;
            if(candidate.desired!=VisibilityTier::Minimal) ++lastBudgetDemotions_;
        }else{
            ++lastBudgetDemotions_;
        }
        entities_[candidate.index].tier=assigned;
    }
}

VisibilityReport VisibilityCore::report() const noexcept {
    VisibilityReport out{};
    out.evaluated=lastEvaluated_;
    out.occluded=lastOccluded_;
    out.budgetDemotions=lastBudgetDemotions_;
    for(std::size_t i=0;i<count_;++i){
        switch(entities_[i].tier){
            case VisibilityTier::Full:++out.full;break;
            case VisibilityTier::Reduced:++out.reduced;break;
            case VisibilityTier::Minimal:++out.minimal;break;
            case VisibilityTier::Dormant:++out.dormant;break;
        }
    }
    return out;
}

bool VisibilityCore::validate() const noexcept {
    if(count_>kMaxEntities||lastEvaluated_>kMaxEntities||lastOccluded_>lastEvaluated_||
       lastBudgetDemotions_>kMaxEntities) return false;
    const auto summary=report();
    if(summary.full>kFullBudget||summary.reduced>kReducedBudget||summary.minimal>kMinimalBudget||
       summary.full+summary.reduced+summary.minimal+summary.dormant!=count_) return false;
    for(std::size_t i=0;i<count_;++i){
        const auto& entity=entities_[i];
        if(entity.id==0||!finiteVec(entity.position)||
           static_cast<std::uint8_t>(entity.tier)>static_cast<std::uint8_t>(VisibilityTier::Dormant)) return false;
        if(!entity.alive&&(entity.tier!=VisibilityTier::Dormant||entity.lineOfSight)) return false;
        if(entity.tier!=VisibilityTier::Dormant&&!entity.lineOfSight) return false;
    }
    return true;
}

} // namespace metse
