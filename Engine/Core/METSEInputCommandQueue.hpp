#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace metse {

enum class InputCommandKind : std::uint8_t {
    Move = 1,
    Look,
    Sprint,
    Aim,
    Fire,
    Reload,
    CycleStance
};

struct InputCommand {
    std::uint64_t sequence = 0;
    InputCommandKind kind = InputCommandKind::Move;
    double a = 0.0;
    double b = 0.0;
    bool flag = false;
};

struct InputQueueMetrics {
    std::uint64_t enqueued = 0;
    std::uint64_t dequeued = 0;
    std::uint64_t coalesced = 0;
    std::uint64_t evictedCoalescible = 0;
    std::uint64_t rejectedCritical = 0;
    std::uint64_t rejectedInvalid = 0;
    std::size_t highWatermark = 0;
};

class InputCommandQueue final {
public:
    static constexpr std::size_t kCapacity = 64;

    void reset() noexcept;
    bool pushMove(double forward, double strafe) noexcept;
    bool pushLook(double yawDelta, double pitchDelta) noexcept;
    bool pushSprint(bool held) noexcept;
    bool pushAim(bool held) noexcept;
    bool pushFire() noexcept;
    bool pushReload() noexcept;
    bool pushCycleStance() noexcept;
    bool pop(InputCommand& out) noexcept;

    [[nodiscard]] std::size_t size() const noexcept { return count_; }
    [[nodiscard]] bool empty() const noexcept { return count_ == 0; }
    [[nodiscard]] const InputQueueMetrics& metrics() const noexcept { return metrics_; }
    [[nodiscard]] bool validate() const noexcept;

private:
    static bool isCoalescible(InputCommandKind kind) noexcept;
    bool pushCoalesced(InputCommandKind kind, double a, double b, bool flag) noexcept;
    bool pushCritical(InputCommandKind kind) noexcept;
    bool append(InputCommand command) noexcept;
    bool removeOldestCoalescible() noexcept;
    std::size_t indexFromOffset(std::size_t offset) const noexcept;

    std::array<InputCommand, kCapacity> commands_{};
    std::size_t head_ = 0;
    std::size_t count_ = 0;
    std::uint64_t nextSequence_ = 1;
    InputQueueMetrics metrics_{};
};

} // namespace metse
