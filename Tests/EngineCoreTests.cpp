#include "METSEEngineCore.hpp"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    metse::EngineConfig config;
    config.fixedStepSeconds = 1.0 / 60.0;
    config.maxCatchUpSteps = 4;
    config.maxCombatants = 32;

    metse::EngineCore engine(config);
    assert(engine.snapshot().simulationTick == 0);
    assert(engine.setActiveCombatants(32));
    assert(!engine.setActiveCombatants(33));

    engine.advance(1.0 / 60.0);
    assert(engine.snapshot().simulationTick == 1);

    const auto before = engine.snapshot().simulationTick;
    engine.advance(10.0);
    const auto advanced = engine.snapshot().simulationTick - before;
    assert(advanced <= 4);
    assert(engine.snapshot().interpolationAlpha >= 0.0);
    assert(engine.snapshot().interpolationAlpha <= 1.0);

    const auto stable = engine.snapshot().simulationTick;
    engine.advance(-1.0);
    assert(engine.snapshot().simulationTick == stable);

    std::cout << "METSE Engine Core Tests: PASS\n";
    return 0;
}
