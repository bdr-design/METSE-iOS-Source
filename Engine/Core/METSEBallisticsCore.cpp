#include "METSEBallisticsCore.hpp"
#include "METSEWorldCollision.hpp"
#include "METSEDamageCore.hpp"
#include "METSEMaterialCore.hpp"
#include <algorithm>
#include <cmath>

namespace metse {
namespace {
double length(Vec3 v) noexcept { return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z); }
Vec3 normalize(Vec3 v) noexcept { const double l=length(v); return (std::isfinite(l)&&l>1e-12)?Vec3{v.x/l,v.y/l,v.z/l}:Vec3{}; }
double dot(Vec3 a,Vec3 b) noexcept { return a.x*b.x+a.y*b.y+a.z*b.z; }
Vec3 mul(Vec3 v,double s) noexcept { return {v.x*s,v.y*s,v.z*s}; }
Vec3 add(Vec3 a,Vec3 b) noexcept { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
}
void BallisticsCore::reset() noexcept { projectiles_={}; metrics_={}; }
bool BallisticsCore::spawn(const ShotSolution& shot) noexcept {
    if (!std::isfinite(shot.muzzleVelocity)||shot.muzzleVelocity<=0.0||!std::isfinite(shot.massKg)||shot.massKg<=0.0||shot.correlationId==0) {++metrics_.rejectedSpawns;return false;}
    for (auto& p:projectiles_) if(!p.active){p={true,shot.origin,{shot.direction.x*shot.muzzleVelocity,shot.direction.y*shot.muzzleVelocity,shot.direction.z*shot.muzzleVelocity},shot.massKg,0.0,shot.correlationId,0,0};++metrics_.spawned;return true;}
    ++metrics_.rejectedSpawns;return false;
}
void BallisticsCore::fixedStep(double dt,const WorldCollisionCore& world,DamageCore& damage) noexcept {
    if (!std::isfinite(dt)||dt<=0.0) return;
    for(auto& p:projectiles_){
        if(!p.active) continue;
        double speed=length(p.velocity);
        if(!std::isfinite(speed)||speed<10.0||p.ageSeconds>4.0){p.active=false;++metrics_.expired;continue;}

        // Atmospheric loss and gravity are integrated once per fixed simulation slice.
        const double drag=std::max(0.0,1.0-dt*0.035);
        p.velocity.x*=drag; p.velocity.z*=drag; p.velocity.y=p.velocity.y*drag-9.81*dt;
        double remainingDt=dt;
        std::uint8_t contactsThisStep=0;

        while(p.active && remainingDt>1e-6 && contactsThisStep<kMaxContactsPerStep){
            speed=length(p.velocity);
            if(!std::isfinite(speed)||speed<10.0){p.active=false;++metrics_.expired;break;}
            const Vec3 from=p.position;
            const Vec3 to={from.x+p.velocity.x*remainingDt,from.y+p.velocity.y*remainingDt,from.z+p.velocity.z*remainingDt};
            const double energy=0.5*p.massKg*speed*speed;
            const auto targetHit=damage.traceSegment(from,to);
            const auto worldHit=world.raycastSegment(from,to);

            // Collision truth is nearest-hit wins. A target can never be damaged through
            // a nearer world surface, and a farther wall never swallows a nearer target.
            if(targetHit.hit && (!worldHit.hit || targetHit.t <= worldHit.t + 1e-9)){
                const auto result=damage.applyIntersection(targetHit,energy,p.correlationId,normalize(p.velocity));
                if(result.hit){
                    p.position={from.x+(to.x-from.x)*targetHit.t,from.y+(to.y-from.y)*targetHit.t,from.z+(to.z-from.z)*targetHit.t};
                    p.active=false;++metrics_.impacts;++metrics_.targetImpacts;break;
                }
            }

            if(!worldHit.hit){p.position=to;remainingDt=0.0;break;}

            ++contactsThisStep;
            ++metrics_.impacts;
            ++metrics_.worldImpacts;
            const auto profile=MaterialCore::ballistic(worldHit.material);
            const double normalLength=length(worldHit.normal);
            const Vec3 velocityDir=normalize(p.velocity);
            const double normalCosine=normalLength>0.5?std::abs(dot(velocityDir,worldHit.normal)):1.0;

            const bool canPenetrate=profile.penetrable &&
                energy>=profile.penetrationThresholdJoules &&
                worldHit.thicknessMeters>1e-5 &&
                worldHit.thicknessMeters<=profile.maxPenetrationThicknessMeters+1e-9 &&
                worldHit.exitT>worldHit.t+1e-9 &&
                p.penetrations<kMaxPenetrationsPerProjectile;
            if(canPenetrate){
                const double depthRatio=std::clamp(worldHit.thicknessMeters/profile.maxPenetrationThicknessMeters,0.0,1.0);
                const double retention=1.0-(1.0-profile.retainedVelocityFraction)*depthRatio;
                p.velocity=mul(p.velocity,retention);
                ++p.penetrations;++metrics_.penetrations;
                const Vec3 after=normalize(p.velocity);
                p.position=add(worldHit.exitPoint,mul(after,0.01));
                remainingDt*=std::max(0.0,1.0-worldHit.exitT);
                continue;
            }

            const bool canRicochet=profile.ricochetEligible &&
                energy>=profile.ricochetMinEnergyJoules &&
                normalLength>0.5 &&
                normalCosine<=profile.ricochetMaxNormalCosine+1e-9 &&
                p.ricochets<kMaxRicochetsPerProjectile;
            if(canRicochet){
                const double velocityDotNormal=dot(p.velocity,worldHit.normal);
                Vec3 reflected={p.velocity.x-2.0*velocityDotNormal*worldHit.normal.x,
                                p.velocity.y-2.0*velocityDotNormal*worldHit.normal.y,
                                p.velocity.z-2.0*velocityDotNormal*worldHit.normal.z};
                reflected=mul(reflected,profile.ricochetRetainedVelocityFraction);
                if(length(reflected)>=10.0){
                    p.velocity=reflected;
                    ++p.ricochets;++metrics_.ricochets;
                    p.position=add(worldHit.point,mul(normalize(reflected),0.01));
                    remainingDt*=std::max(0.0,1.0-worldHit.t);
                    continue;
                }
            }

            ++metrics_.terminalWorldImpacts;
            p.position=worldHit.point;
            p.active=false;
            break;
        }
        if(p.active) p.ageSeconds+=dt;
    }
}
std::size_t BallisticsCore::activeCount() const noexcept {std::size_t n=0;for(const auto& p:projectiles_)if(p.active)++n;return n;}
bool BallisticsCore::validate() const noexcept {
    if(metrics_.terminalWorldImpacts>metrics_.worldImpacts||metrics_.penetrations>metrics_.worldImpacts||metrics_.ricochets>metrics_.worldImpacts)return false;
    for(WorldMaterial material:{WorldMaterial::Concrete,WorldMaterial::Steel,WorldMaterial::Wood,WorldMaterial::Brick,WorldMaterial::Glass,WorldMaterial::Soil,WorldMaterial::Rock})if(!MaterialCore::validateProfile(MaterialCore::ballistic(material)))return false;
    for(const auto& p:projectiles_)if(p.active){if(!std::isfinite(p.position.x)||!std::isfinite(p.position.y)||!std::isfinite(p.position.z)||!std::isfinite(p.velocity.x)||!std::isfinite(p.velocity.y)||!std::isfinite(p.velocity.z)||!std::isfinite(p.massKg)||p.massKg<=0.0||!std::isfinite(p.ageSeconds)||p.ageSeconds<0.0||p.correlationId==0||p.penetrations>kMaxPenetrationsPerProjectile||p.ricochets>kMaxRicochetsPerProjectile)return false;}return activeCount()<=kMaxProjectiles;
}
} // namespace metse
