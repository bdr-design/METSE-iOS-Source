#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace metse {

using Sha256Digest = std::array<std::uint8_t, 32>;

Sha256Digest sha256(std::span<const std::uint8_t> bytes) noexcept;
Sha256Digest sha256(std::string_view text) noexcept;
std::string sha256Hex(const Sha256Digest& digest);

enum class CommandKind : std::uint8_t {
    ResetSession = 1,
    SetActiveCombatants,
    SetMovementIntent,
    AddLookIntent,
    FireWeapon
};

enum class CommandStatus : std::uint8_t {
    Admitted = 1,
    Committed,
    Rejected,
    RolledBack
};

enum class EventKind : std::uint8_t {
    EngineBoot = 1,
    CommandAdmitted,
    CommandCommitted,
    CommandRejected,
    CommandRolledBack,
    SessionReset,
    CombatantCountChanged,
    MovementIntentChanged,
    LookIntentChanged,
    ShotFired
};

struct CommandRecord {
    std::uint64_t id = 0;
    std::uint64_t correlationId = 0;
    std::uint64_t causationId = 0;
    std::uint64_t admittedAtTick = 0;
    std::uint64_t completedAtTick = 0;
    CommandKind kind = CommandKind::ResetSession;
    CommandStatus status = CommandStatus::Admitted;
};

struct EventRecord {
    std::uint64_t sequence = 0;
    std::uint64_t commandId = 0;
    std::uint64_t correlationId = 0;
    std::uint64_t causationId = 0;
    std::uint64_t simulationTick = 0;
    EventKind kind = EventKind::EngineBoot;
    Sha256Digest previousHash{};
    Sha256Digest hash{};
};

struct IntegrityMetrics {
    std::uint64_t commandsAdmitted = 0;
    std::uint64_t commandsCommitted = 0;
    std::uint64_t commandsRejected = 0;
    std::uint64_t commandsRolledBack = 0;
    std::uint64_t eventSequence = 0;
};

class IntegrityCore final {
public:
    static constexpr std::size_t kCommandCapacity = 96;
    static constexpr std::size_t kEventCapacity = 256;

    IntegrityCore() noexcept;

    std::uint64_t admit(CommandKind kind,
                        std::uint64_t simulationTick,
                        std::uint64_t correlationId = 0,
                        std::uint64_t causationId = 0) noexcept;

    void commit(std::uint64_t commandId, std::uint64_t simulationTick) noexcept;
    void reject(std::uint64_t commandId, std::uint64_t simulationTick) noexcept;
    void rollback(std::uint64_t commandId, std::uint64_t simulationTick) noexcept;

    void appendDomainEvent(EventKind kind,
                           std::uint64_t commandId,
                           std::uint64_t simulationTick) noexcept;

    void appendSystemEvent(EventKind kind, std::uint64_t simulationTick) noexcept;

    [[nodiscard]] const IntegrityMetrics& metrics() const noexcept { return metrics_; }
    [[nodiscard]] const Sha256Digest& journalHead() const noexcept { return journalHead_; }
    [[nodiscard]] bool verifyJournal() const noexcept;
    [[nodiscard]] std::size_t eventCount() const noexcept { return eventCount_; }
    [[nodiscard]] std::size_t commandCount() const noexcept { return commandCount_; }
    [[nodiscard]] bool newestEvent(std::size_t offset, EventRecord& out) const noexcept;
    [[nodiscard]] bool newestCommand(std::size_t offset, CommandRecord& out) const noexcept;

private:
    CommandRecord* findCommand(std::uint64_t id) noexcept;
    const CommandRecord* findCommand(std::uint64_t id) const noexcept;
    void appendEvent(EventKind kind,
                     std::uint64_t commandId,
                     std::uint64_t correlationId,
                     std::uint64_t causationId,
                     std::uint64_t simulationTick) noexcept;
    static Sha256Digest hashEvent(const EventRecord& event) noexcept;

    std::array<CommandRecord, kCommandCapacity> commands_{};
    std::array<EventRecord, kEventCapacity> events_{};
    std::size_t commandWrite_ = 0;
    std::size_t commandCount_ = 0;
    std::size_t eventWrite_ = 0;
    std::size_t eventCount_ = 0;
    std::uint64_t nextCommandId_ = 1;
    Sha256Digest journalHead_{};
    IntegrityMetrics metrics_{};
};

} // namespace metse
