#include "METSEWorldCollision.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace metse {
namespace {

double segmentLength(Vec3 a,Vec3 b) noexcept {
    return std::sqrt((b.x-a.x)*(b.x-a.x)+(b.y-a.y)*(b.y-a.y)+(b.z-a.z)*(b.z-a.z));
}

bool finiteVec(Vec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

} // namespace

WorldCollisionCore::WorldCollisionCore() noexcept {
    // Build 008 / 009-A..E compatibility anchors. Never reorder or reshape these six.
    obstacles_[0]={6,0,12,10,2.8,17,WorldMaterial::Concrete};
    obstacles_[1]={-14,0,8,-12,2.2,24,WorldMaterial::Steel};
    obstacles_[2]={-4,0,24,3,3.4,31,WorldMaterial::Concrete};
    obstacles_[3]={14,0,-3,14.18,1.7,-1,WorldMaterial::Wood};
    obstacles_[4]={-20,0,-22,-11,3.0,-13,WorldMaterial::Concrete};
    obstacles_[5]={-2.8,1.34,6.0,2.8,1.65,10.5,WorldMaterial::Steel};

    // Build 009-F battlefield geometry.
    obstacles_[6]={-38,0,14,-30,4.5,28,WorldMaterial::Brick};
    obstacles_[7]={-28,0,30,-18,5.0,40,WorldMaterial::Concrete};
    obstacles_[8]={-40,0,-6,-34,2.8,4,WorldMaterial::Glass};
    obstacles_[9]={-31,0,-10,-29,2.2,6,WorldMaterial::Brick};
    obstacles_[10]={26,0,-30,34,2.6,-24,WorldMaterial::Steel};
    obstacles_[11]={20,0,-20,28,2.6,-14,WorldMaterial::Steel};
    obstacles_[12]={32,0,-12,43,4.2,2,WorldMaterial::Concrete};
    obstacles_[13]={21,0,4,25,2.6,9,WorldMaterial::Glass};
    obstacles_[14]={30,0,10,30.18,1.9,18,WorldMaterial::Wood};
    obstacles_[15]={-34,0,-34,-28,2.4,-29,WorldMaterial::Rock};
    obstacles_[16]={-24,0,-38,-18,3.0,-32,WorldMaterial::Rock};
    obstacles_[17]={-8,0,-34,2,1.4,-31,WorldMaterial::Soil};
    obstacles_[18]={8,0,-38,14,2.1,-34,WorldMaterial::Rock};
    obstacles_[19]={11,0,28,17,2.4,32,WorldMaterial::Brick};
    obstacleCount_=kMaxObstacles;

    // Build 009-G traversable surface semantics. Small precise patches come first
    // and therefore override broader sector patches. Areas outside every patch are Soil.
    surfacePatches_[0]={25.0,-33.0,35.0,-21.0,WorldMaterial::Steel};
    surfacePatches_[1]={-46.0,8.0,-16.0,46.0,WorldMaterial::Concrete};
    surfacePatches_[2]={18.0,-34.0,46.0,22.0,WorldMaterial::Concrete};
    surfacePatches_[3]={-46.0,-46.0,18.0,-24.0,WorldMaterial::Rock};
    surfacePatches_[4]={-10.0,20.0,18.0,44.0,WorldMaterial::Concrete};
    surfacePatches_[5]={-8.0,-10.0,8.0,12.0,WorldMaterial::Soil};
    surfacePatchCount_=kMaxSurfacePatches;

    rebuildCoverCandidates();
}

bool WorldCollisionCore::overlaps(double value,double minimum,double maximum) noexcept {
    return value>minimum && value<maximum;
}

double WorldCollisionCore::nearestBoundary(double previous,double desired,double minimum,double maximum) noexcept {
    if(previous<=minimum) return minimum;
    if(previous>=maximum) return maximum;
    return std::abs(desired-minimum)<=std::abs(desired-maximum)?minimum:maximum;
}

CollisionResult WorldCollisionCore::resolve(double previousX,double previousZ,double desiredX,double desiredZ,double radius,double capsuleHeight) const noexcept {
    CollisionResult out{};
    if(!std::isfinite(previousX)||!std::isfinite(previousZ)||!std::isfinite(desiredX)||!std::isfinite(desiredZ)||
       !std::isfinite(radius)||!std::isfinite(capsuleHeight)||radius<=0.0||capsuleHeight<=0.0){
        out.x=previousX;
        out.z=previousZ;
        return out;
    }
    const double minX=minWorldX_+radius,maxX=maxWorldX_-radius,minZ=minWorldZ_+radius,maxZ=maxWorldZ_-radius;
    out.x=std::clamp(desiredX,minX,maxX);
    out.z=std::clamp(desiredZ,minZ,maxZ);
    if(out.x!=desiredX){out.hitX=true;++out.contacts;}
    if(out.z!=desiredZ){out.hitZ=true;++out.contacts;}

    for(std::size_t i=0;i<obstacleCount_;++i){
        const auto& obstacle=obstacles_[i];
        const bool verticalOverlap=(0.0<obstacle.maxY&&capsuleHeight>obstacle.minY);
        if(!verticalOverlap) continue;
        const double minObstacleX=obstacle.minX-radius,maxObstacleX=obstacle.maxX+radius;
        const double minObstacleZ=obstacle.minZ-radius,maxObstacleZ=obstacle.maxZ+radius;
        if(overlaps(out.z,minObstacleZ,maxObstacleZ)&&overlaps(out.x,minObstacleX,maxObstacleX)){
            out.x=std::clamp(nearestBoundary(previousX,out.x,minObstacleX,maxObstacleX),minX,maxX);
            out.hitX=true;
            ++out.contacts;
        }
    }
    for(std::size_t i=0;i<obstacleCount_;++i){
        const auto& obstacle=obstacles_[i];
        const bool verticalOverlap=(0.0<obstacle.maxY&&capsuleHeight>obstacle.minY);
        if(!verticalOverlap) continue;
        const double minObstacleX=obstacle.minX-radius,maxObstacleX=obstacle.maxX+radius;
        const double minObstacleZ=obstacle.minZ-radius,maxObstacleZ=obstacle.maxZ+radius;
        if(overlaps(out.x,minObstacleX,maxObstacleX)&&overlaps(out.z,minObstacleZ,maxObstacleZ)){
            out.z=std::clamp(nearestBoundary(previousZ,out.z,minObstacleZ,maxObstacleZ),minZ,maxZ);
            out.hitZ=true;
            ++out.contacts;
        }
    }
    return out;
}

double WorldCollisionCore::clearanceHeightAt(double x,double z,double radius) const noexcept {
    double clearance=std::numeric_limits<double>::infinity();
    for(std::size_t i=0;i<obstacleCount_;++i){
        const auto& obstacle=obstacles_[i];
        if(obstacle.minY<=0.01) continue;
        if(x>obstacle.minX-radius&&x<obstacle.maxX+radius&&z>obstacle.minZ-radius&&z<obstacle.maxZ+radius)
            clearance=std::min(clearance,obstacle.minY);
    }
    return clearance;
}

WorldMaterial WorldCollisionCore::surfaceMaterialAt(double x,double z) const noexcept {
    if(!std::isfinite(x)||!std::isfinite(z)) return WorldMaterial::Soil;
    for(std::size_t i=0;i<surfacePatchCount_;++i){
        const auto& patch=surfacePatches_[i];
        if(x>=patch.minX&&x<=patch.maxX&&z>=patch.minZ&&z<=patch.maxZ) return patch.material;
    }
    return WorldMaterial::Soil;
}

bool WorldCollisionCore::hasOverheadCover(Vec3 position,double maxHeightMeters) const noexcept {
    if(!finiteVec(position)||!std::isfinite(maxHeightMeters)||maxHeightMeters<=0.05) return false;
    const Vec3 top{position.x,position.y+maxHeightMeters,position.z};
    const auto hit=raycastSegment(position,top);
    return hit.hit && hit.point.y>position.y+0.02;
}

bool WorldCollisionCore::segmentAabb(Vec3 a,Vec3 b,const WorldObstacle& obstacle,double& tEntry,double& tExit,Vec3& normal) noexcept {
    const Vec3 delta{b.x-a.x,b.y-a.y,b.z-a.z};
    double tMin=0.0,tMax=1.0;
    Vec3 enterNormal{};
    auto axis=[&](double origin,double direction,double minimum,double maximum,Vec3 negativeFaceNormal,Vec3 positiveFaceNormal){
        if(std::abs(direction)<1e-10) return origin>=minimum&&origin<=maximum;
        const double inverse=1.0/direction;
        double t1=(minimum-origin)*inverse,t2=(maximum-origin)*inverse;
        Vec3 n1=negativeFaceNormal,n2=positiveFaceNormal;
        if(t1>t2){std::swap(t1,t2);std::swap(n1,n2);}
        if(t1>tMin){tMin=t1;enterNormal=n1;}
        tMax=std::min(tMax,t2);
        return tMin<=tMax;
    };
    if(!axis(a.x,delta.x,obstacle.minX,obstacle.maxX,{-1,0,0},{1,0,0})||
       !axis(a.y,delta.y,obstacle.minY,obstacle.maxY,{0,-1,0},{0,1,0})||
       !axis(a.z,delta.z,obstacle.minZ,obstacle.maxZ,{0,0,-1},{0,0,1})) return false;
    if(tMax<0.0||tMin>1.0) return false;
    tEntry=std::clamp(tMin,0.0,1.0);
    tExit=std::clamp(tMax,tEntry,1.0);
    normal=enterNormal;
    return true;
}

WorldRayHit WorldCollisionCore::raycastSegment(Vec3 a,Vec3 b) const noexcept {
    WorldRayHit best{};
    best.t=2.0;
    const double length=segmentLength(a,b);
    for(std::size_t i=0;i<obstacleCount_;++i){
        double entry=0.0,exit=0.0;
        Vec3 normal{};
        if(segmentAabb(a,b,obstacles_[i],entry,exit,normal)&&entry<best.t){
            best.hit=true;
            best.t=entry;
            best.exitT=exit;
            best.point={a.x+(b.x-a.x)*entry,a.y+(b.y-a.y)*entry,a.z+(b.z-a.z)*entry};
            best.exitPoint={a.x+(b.x-a.x)*exit,a.y+(b.y-a.y)*exit,a.z+(b.z-a.z)*exit};
            best.normal=normal;
            best.material=obstacles_[i].material;
            best.obstacleIndex=i;
            best.thicknessMeters=length*std::max(0.0,exit-entry);
        }
    }
    if(a.y>=0.0&&b.y<0.0){
        const double t=a.y/(a.y-b.y);
        if(t<best.t){
            const Vec3 point{a.x+(b.x-a.x)*t,0.0,a.z+(b.z-a.z)*t};
            best={true,t,t,point,point,{0,1,0},WorldMaterial::Soil,kMaxObstacles,0.0};
        }
    }
    return best;
}

void WorldCollisionCore::rebuildCoverCandidates() noexcept {
    coverCandidates_={};
    coverCandidateCount_=0;
    constexpr double kAgentRadius=0.34;
    constexpr double kCoverGap=0.24;
    constexpr double kCandidateOffset=kAgentRadius+kCoverGap;
    constexpr double kStandingCapsuleHeight=1.72;

    auto append=[&](std::size_t obstacleIndex,Vec3 position,Vec3 outwardNormal){
        if(coverCandidateCount_>=kMaxCoverCandidates) return;
        if(position.x<=minWorldX_+kAgentRadius||position.x>=maxWorldX_-kAgentRadius||
           position.z<=minWorldZ_+kAgentRadius||position.z>=maxWorldZ_-kAgentRadius) return;
        const double clearance=clearanceHeightAt(position.x,position.z,kAgentRadius);
        if(std::isfinite(clearance)&&clearance<kStandingCapsuleHeight+0.02) return;
        const auto resolved=resolve(position.x,position.z,position.x,position.z,kAgentRadius,kStandingCapsuleHeight);
        if(std::abs(resolved.x-position.x)>1e-8||std::abs(resolved.z-position.z)>1e-8) return;
        coverCandidates_[coverCandidateCount_++]={position,outwardNormal,obstacleIndex,true};
    };

    for(std::size_t i=0;i<obstacleCount_;++i){
        const auto& obstacle=obstacles_[i];
        if(obstacle.minY>0.05||obstacle.maxY<1.15) continue;
        const double centerX=(obstacle.minX+obstacle.maxX)*0.5;
        const double centerZ=(obstacle.minZ+obstacle.maxZ)*0.5;
        append(i,{obstacle.minX-kCandidateOffset,0.0,centerZ},{-1.0,0.0,0.0});
        append(i,{obstacle.maxX+kCandidateOffset,0.0,centerZ},{1.0,0.0,0.0});
        append(i,{centerX,0.0,obstacle.minZ-kCandidateOffset},{0.0,0.0,-1.0});
        append(i,{centerX,0.0,obstacle.maxZ+kCandidateOffset},{0.0,0.0,1.0});
    }
}

bool WorldCollisionCore::validate() const noexcept {
    if(!std::isfinite(minWorldX_)||!std::isfinite(maxWorldX_)||!std::isfinite(minWorldZ_)||!std::isfinite(maxWorldZ_)||
       minWorldX_>=maxWorldX_||minWorldZ_>=maxWorldZ_||obstacleCount_>kMaxObstacles||surfacePatchCount_>kMaxSurfacePatches||
       coverCandidateCount_>kMaxCoverCandidates) return false;
    if(obstacleCount_<kLegacyObstacleCount) return false;
    for(std::size_t i=0;i<obstacleCount_;++i){
        const auto& obstacle=obstacles_[i];
        if(!std::isfinite(obstacle.minX)||!std::isfinite(obstacle.minY)||!std::isfinite(obstacle.minZ)||
           !std::isfinite(obstacle.maxX)||!std::isfinite(obstacle.maxY)||!std::isfinite(obstacle.maxZ)||
           obstacle.minX>=obstacle.maxX||obstacle.minY>=obstacle.maxY||obstacle.minZ>=obstacle.maxZ) return false;
        if(obstacle.minX<=minWorldX_||obstacle.maxX>=maxWorldX_||obstacle.minZ<=minWorldZ_||obstacle.maxZ>=maxWorldZ_) return false;
        if(static_cast<std::uint8_t>(obstacle.material)>static_cast<std::uint8_t>(WorldMaterial::Rock)) return false;
    }
    for(std::size_t i=0;i<surfacePatchCount_;++i){
        const auto& patch=surfacePatches_[i];
        if(!std::isfinite(patch.minX)||!std::isfinite(patch.minZ)||!std::isfinite(patch.maxX)||!std::isfinite(patch.maxZ)||
           patch.minX>=patch.maxX||patch.minZ>=patch.maxZ) return false;
        if(patch.minX<minWorldX_||patch.maxX>maxWorldX_||patch.minZ<minWorldZ_||patch.maxZ>maxWorldZ_) return false;
        if(static_cast<std::uint8_t>(patch.material)>static_cast<std::uint8_t>(WorldMaterial::Rock)) return false;
    }
    for(std::size_t i=0;i<coverCandidateCount_;++i){
        const auto& candidate=coverCandidates_[i];
        if(!candidate.valid||candidate.obstacleIndex>=obstacleCount_||!finiteVec(candidate.position)||!finiteVec(candidate.outwardNormal)) return false;
        const double normalLength=std::hypot(candidate.outwardNormal.x,candidate.outwardNormal.z);
        if(std::abs(normalLength-1.0)>1e-8||std::abs(candidate.outwardNormal.y)>1e-9) return false;
        if(candidate.position.x<=minWorldX_||candidate.position.x>=maxWorldX_||candidate.position.z<=minWorldZ_||candidate.position.z>=maxWorldZ_) return false;
        const auto& obstacle=obstacles_[candidate.obstacleIndex];
        if(obstacle.minY>0.05||obstacle.maxY<1.15) return false;
    }
    for(std::size_t i=coverCandidateCount_;i<kMaxCoverCandidates;++i){
        if(coverCandidates_[i].valid) return false;
    }
    return true;
}

} // namespace metse
