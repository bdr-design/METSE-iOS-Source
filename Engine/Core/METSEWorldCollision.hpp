#pragma once
#include "METSEWeaponCore.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
namespace metse {
enum class WorldMaterial:std::uint8_t{Concrete=0,Steel=1,Wood=2,Brick=3,Glass=4,Soil=5,Rock=6};
struct WorldObstacle{double minX=0,minY=0,minZ=0,maxX=0,maxY=0,maxZ=0;WorldMaterial material=WorldMaterial::Concrete;};
struct CollisionResult{double x=0,z=0;bool hitX=false,hitZ=false;std::uint32_t contacts=0;};
struct WorldRayHit{
    bool hit=false;
    double t=1.0;
    double exitT=1.0;
    Vec3 point{};
    Vec3 exitPoint{};
    Vec3 normal{};
    WorldMaterial material=WorldMaterial::Concrete;
    std::size_t obstacleIndex=0;
    double thicknessMeters=0.0;
};
class WorldCollisionCore final{
public:static constexpr std::size_t kMaxObstacles=8;WorldCollisionCore()noexcept;
[[nodiscard]] CollisionResult resolve(double previousX,double previousZ,double desiredX,double desiredZ,double radius,double capsuleHeight)const noexcept;
[[nodiscard]] WorldRayHit raycastSegment(Vec3 from,Vec3 to)const noexcept;
[[nodiscard]] double clearanceHeightAt(double x,double z,double radius)const noexcept;
[[nodiscard]] const std::array<WorldObstacle,kMaxObstacles>& obstacles()const noexcept{return obstacles_;}
[[nodiscard]] std::size_t obstacleCount()const noexcept{return obstacleCount_;}
[[nodiscard]] double minWorldX()const noexcept{return minWorldX_;}[[nodiscard]] double maxWorldX()const noexcept{return maxWorldX_;}[[nodiscard]] double minWorldZ()const noexcept{return minWorldZ_;}[[nodiscard]] double maxWorldZ()const noexcept{return maxWorldZ_;}
[[nodiscard]] bool validate()const noexcept;
private:static bool overlaps(double v,double mn,double mx)noexcept;static double nearestBoundary(double previous,double desired,double mn,double mx)noexcept;static bool segmentAabb(Vec3 a,Vec3 b,const WorldObstacle&o,double&tEntry,double&tExit,Vec3&normal)noexcept;
std::array<WorldObstacle,kMaxObstacles> obstacles_{};std::size_t obstacleCount_=0;double minWorldX_=-48,maxWorldX_=48,minWorldZ_=-48,maxWorldZ_=48;};
} // namespace metse
