#include "METSEVisibilityCore.hpp"
#include <algorithm>
#include <cmath>
namespace metse {
void VisibilityCore::reset() noexcept{entities_={};count_=0;}
void VisibilityCore::syncTarget(std::size_t index,std::uint32_t id,Vec3 p,bool alive) noexcept{if(index>=kMaxEntities||id==0)return;entities_[index]={id,p,VisibilityTier::Dormant,alive};count_=std::max(count_,index+1);}
void VisibilityCore::update(Vec3 c,double yaw) noexcept{const double sx=std::sin(yaw),cz=std::cos(yaw);for(std::size_t i=0;i<count_;++i){auto&e=entities_[i];if(!e.alive){e.tier=VisibilityTier::Dormant;continue;}const double dx=e.position.x-c.x,dz=e.position.z-c.z,d=std::hypot(dx,dz);const double forward=(d>1e-6)?(dx*sx+dz*cz)/d:1.0;if(d<35.0&&forward>-0.25)e.tier=VisibilityTier::Full;else if(d<80.0&&forward>-0.55)e.tier=VisibilityTier::Reduced;else if(d<150.0)e.tier=VisibilityTier::Minimal;else e.tier=VisibilityTier::Dormant;}}
VisibilityReport VisibilityCore::report()const noexcept{VisibilityReport r{};for(std::size_t i=0;i<count_;++i)switch(entities_[i].tier){case VisibilityTier::Full:++r.full;break;case VisibilityTier::Reduced:++r.reduced;break;case VisibilityTier::Minimal:++r.minimal;break;case VisibilityTier::Dormant:++r.dormant;break;}return r;}
bool VisibilityCore::validate()const noexcept{if(count_>kMaxEntities)return false;for(std::size_t i=0;i<count_;++i){const auto&e=entities_[i];if(e.id==0||!std::isfinite(e.position.x)||!std::isfinite(e.position.y)||!std::isfinite(e.position.z))return false;}return true;}
} // namespace metse
