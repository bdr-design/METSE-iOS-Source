#include "METSEDamageCore.hpp"
#include <algorithm>
#include <cmath>

namespace metse {

DamageCore::DamageCore() noexcept { reset(); }
void DamageCore::reset() noexcept {
    targets_ = {};
    // Spawn targets are intentionally placed on unobstructed sight lines from the
    // default camera so the renderer and ballistics truth can be validated on device.
    targets_[0] = {1,{-10.0,0.0,18.0},100.0,0.36,true};
    targets_[1] = {2,{22.0,0.0,26.0},100.0,0.36,true};
    targetCount_ = 2;
    totalHits_ = 0;
    totalKills_ = 0;
    lastResult_ = {};
    resultSequence_ = 0;
}

double DamageCore::segmentPointDistanceXZ(const Vec3& a,const Vec3& b,const Vec3& p,double& t) noexcept {
    const double dx=b.x-a.x, dz=b.z-a.z;
    const double denom=dx*dx+dz*dz;
    if (denom <= 1e-12) { t=0.0; return std::hypot(p.x-a.x,p.z-a.z); }
    t=std::clamp(((p.x-a.x)*dx+(p.z-a.z)*dz)/denom,0.0,1.0);
    return std::hypot(a.x+dx*t-p.x,a.z+dz*t-p.z);
}

DamageIntersection DamageCore::traceSegment(const Vec3& from,const Vec3& to) const noexcept {
    DamageIntersection best{};
    for (std::size_t i=0;i<targetCount_;++i) {
        const auto& target=targets_[i];
        if (!target.alive) continue;
        double t=0.0;
        if (segmentPointDistanceXZ(from,to,target.position,t)>target.radius || t>=best.t) continue;
        const double hitY=from.y+(to.y-from.y)*t-target.position.y;
        if (hitY < 0.15 || hitY > 1.88) continue;
        HitRegion region=HitRegion::Limb;
        if (hitY >= 1.55) region=HitRegion::Head;
        else if (hitY >= 1.02) region=HitRegion::Thorax;
        else if (hitY >= 0.66) region=HitRegion::Abdomen;
        best={true,t,i,target.id,region};
    }
    return best;
}

DamageResult DamageCore::applyIntersection(const DamageIntersection& hit,double energy,std::uint64_t correlationId) noexcept {
    DamageResult out{};
    if (!hit.hit || hit.targetIndex>=targetCount_ || !std::isfinite(energy) || energy<=0.0 || correlationId==0) return out;
    auto& target=targets_[hit.targetIndex];
    if (!target.alive || target.id!=hit.targetId) return out;
    double multiplier=0.40, armorThreshold=0.0;
    if (hit.region==HitRegion::Head) { multiplier=1.35; armorThreshold=320.0; }
    else if (hit.region==HitRegion::Thorax) { multiplier=0.78; armorThreshold=610.0; }
    else if (hit.region==HitRegion::Abdomen) multiplier=0.62;
    const double effectiveEnergy=std::max(0.0,energy-armorThreshold);
    double damage=std::clamp(effectiveEnergy/8.0*multiplier,4.0,130.0);
    if (armorThreshold>0.0 && effectiveEnergy<=0.0) damage=std::min(damage,8.0);
    target.health=std::max(0.0,target.health-damage);
    const bool killed=target.health<=0.0;
    target.alive=!killed;
    ++totalHits_; if(killed)++totalKills_;
    out={true,killed,target.id,hit.region,damage,target.health,correlationId};
    lastResult_=out; ++resultSequence_;
    return out;
}

DamageResult DamageCore::applySegment(const Vec3& from,const Vec3& to,double energy,std::uint64_t correlationId) noexcept {
    return applyIntersection(traceSegment(from,to),energy,correlationId);
}

bool DamageCore::validate() const noexcept {
    if (targetCount_>kMaxTargets) return false;
    for (std::size_t i=0;i<targetCount_;++i) {
        const auto& t=targets_[i];
        if (t.id==0 || !std::isfinite(t.position.x)||!std::isfinite(t.position.y)||!std::isfinite(t.position.z)||
            !std::isfinite(t.health)||t.health<0.0||t.health>100.0001||!std::isfinite(t.radius)||t.radius<=0.0) return false;
        if (t.alive != (t.health>0.0)) return false;
    }
    return true;
}

} // namespace metse
