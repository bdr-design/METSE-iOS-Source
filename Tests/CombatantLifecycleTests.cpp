#include "../Engine/Core/METSECombatantCore.hpp"
#include "../Engine/Core/METSEEngineCore.hpp"
#include <cassert>
#include <iostream>

int main() {
    using namespace metse;

    CombatantCore lifecycle;
    const CombatantIdentity player{CombatantCore::kPlayerId,CombatantCore::kPlayerTeam,
                                   CombatantCore::kPlayerFaction,CombatantRole::Player};
    const CombatantIdentity hostile{1,CombatantCore::kHostileTeam,CombatantCore::kHostileFaction,
                                    CombatantRole::AI};
    assert(lifecycle.configure(0,player));
    assert(lifecycle.configure(1,hostile));
    assert(lifecycle.syncLifecycle(0,player.id,CombatantLifecycleState::Wounded));
    assert(lifecycle.syncLifecycle(1,hostile.id,CombatantLifecycleState::Incapacitated));
    auto report=lifecycle.report();
    assert(report.wounded==1&&report.incapacitated==1&&report.active==0);
    assert(!lifecycle.record(1).combatCapable&&lifecycle.record(1).alive);
    assert(lifecycle.syncLifecycle(1,hostile.id,CombatantLifecycleState::Dead));
    report=lifecycle.report();
    assert(report.dead==1&&report.incapacitated==0);
    assert(!lifecycle.record(1).alive&&!lifecycle.record(1).targetable);
    assert(lifecycle.validate());

    EngineCore core;
    assert(core.setActiveCombatants(32));
    for(int i=0;i<1200;++i) core.advance(1.0/60.0);
    const auto diagnostics=core.diagnostics();
    assert(diagnostics.combatants.validate());
    assert(diagnostics.combatantLifecycle.dead+diagnostics.combatantLifecycle.incapacitated+
           diagnostics.combatantLifecycle.active+diagnostics.combatantLifecycle.wounded==32);
    assert(diagnostics.combatantLifecycle.dead+diagnostics.combatantLifecycle.incapacitated>0);
    assert(core.deterministicStateHash()==core.deterministicStateHash());

    std::cout<<"METSE Build 010-B Combatant Lifecycle Tests: PASS\n";
    return 0;
}
