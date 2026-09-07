#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace metse {

struct WorldObstacle {
    double minX = 0.0;
    double minZ = 0.0;
    double maxX = 0.0;
    double maxZ = 0.0;
    double height = 2.4;
};

struct CollisionResult {
    double x = 0.0;
    double z = 0.0;
    bool hitX = false;
    bool hitZ = false;
    std::uint32_t contacts = 0;
};

class WorldCollisionCore final {
public:
    static constexpr std::size_t kMaxObstacles = 6;

    WorldCollisionCore() noexcept;

    [[nodiscard]] CollisionResult resolve(double previousX,
                                          double previousZ,
                                          double desiredX,
                                          double desiredZ,
                                          double radius) const noexcept;

    [[nodiscard]] const std::array<WorldObstacle, kMaxObstacles>& obstacles() const noexcept { return obstacles_; }
    [[nodiscard]] std::size_t obstacleCount() const noexcept { return obstacleCount_; }
    [[nodiscard]] double minWorldX() const noexcept { return minWorldX_; }
    [[nodiscard]] double maxWorldX() const noexcept { return maxWorldX_; }
    [[nodiscard]] double minWorldZ() const noexcept { return minWorldZ_; }
    [[nodiscard]] double maxWorldZ() const noexcept { return maxWorldZ_; }
    [[nodiscard]] bool validate() const noexcept;

private:
    static bool overlaps(double value, double minValue, double maxValue) noexcept;
    static double nearestBoundary(double previous,
                                  double desired,
                                  double minValue,
                                  double maxValue) noexcept;

    std::array<WorldObstacle, kMaxObstacles> obstacles_{};
    std::size_t obstacleCount_ = 0;
    double minWorldX_ = -48.0;
    double maxWorldX_ = 48.0;
    double minWorldZ_ = -48.0;
    double maxWorldZ_ = 48.0;
};

} // namespace metse
