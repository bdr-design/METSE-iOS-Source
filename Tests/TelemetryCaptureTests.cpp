#include "../Engine/Core/METSEEngineCore.hpp"
#include <cassert>
#include <iostream>
#include <type_traits>

int main() {
    using namespace metse;
    ObservatoryCore observer;
    ObservatoryFrameInput input{};
    input.realDeltaSeconds=1.0/60.0;
    input.simulationTick=1; input.catchUpSteps=1;
    input.configuredCombatants=32; input.aiActiveAgents=31;
    input.playerCombatCapable=true; input.activeProjectiles=64;
    observer.observe(input);
    assert(observer.report().combinedLoadSeconds==1.0/60.0);
    observer.observe(input); // Duplicate tick cannot manufacture coverage.
    assert(observer.report().combinedLoadSeconds==1.0/60.0);
    ++input.simulationTick; input.playerCombatCapable=false; observer.observe(input);
    ++input.simulationTick; input.playerCombatCapable=true; input.catchUpClamped=true; observer.observe(input);
    assert(observer.report().combinedLoadSeconds==1.0/60.0);
    ++input.simulationTick; input.catchUpClamped=false; input.activeProjectiles=63; observer.observe(input);
    assert(observer.report().fullCombatantLoadSeconds==2.0/60.0);
    assert(observer.report().combinedLoadSeconds==1.0/60.0);
    assert(observer.validate());
    observer.reset();
    assert(observer.report().fullCombatantLoadSeconds==0.0);
    static_assert(!std::is_copy_constructible_v<EngineCore>);
    static_assert(!std::is_copy_assignable_v<EngineCore>);
    static_assert(sizeof(EngineDiagnosticsCapture)<sizeof(EngineCore));
    static_assert(sizeof(EngineDiagnosticsCapture)<=192*1024);
    EngineCore core;
    assert(core.setActiveCombatants(32));
    for(int i=0;i<800;++i) core.advance(1.0/60.0);
    const auto hash=core.deterministicStateHash();
    EngineDiagnosticsCapture capture;
    core.captureDiagnostics(capture);
    const auto before=capture.finish();
    assert(before.stateHash==hash);
    assert(before.journalValid && before.observatoryValid && before.damageValid);
    assert(before.retainedBlackBoxFrames==EngineCore::kBlackBoxCapacity);
    const auto tick=capture.snapshot.simulationTick;
    core.setMovementInput(1.0,0.0);
    core.advance(1.0/60.0);
    core.reset();
    const auto after=capture.finish();
    assert(capture.snapshot.simulationTick==tick);
    assert(after.stateHash==before.stateHash);
    assert(after.journalHead==before.journalHead);
    assert(after.observatory.observedFrames==before.observatory.observedFrames);
    assert(after.journalValid && after.observatoryValid);
    assert(core.snapshot().simulationTick==0);
    core.captureDiagnostics(capture);
    const auto fresh=capture.finish();
    assert(fresh.stateHash==core.deterministicStateHash());
    assert(fresh.retainedPreSpikeCallbackFrames==0);
    std::cout<<"Telemetry capture PASS; core bytes="<<sizeof(EngineCore)
             <<" capture bytes="<<sizeof(EngineDiagnosticsCapture)<<'\n';
}
