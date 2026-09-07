#include "METSEIntegrityCore.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>

namespace metse {
namespace {

constexpr std::array<std::uint32_t, 64> kSha256K = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};

constexpr std::uint32_t rotr(std::uint32_t x, unsigned n) noexcept {
    return (x >> n) | (x << (32u - n));
}

void writeBigEndian32(std::uint8_t* out, std::uint32_t value) noexcept {
    out[0] = static_cast<std::uint8_t>(value >> 24);
    out[1] = static_cast<std::uint8_t>(value >> 16);
    out[2] = static_cast<std::uint8_t>(value >> 8);
    out[3] = static_cast<std::uint8_t>(value);
}

void writeBigEndian64(std::uint8_t* out, std::uint64_t value) noexcept {
    for (int i = 7; i >= 0; --i) {
        out[7 - i] = static_cast<std::uint8_t>(value >> (i * 8));
    }
}

std::uint32_t readBigEndian32(const std::uint8_t* in) noexcept {
    return (static_cast<std::uint32_t>(in[0]) << 24) |
           (static_cast<std::uint32_t>(in[1]) << 16) |
           (static_cast<std::uint32_t>(in[2]) << 8) |
           static_cast<std::uint32_t>(in[3]);
}

void transformSha256(std::array<std::uint32_t, 8>& h, const std::uint8_t* block) noexcept {
    std::array<std::uint32_t, 64> w{};
    for (std::size_t i = 0; i < 16; ++i) w[i] = readBigEndian32(block + i * 4);
    for (std::size_t i = 16; i < 64; ++i) {
        const auto s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const auto s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    auto a=h[0], b=h[1], c=h[2], d=h[3], e=h[4], f=h[5], g=h[6], hh=h[7];
    for (std::size_t i = 0; i < 64; ++i) {
        const auto s1 = rotr(e,6) ^ rotr(e,11) ^ rotr(e,25);
        const auto ch = (e & f) ^ ((~e) & g);
        const auto temp1 = hh + s1 + ch + kSha256K[i] + w[i];
        const auto s0 = rotr(a,2) ^ rotr(a,13) ^ rotr(a,22);
        const auto maj = (a & b) ^ (a & c) ^ (b & c);
        const auto temp2 = s0 + maj;
        hh=g; g=f; f=e; e=d+temp1; d=c; c=b; b=a; a=temp1+temp2;
    }
    h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
}

void appendU64(std::array<std::uint8_t, 96>& buffer, std::size_t& cursor, std::uint64_t value) noexcept {
    writeBigEndian64(buffer.data() + cursor, value);
    cursor += 8;
}

} // namespace

Sha256Digest sha256(std::span<const std::uint8_t> bytes) noexcept {
    std::array<std::uint32_t, 8> h = {
        0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
        0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u
    };

    const std::size_t fullBlocks = bytes.size() / 64;
    for (std::size_t i = 0; i < fullBlocks; ++i) transformSha256(h, bytes.data() + i * 64);

    std::array<std::uint8_t, 128> tail{};
    const std::size_t remainder = bytes.size() % 64;
    if (remainder) std::memcpy(tail.data(), bytes.data() + fullBlocks * 64, remainder);
    tail[remainder] = 0x80;
    const std::size_t tailBlocks = remainder < 56 ? 1 : 2;
    const std::uint64_t bitLength = static_cast<std::uint64_t>(bytes.size()) * 8u;
    writeBigEndian64(tail.data() + tailBlocks * 64 - 8, bitLength);
    for (std::size_t i = 0; i < tailBlocks; ++i) transformSha256(h, tail.data() + i * 64);

    Sha256Digest out{};
    for (std::size_t i = 0; i < h.size(); ++i) writeBigEndian32(out.data() + i * 4, h[i]);
    return out;
}

Sha256Digest sha256(std::string_view text) noexcept {
    return sha256(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(text.data()), text.size()));
}

std::string sha256Hex(const Sha256Digest& digest) {
    static constexpr char hex[] = "0123456789abcdef";
    std::string out;
    out.resize(64);
    for (std::size_t i = 0; i < digest.size(); ++i) {
        out[i * 2] = hex[digest[i] >> 4];
        out[i * 2 + 1] = hex[digest[i] & 0x0f];
    }
    return out;
}

IntegrityCore::IntegrityCore() noexcept = default;

std::uint64_t IntegrityCore::admit(CommandKind kind,
                                   std::uint64_t simulationTick,
                                   std::uint64_t correlationId,
                                   std::uint64_t causationId) noexcept {
    const std::uint64_t id = nextCommandId_++;
    if (correlationId == 0) correlationId = id;
    CommandRecord record{};
    record.id = id;
    record.correlationId = correlationId;
    record.causationId = causationId;
    record.admittedAtTick = simulationTick;
    record.kind = kind;
    record.status = CommandStatus::Admitted;
    commands_[commandWrite_] = record;
    commandWrite_ = (commandWrite_ + 1) % kCommandCapacity;
    commandCount_ = std::min(commandCount_ + 1, kCommandCapacity);
    ++metrics_.commandsAdmitted;
    appendEvent(EventKind::CommandAdmitted, id, correlationId, causationId, simulationTick);
    return id;
}

void IntegrityCore::commit(std::uint64_t commandId, std::uint64_t simulationTick) noexcept {
    auto* command = findCommand(commandId);
    if (!command || command->status != CommandStatus::Admitted) return;
    command->status = CommandStatus::Committed;
    command->completedAtTick = simulationTick;
    ++metrics_.commandsCommitted;
    appendEvent(EventKind::CommandCommitted, command->id, command->correlationId, command->causationId, simulationTick);
}

void IntegrityCore::reject(std::uint64_t commandId, std::uint64_t simulationTick) noexcept {
    auto* command = findCommand(commandId);
    if (!command || command->status != CommandStatus::Admitted) return;
    command->status = CommandStatus::Rejected;
    command->completedAtTick = simulationTick;
    ++metrics_.commandsRejected;
    appendEvent(EventKind::CommandRejected, command->id, command->correlationId, command->causationId, simulationTick);
}

void IntegrityCore::rollback(std::uint64_t commandId, std::uint64_t simulationTick) noexcept {
    auto* command = findCommand(commandId);
    if (!command || command->status != CommandStatus::Admitted) return;
    command->status = CommandStatus::RolledBack;
    command->completedAtTick = simulationTick;
    ++metrics_.commandsRolledBack;
    appendEvent(EventKind::CommandRolledBack, command->id, command->correlationId, command->causationId, simulationTick);
}

void IntegrityCore::appendDomainEvent(EventKind kind,
                                      std::uint64_t commandId,
                                      std::uint64_t simulationTick) noexcept {
    const auto* command = findCommand(commandId);
    appendEvent(kind,
                commandId,
                command ? command->correlationId : commandId,
                command ? command->causationId : 0,
                simulationTick);
}

void IntegrityCore::appendSystemEvent(EventKind kind, std::uint64_t simulationTick) noexcept {
    appendEvent(kind, 0, 0, 0, simulationTick);
}

CommandRecord* IntegrityCore::findCommand(std::uint64_t id) noexcept {
    for (std::size_t i = 0; i < commandCount_; ++i) {
        const std::size_t index = (commandWrite_ + kCommandCapacity - 1 - i) % kCommandCapacity;
        if (commands_[index].id == id) return &commands_[index];
    }
    return nullptr;
}

const CommandRecord* IntegrityCore::findCommand(std::uint64_t id) const noexcept {
    for (std::size_t i = 0; i < commandCount_; ++i) {
        const std::size_t index = (commandWrite_ + kCommandCapacity - 1 - i) % kCommandCapacity;
        if (commands_[index].id == id) return &commands_[index];
    }
    return nullptr;
}

void IntegrityCore::appendEvent(EventKind kind,
                                std::uint64_t commandId,
                                std::uint64_t correlationId,
                                std::uint64_t causationId,
                                std::uint64_t simulationTick) noexcept {
    EventRecord event{};
    event.sequence = ++metrics_.eventSequence;
    event.commandId = commandId;
    event.correlationId = correlationId;
    event.causationId = causationId;
    event.simulationTick = simulationTick;
    event.kind = kind;
    event.previousHash = journalHead_;
    event.hash = hashEvent(event);
    journalHead_ = event.hash;
    events_[eventWrite_] = event;
    eventWrite_ = (eventWrite_ + 1) % kEventCapacity;
    eventCount_ = std::min(eventCount_ + 1, kEventCapacity);
}

Sha256Digest IntegrityCore::hashEvent(const EventRecord& event) noexcept {
    std::array<std::uint8_t, 96> canonical{};
    std::size_t cursor = 0;
    std::copy(event.previousHash.begin(), event.previousHash.end(), canonical.begin());
    cursor += event.previousHash.size();
    canonical[cursor++] = static_cast<std::uint8_t>(event.kind);
    appendU64(canonical, cursor, event.sequence);
    appendU64(canonical, cursor, event.commandId);
    appendU64(canonical, cursor, event.correlationId);
    appendU64(canonical, cursor, event.causationId);
    appendU64(canonical, cursor, event.simulationTick);
    return sha256(std::span<const std::uint8_t>(canonical.data(), cursor));
}

bool IntegrityCore::verifyJournal() const noexcept {
    if (eventCount_ == 0) return journalHead_ == Sha256Digest{};
    const std::size_t oldest = (eventWrite_ + kEventCapacity - eventCount_) % kEventCapacity;
    Sha256Digest previous{};
    bool first = true;
    Sha256Digest last{};
    for (std::size_t i = 0; i < eventCount_; ++i) {
        const auto& event = events_[(oldest + i) % kEventCapacity];
        if (!first && event.previousHash != previous) return false;
        if (hashEvent(event) != event.hash) return false;
        previous = event.hash;
        last = event.hash;
        first = false;
    }
    return last == journalHead_;
}

bool IntegrityCore::newestEvent(std::size_t offset, EventRecord& out) const noexcept {
    if (offset >= eventCount_) return false;
    const std::size_t index = (eventWrite_ + kEventCapacity - 1 - offset) % kEventCapacity;
    out = events_[index];
    return true;
}

bool IntegrityCore::newestCommand(std::size_t offset, CommandRecord& out) const noexcept {
    if (offset >= commandCount_) return false;
    const std::size_t index = (commandWrite_ + kCommandCapacity - 1 - offset) % kCommandCapacity;
    out = commands_[index];
    return true;
}

} // namespace metse
