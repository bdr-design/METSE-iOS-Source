#include "METSEWorldCollision.hpp"

#include <algorithm>
#include <cmath>

namespace metse {

WorldCollisionCore::WorldCollisionCore() noexcept {
    obstacles_[0] = { 6.0, 12.0, 10.0, 17.0, 2.8 };
    obstacles_[1] = { -14.0, 8.0, -12.0, 24.0, 2.2 };
    obstacles_[2] = { -4.0, 24.0, 3.0, 31.0, 3.4 };
    obstacles_[3] = { 14.0, -3.0, 26.0, -1.0, 1.7 };
    obstacles_[4] = { -20.0, -22.0, -11.0, -13.0, 3.0 };
    obstacleCount_ = 5;
}

bool WorldCollisionCore::overlaps(double value, double minValue, double maxValue) noexcept {
    return value > minValue && value < maxValue;
}

double WorldCollisionCore::nearestBoundary(double previous,
                                           double desired,
                                           double minValue,
                                           double maxValue) noexcept {
    if (previous <= minValue) return minValue;
    if (previous >= maxValue) return maxValue;
    const double distanceToMin = std::abs(desired - minValue);
    const double distanceToMax = std::abs(desired - maxValue);
    return distanceToMin <= distanceToMax ? minValue : maxValue;
}

CollisionResult WorldCollisionCore::resolve(double previousX,
                                            double previousZ,
                                            double desiredX,
                                            double desiredZ,
                                            double radius) const noexcept {
    CollisionResult out{};
    if (!std::isfinite(previousX) || !std::isfinite(previousZ) ||
        !std::isfinite(desiredX) || !std::isfinite(desiredZ) ||
        !std::isfinite(radius) || radius <= 0.0) {
        out.x = previousX;
        out.z = previousZ;
        return out;
    }

    const double minX = minWorldX_ + radius;
    const double maxX = maxWorldX_ - radius;
    const double minZ = minWorldZ_ + radius;
    const double maxZ = maxWorldZ_ - radius;

    out.x = std::clamp(desiredX, minX, maxX);
    out.z = std::clamp(desiredZ, minZ, maxZ);
    if (out.x != desiredX) { out.hitX = true; ++out.contacts; }
    if (out.z != desiredZ) { out.hitZ = true; ++out.contacts; }

    for (std::size_t i = 0; i < obstacleCount_; ++i) {
        const auto& obstacle = obstacles_[i];
        const double expandedMinX = obstacle.minX - radius;
        const double expandedMaxX = obstacle.maxX + radius;
        const double expandedMinZ = obstacle.minZ - radius;
        const double expandedMaxZ = obstacle.maxZ + radius;

        if (overlaps(out.z, expandedMinZ, expandedMaxZ) &&
            overlaps(out.x, expandedMinX, expandedMaxX)) {
            out.x = nearestBoundary(previousX, out.x, expandedMinX, expandedMaxX);
            out.x = std::clamp(out.x, minX, maxX);
            out.hitX = true;
            ++out.contacts;
        }
    }

    for (std::size_t i = 0; i < obstacleCount_; ++i) {
        const auto& obstacle = obstacles_[i];
        const double expandedMinX = obstacle.minX - radius;
        const double expandedMaxX = obstacle.maxX + radius;
        const double expandedMinZ = obstacle.minZ - radius;
        const double expandedMaxZ = obstacle.maxZ + radius;

        if (overlaps(out.x, expandedMinX, expandedMaxX) &&
            overlaps(out.z, expandedMinZ, expandedMaxZ)) {
            out.z = nearestBoundary(previousZ, out.z, expandedMinZ, expandedMaxZ);
            out.z = std::clamp(out.z, minZ, maxZ);
            out.hitZ = true;
            ++out.contacts;
        }
    }

    return out;
}

bool WorldCollisionCore::validate() const noexcept {
    if (!std::isfinite(minWorldX_) || !std::isfinite(maxWorldX_) ||
        !std::isfinite(minWorldZ_) || !std::isfinite(maxWorldZ_)) return false;
    if (minWorldX_ >= maxWorldX_ || minWorldZ_ >= maxWorldZ_) return false;
    if (obstacleCount_ > kMaxObstacles) return false;
    for (std::size_t i = 0; i < obstacleCount_; ++i) {
        const auto& obstacle = obstacles_[i];
        if (!std::isfinite(obstacle.minX) || !std::isfinite(obstacle.maxX) ||
            !std::isfinite(obstacle.minZ) || !std::isfinite(obstacle.maxZ) ||
            !std::isfinite(obstacle.height)) return false;
        if (obstacle.minX >= obstacle.maxX || obstacle.minZ >= obstacle.maxZ || obstacle.height <= 0.0) return false;
        if (obstacle.minX <= minWorldX_ || obstacle.maxX >= maxWorldX_ ||
            obstacle.minZ <= minWorldZ_ || obstacle.maxZ >= maxWorldZ_) return false;
    }
    return true;
}

} // namespace metse
