#include "METSEWorldCollision.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace metse {
namespace {double segmentLength(Vec3 a,Vec3 b)noexcept{return std::sqrt((b.x-a.x)*(b.x-a.x)+(b.y-a.y)*(b.y-a.y)+(b.z-a.z)*(b.z-a.z));}}
WorldCollisionCore::WorldCollisionCore()noexcept{
obstacles_[0]={6,0,12,10,2.8,17,WorldMaterial::Concrete};
obstacles_[1]={-14,0,8,-12,2.2,24,WorldMaterial::Steel};
obstacles_[2]={-4,0,24,3,3.4,31,WorldMaterial::Concrete};
obstacles_[3]={14,0,-3,26,1.7,-1,WorldMaterial::Wood};
obstacles_[4]={-20,0,-22,-11,3.0,-13,WorldMaterial::Concrete};
// Low overhead: crouch/prone can pass, standing cannot.
obstacles_[5]={-2.8,1.34,6.0,2.8,1.65,10.5,WorldMaterial::Steel};
// Build 009 material lanes: deliberately thin test walls kept away from Aim Truth targets.
obstacles_[6]={28.0,0,-22,28.12,2.5,-17,WorldMaterial::Brick};
obstacles_[7]={-32.0,0,14,-31.96,2.2,20,WorldMaterial::Glass};
obstacleCount_=8;
}
bool WorldCollisionCore::overlaps(double v,double mn,double mx)noexcept{return v>mn&&v<mx;}
double WorldCollisionCore::nearestBoundary(double p,double d,double mn,double mx)noexcept{if(p<=mn)return mn;if(p>=mx)return mx;return std::abs(d-mn)<=std::abs(d-mx)?mn:mx;}
CollisionResult WorldCollisionCore::resolve(double px,double pz,double dx,double dz,double r,double h)const noexcept{CollisionResult out{};if(!std::isfinite(px)||!std::isfinite(pz)||!std::isfinite(dx)||!std::isfinite(dz)||!std::isfinite(r)||!std::isfinite(h)||r<=0||h<=0){out.x=px;out.z=pz;return out;}const double minX=minWorldX_+r,maxX=maxWorldX_-r,minZ=minWorldZ_+r,maxZ=maxWorldZ_-r;out.x=std::clamp(dx,minX,maxX);out.z=std::clamp(dz,minZ,maxZ);if(out.x!=dx){out.hitX=true;++out.contacts;}if(out.z!=dz){out.hitZ=true;++out.contacts;}
for(std::size_t i=0;i<obstacleCount_;++i){const auto&o=obstacles_[i];const bool verticalOverlap=(0.0<o.maxY&&h>o.minY);if(!verticalOverlap)continue;double mnx=o.minX-r,mxx=o.maxX+r,mnz=o.minZ-r,mxz=o.maxZ+r;if(overlaps(out.z,mnz,mxz)&&overlaps(out.x,mnx,mxx)){out.x=std::clamp(nearestBoundary(px,out.x,mnx,mxx),minX,maxX);out.hitX=true;++out.contacts;}}
for(std::size_t i=0;i<obstacleCount_;++i){const auto&o=obstacles_[i];const bool verticalOverlap=(0.0<o.maxY&&h>o.minY);if(!verticalOverlap)continue;double mnx=o.minX-r,mxx=o.maxX+r,mnz=o.minZ-r,mxz=o.maxZ+r;if(overlaps(out.x,mnx,mxx)&&overlaps(out.z,mnz,mxz)){out.z=std::clamp(nearestBoundary(pz,out.z,mnz,mxz),minZ,maxZ);out.hitZ=true;++out.contacts;}}
return out;}

double WorldCollisionCore::clearanceHeightAt(double x,double z,double r)const noexcept{double clearance=std::numeric_limits<double>::infinity();for(std::size_t i=0;i<obstacleCount_;++i){const auto&o=obstacles_[i];if(o.minY<=0.01)continue;if(x>o.minX-r&&x<o.maxX+r&&z>o.minZ-r&&z<o.maxZ+r)clearance=std::min(clearance,o.minY);}return clearance;}

bool WorldCollisionCore::segmentAabb(Vec3 a,Vec3 b,const WorldObstacle&o,double&tEntry,double&tExit,Vec3&normal)noexcept{
const Vec3 d{b.x-a.x,b.y-a.y,b.z-a.z};double tmin=0.0,tmax=1.0;Vec3 enterNormal{};
auto axis=[&](double origin,double dir,double mn,double mx,Vec3 negativeFaceNormal,Vec3 positiveFaceNormal){
    if(std::abs(dir)<1e-10)return origin>=mn&&origin<=mx;
    double inv=1.0/dir,t1=(mn-origin)*inv,t2=(mx-origin)*inv;Vec3 n1=negativeFaceNormal,n2=positiveFaceNormal;
    if(t1>t2){std::swap(t1,t2);std::swap(n1,n2);}if(t1>tmin){tmin=t1;enterNormal=n1;}tmax=std::min(tmax,t2);return tmin<=tmax;
};
if(!axis(a.x,d.x,o.minX,o.maxX,{-1,0,0},{1,0,0})||!axis(a.y,d.y,o.minY,o.maxY,{0,-1,0},{0,1,0})||!axis(a.z,d.z,o.minZ,o.maxZ,{0,0,-1},{0,0,1}))return false;
if(tmax<0.0||tmin>1.0)return false;tEntry=std::clamp(tmin,0.0,1.0);tExit=std::clamp(tmax,tEntry,1.0);normal=enterNormal;return true;
}
WorldRayHit WorldCollisionCore::raycastSegment(Vec3 a,Vec3 b)const noexcept{WorldRayHit best{};best.t=2.0;const double length=segmentLength(a,b);for(std::size_t i=0;i<obstacleCount_;++i){double entry=0,exit=0;Vec3 normal{};if(segmentAabb(a,b,obstacles_[i],entry,exit,normal)&&entry<best.t){best.hit=true;best.t=entry;best.exitT=exit;best.point={a.x+(b.x-a.x)*entry,a.y+(b.y-a.y)*entry,a.z+(b.z-a.z)*entry};best.exitPoint={a.x+(b.x-a.x)*exit,a.y+(b.y-a.y)*exit,a.z+(b.z-a.z)*exit};best.normal=normal;best.material=obstacles_[i].material;best.obstacleIndex=i;best.thicknessMeters=length*std::max(0.0,exit-entry);}}
// Ground is a terminal surface for the current battlefield prototype. It is Soil so
// material telemetry remains truthful even though the terrain mesh is procedural.
if(a.y>=0.0&&b.y<0.0){double t=a.y/(a.y-b.y);if(t<best.t){Vec3 p{a.x+(b.x-a.x)*t,0.0,a.z+(b.z-a.z)*t};best={true,t,t,p,p,{0,1,0},WorldMaterial::Soil,kMaxObstacles,0.0};}}
return best;}
bool WorldCollisionCore::validate()const noexcept{if(!std::isfinite(minWorldX_)||!std::isfinite(maxWorldX_)||!std::isfinite(minWorldZ_)||!std::isfinite(maxWorldZ_)||minWorldX_>=maxWorldX_||minWorldZ_>=maxWorldZ_||obstacleCount_>kMaxObstacles)return false;for(std::size_t i=0;i<obstacleCount_;++i){const auto&o=obstacles_[i];if(!std::isfinite(o.minX)||!std::isfinite(o.minY)||!std::isfinite(o.minZ)||!std::isfinite(o.maxX)||!std::isfinite(o.maxY)||!std::isfinite(o.maxZ)||o.minX>=o.maxX||o.minY>=o.maxY||o.minZ>=o.maxZ)return false;if(o.minX<=minWorldX_||o.maxX>=maxWorldX_||o.minZ<=minWorldZ_||o.maxZ>=maxWorldZ_)return false;if(static_cast<std::uint8_t>(o.material)>static_cast<std::uint8_t>(WorldMaterial::Rock))return false;}return true;}
} // namespace metse
