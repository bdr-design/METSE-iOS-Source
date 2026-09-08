#include "METSEInputCommandQueue.hpp"

#include <algorithm>
#include <cmath>

namespace metse {

void InputCommandQueue::reset() noexcept {
    commands_ = {};
    head_ = 0;
    count_ = 0;
    nextSequence_ = 1;
    metrics_ = {};
}

bool InputCommandQueue::isCoalescible(InputCommandKind kind) noexcept {
    return kind == InputCommandKind::Move || kind == InputCommandKind::Look ||
           kind == InputCommandKind::Sprint || kind == InputCommandKind::Aim;
}

std::size_t InputCommandQueue::indexFromOffset(std::size_t offset) const noexcept {
    return (head_ + offset) % kCapacity;
}

bool InputCommandQueue::append(InputCommand command) noexcept {
    if (count_ >= kCapacity) return false;
    const std::size_t index = indexFromOffset(count_);
    command.sequence = nextSequence_++;
    commands_[index] = command;
    ++count_;
    ++metrics_.enqueued;
    metrics_.highWatermark = std::max(metrics_.highWatermark, count_);
    return true;
}

bool InputCommandQueue::pushCoalesced(InputCommandKind kind, double a, double b, bool flag) noexcept {
    if (!std::isfinite(a) || !std::isfinite(b)) {
        ++metrics_.rejectedInvalid;
        return false;
    }

    // Do not coalesce across a discrete-command ordering barrier. Search only the
    // coalescible tail after the newest critical command.
    for (std::size_t reverse = 0; reverse < count_; ++reverse) {
        const std::size_t offset = count_ - 1 - reverse;
        InputCommand& existing = commands_[indexFromOffset(offset)];
        if (!isCoalescible(existing.kind)) break;
        if (existing.kind == kind) {
            if (kind == InputCommandKind::Look) {
                existing.a = std::clamp(existing.a + a, -1.2, 1.2);
                existing.b = std::clamp(existing.b + b, -0.9, 0.9);
            } else {
                existing.a = a;
                existing.b = b;
                existing.flag = flag;
            }
            ++metrics_.coalesced;
            return true;
        }
    }

    if (count_ >= kCapacity && !removeOldestCoalescible()) {
        ++metrics_.rejectedInvalid;
        return false;
    }
    return append(InputCommand{0, kind, a, b, flag});
}

bool InputCommandQueue::removeOldestCoalescible() noexcept {
    if (count_ == 0) return false;
    std::size_t removable = count_;
    for (std::size_t offset = 0; offset < count_; ++offset) {
        if (isCoalescible(commands_[indexFromOffset(offset)].kind)) {
            removable = offset;
            break;
        }
    }
    if (removable == count_) return false;

    for (std::size_t offset = removable; offset + 1 < count_; ++offset) {
        commands_[indexFromOffset(offset)] = commands_[indexFromOffset(offset + 1)];
    }
    commands_[indexFromOffset(count_ - 1)] = {};
    --count_;
    ++metrics_.evictedCoalescible;
    return true;
}

bool InputCommandQueue::pushCritical(InputCommandKind kind) noexcept {
    if (count_ >= kCapacity && !removeOldestCoalescible()) {
        ++metrics_.rejectedCritical;
        return false;
    }
    return append(InputCommand{0, kind, 0.0, 0.0, false});
}

bool InputCommandQueue::pushMove(double forward, double strafe) noexcept {
    if (!std::isfinite(forward) || !std::isfinite(strafe)) {
        ++metrics_.rejectedInvalid;
        return false;
    }
    forward = std::clamp(forward, -1.0, 1.0);
    strafe = std::clamp(strafe, -1.0, 1.0);
    const double length = std::hypot(forward, strafe);
    if (length > 1.0) {
        forward /= length;
        strafe /= length;
    }
    return pushCoalesced(InputCommandKind::Move, forward, strafe, false);
}

bool InputCommandQueue::pushLook(double yawDelta, double pitchDelta) noexcept {
    if (!std::isfinite(yawDelta) || !std::isfinite(pitchDelta)) {
        ++metrics_.rejectedInvalid;
        return false;
    }
    return pushCoalesced(InputCommandKind::Look,
                         std::clamp(yawDelta, -0.7, 0.7),
                         std::clamp(pitchDelta, -0.5, 0.5),
                         false);
}

bool InputCommandQueue::pushSprint(bool held) noexcept {
    return pushCoalesced(InputCommandKind::Sprint, 0.0, 0.0, held);
}

bool InputCommandQueue::pushAim(bool held) noexcept {
    return pushCoalesced(InputCommandKind::Aim, 0.0, 0.0, held);
}

bool InputCommandQueue::pushFire() noexcept { return pushCritical(InputCommandKind::Fire); }
bool InputCommandQueue::pushReload() noexcept { return pushCritical(InputCommandKind::Reload); }
bool InputCommandQueue::pushCycleStance() noexcept { return pushCritical(InputCommandKind::CycleStance); }

bool InputCommandQueue::pop(InputCommand& out) noexcept {
    if (count_ == 0) return false;
    out = commands_[head_];
    commands_[head_] = {};
    head_ = (head_ + 1) % kCapacity;
    --count_;
    ++metrics_.dequeued;
    return true;
}

bool InputCommandQueue::validate() const noexcept {
    if (head_ >= kCapacity || count_ > kCapacity || metrics_.highWatermark > kCapacity) return false;
    std::uint64_t previousSequence = 0;
    for (std::size_t offset = 0; offset < count_; ++offset) {
        const auto& command = commands_[indexFromOffset(offset)];
        if (command.sequence == 0 || command.sequence <= previousSequence) return false;
        previousSequence = command.sequence;
        if (!std::isfinite(command.a) || !std::isfinite(command.b)) return false;
    }
    return true;
}

} // namespace metse
