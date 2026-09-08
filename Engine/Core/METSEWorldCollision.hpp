#pragma once
#include "METSEWeaponCore.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace metse {

enum class WorldMaterial : std::uint8_t { Concrete=0, Steel=1, Wood=2, Brick=3, Glass=4, Soil=5, Rock=6 };

struct WorldObstacle {
    double minX=0.0, minY=0.0, minZ=0.0;
    double maxX=0.0, maxY=0.0, maxZ=0.0;
    WorldMaterial material=WorldMaterial::Concrete;
};

struct CollisionResult {
    double x=0.0, z=0.0;
    bool hitX=false, hitZ=false;
    std::uint32_t contacts=0;
};

struct WorldRayHit {
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

// Cover candidates are derived once from authoritative world obstacle geometry.
// They are not authored gameplay points and do not create a second navigation/world SSOT.
struct WorldCoverCandidate {
    Vec3 position{};
    Vec3 outwardNormal{};
    std::size_t obstacleIndex=0;
    bool valid=false;
};

class WorldCollisionCore final {
public:
    static constexpr std::size_t kMaxObstacles=8;
    static constexpr std::size_t kMaxCoverCandidates=kMaxObstacles*4;

    WorldCollisionCore() noexcept;
    [[nodiscard]] CollisionResult resolve(double previousX,double previousZ,double desiredX,double desiredZ,double radius,double capsuleHeight) const noexcept;
    [[nodiscard]] WorldRayHit raycastSegment(Vec3 from,Vec3 to) const noexcept;
    [[nodiscard]] double clearanceHeightAt(double x,double z,double radius) const noexcept;
    [[nodiscard]] const std::array<WorldObstacle,kMaxObstacles>& obstacles() const noexcept { return obstacles_; }
    [[nodiscard]] std::size_t obstacleCount() const noexcept { return obstacleCount_; }
    [[nodiscard]] const std::array<WorldCoverCandidate,kMaxCoverCandidates>& coverCandidates() const noexcept { return coverCandidates_; }
    [[nodiscard]] std::size_t coverCandidateCount() const noexcept { return coverCandidateCount_; }
    [[nodiscard]] double minWorldX() const noexcept { return minWorldX_; }
    [[nodiscard]] double maxWorldX() const noexcept { return maxWorldX_; }
    [[nodiscard]] double minWorldZ() const noexcept { return minWorldZ_; }
    [[nodiscard]] double maxWorldZ() const noexcept { return maxWorldZ_; }
    [[nodiscard]] bool validate() const noexcept;

private:
    static bool overlaps(double value,double minimum,double maximum) noexcept;
    static double nearestBoundary(double previous,double desired,double minimum,double maximum) noexcept;
    static bool segmentAabb(Vec3 from,Vec3 to,const WorldObstacle& obstacle,double& tEntry,double& tExit,Vec3& normal) noexcept;
    void rebuildCoverCandidates() noexcept;

    std::array<WorldObstacle,kMaxObstacles> obstacles_{};
    std::size_t obstacleCount_=0;
    std::array<WorldCoverCandidate,kMaxCoverCandidates> coverCandidates_{};
    std::size_t coverCandidateCount_=0;
    double minWorldX_=-48.0, maxWorldX_=48.0, minWorldZ_=-48.0, maxWorldZ_=48.0;
};

} // namespace metse
