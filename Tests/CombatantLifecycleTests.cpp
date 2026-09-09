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
    assert(!lifecycle.syncLifecycle(1,hostile.id,static_cast<CombatantLifecycleState>(255)));
    assert(!lifecycle.syncLifecycle(1,hostile.id,CombatantLifecycleState::Active));
    assert(!lifecycle.syncState(1,hostile.id,true,true));
    assert(lifecycle.record(1).lifecycle==CombatantLifecycleState::Dead);
    assert(lifecycle.syncLifecycle(1,hostile.id,CombatantLifecycleState::Removed));
    assert(lifecycle.report().removed==1&&lifecycle.report().dead==0);
    assert(!lifecycle.syncLifecycle(1,hostile.id,CombatantLifecycleState::Dead));
    assert(!lifecycle.syncLifecycle(0,hostile.id,CombatantLifecycleState::Dead));
    assert(lifecycle.record(0).lifecycle==CombatantLifecycleState::Wounded);
    assert(!lifecycle.configure(3,{3,2,2,CombatantRole::AI}));
    assert(!lifecycle.configure(2,{3,2,2,static_cast<CombatantRole>(255)}));
    assert(!lifecycle.configure(2,hostile));
    assert(lifecycle.count()==2);
    assert(lifecycle.validate());

    EngineCore core;
    EngineCore repeated;
    assert(core.setActiveCombatants(32));
    assert(repeated.setActiveCombatants(32));
    for(int i=0;i<1200;++i){
        core.advance(1.0/60.0);
        repeated.advance(1.0/60.0);
        assert(core.deterministicStateHash()==repeated.deterministicStateHash());
    }
    const auto diagnostics=core.diagnostics();
    assert(diagnostics.combatants.validate());
    assert(diagnostics.combatantLifecycle.dead+diagnostics.combatantLifecycle.incapacitated+
           diagnostics.combatantLifecycle.active+diagnostics.combatantLifecycle.wounded==32);
    assert(diagnostics.combatantLifecycle.dead+diagnostics.combatantLifecycle.incapacitated>0);
    const auto beforeRollback=core.deterministicStateHash();
    assert(!core.testOnlyExecuteInvariantViolation());
    assert(core.deterministicStateHash()==beforeRollback);
    assert(core.diagnostics().combatants.validate());

    std::cout<<"METSE Build 010-B Combatant Lifecycle Tests: PASS\n";
    return 0;
}
