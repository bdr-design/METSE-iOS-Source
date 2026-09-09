#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace metse {

using CombatantId = std::uint32_t;
using TeamId = std::uint16_t;
using FactionId = std::uint16_t;

enum class CombatantRole : std::uint8_t { Player=0, AI=1 };
enum class TargetRelation : std::uint8_t { Invalid=0, Self=1, Friendly=2, Neutral=3, Hostile=4 };
enum class TargetingPolicy : std::uint8_t { HostileOnly=0, AllowFriendlyFire=1 };

struct CombatantIdentity {
    CombatantId id = 0;
    TeamId teamId = 0;
    FactionId factionId = 0;
    CombatantRole role = CombatantRole::AI;
};

struct DamageSource {
    CombatantIdentity identity{};
    TargetingPolicy policy = TargetingPolicy::HostileOnly;
    bool includePlayerTarget = false;
};

struct CombatantRecord {
    CombatantIdentity identity{};
    bool alive = false;
    bool combatCapable = false;
    bool targetable = false;
};

class CombatantCore final {
public:
    static constexpr std::size_t kMaxCombatants = 32;
    static constexpr CombatantId kPlayerId = 0x4D455453u; // "METS", stable player identity.
    static constexpr TeamId kPlayerTeam = 1;
    static constexpr TeamId kHostileTeam = 2;
    static constexpr FactionId kPlayerFaction = 1;
    static constexpr FactionId kHostileFaction = 2;

    void reset() noexcept;
    bool configure(std::size_t index,CombatantIdentity identity,bool alive=true,bool combatCapable=true,bool targetable=true) noexcept;
    bool syncState(std::size_t index,CombatantId id,bool alive,bool combatCapable,bool targetable=true) noexcept;

    [[nodiscard]] static TargetRelation relation(CombatantIdentity source,CombatantIdentity target) noexcept;
    [[nodiscard]] static bool canTarget(const DamageSource& source,CombatantIdentity target) noexcept;
    [[nodiscard]] const CombatantRecord* recordById(CombatantId id) const noexcept;
    [[nodiscard]] const CombatantRecord& record(std::size_t index) const noexcept { return records_[index]; }
    [[nodiscard]] const std::array<CombatantRecord,kMaxCombatants>& records() const noexcept { return records_; }
    [[nodiscard]] std::size_t count() const noexcept { return count_; }
    [[nodiscard]] bool validate() const noexcept;

private:
    std::array<CombatantRecord,kMaxCombatants> records_{};
    std::size_t count_ = 0;
};

} // namespace metse
