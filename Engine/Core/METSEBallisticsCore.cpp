#include "METSEBallisticsCore.hpp"
#include "METSEWorldCollision.hpp"
#include "METSEDamageCore.hpp"
#include <algorithm>
#include <cmath>

namespace metse {
namespace { double length(Vec3 v){return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);} }
void BallisticsCore::reset() noexcept { projectiles_={}; metrics_={}; }
bool BallisticsCore::spawn(const ShotSolution& shot) noexcept {
    if (!std::isfinite(shot.muzzleVelocity)||shot.muzzleVelocity<=0.0||!std::isfinite(shot.massKg)||shot.massKg<=0.0||shot.correlationId==0) {++metrics_.rejectedSpawns;return false;}
    for (auto& p:projectiles_) if(!p.active){p={true,shot.origin,{shot.direction.x*shot.muzzleVelocity,shot.direction.y*shot.muzzleVelocity,shot.direction.z*shot.muzzleVelocity},shot.massKg,0.0,shot.correlationId,0};++metrics_.spawned;return true;}
    ++metrics_.rejectedSpawns;return false;
}
void BallisticsCore::fixedStep(double dt,const WorldCollisionCore& world,DamageCore& damage) noexcept {
    if (!std::isfinite(dt)||dt<=0.0) return;
    for(auto& p:projectiles_){ if(!p.active) continue;
        Vec3 from=p.position;
        const double speed=length(p.velocity);
        if(!std::isfinite(speed)||speed<10.0||p.ageSeconds>4.0){p.active=false;++metrics_.expired;continue;}
        const double drag=std::max(0.0,1.0-dt*0.035);
        p.velocity.x*=drag; p.velocity.z*=drag; p.velocity.y=p.velocity.y*drag-9.81*dt;
        Vec3 to={from.x+p.velocity.x*dt,from.y+p.velocity.y*dt,from.z+p.velocity.z*dt};
        const double energy=0.5*p.massKg*speed*speed;
        auto damageResult=damage.applySegment(from,to,energy,p.correlationId);
        if(damageResult.hit){p.position=to;p.active=false;++metrics_.impacts;++metrics_.targetImpacts;continue;}
        const auto hit=world.raycastSegment(from,to);
        if(hit.hit){++metrics_.impacts;++metrics_.worldImpacts;
            if(hit.material==WorldMaterial::Wood && energy>420.0 && p.penetrations<1){++p.penetrations;++metrics_.penetrations;p.velocity.x*=0.58;p.velocity.y*=0.58;p.velocity.z*=0.58;p.position={hit.point.x+p.velocity.x*0.0008,hit.point.y+p.velocity.y*0.0008,hit.point.z+p.velocity.z*0.0008};continue;}
            p.position=hit.point;p.active=false;continue;
        }
        p.position=to;p.ageSeconds+=dt;
    }
}
std::size_t BallisticsCore::activeCount() const noexcept {std::size_t n=0;for(const auto& p:projectiles_)if(p.active)++n;return n;}
bool BallisticsCore::validate() const noexcept {for(const auto& p:projectiles_)if(p.active){if(!std::isfinite(p.position.x)||!std::isfinite(p.position.y)||!std::isfinite(p.position.z)||!std::isfinite(p.velocity.x)||!std::isfinite(p.velocity.y)||!std::isfinite(p.velocity.z)||!std::isfinite(p.massKg)||p.massKg<=0.0||!std::isfinite(p.ageSeconds)||p.ageSeconds<0.0||p.correlationId==0)return false;}return activeCount()<=kMaxProjectiles;}
} // namespace metse
