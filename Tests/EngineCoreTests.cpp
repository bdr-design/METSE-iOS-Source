#include "../Engine/Core/METSEEngineCore.hpp"
#include "../Engine/Core/METSEIntegrityCore.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

int main() {
    using namespace metse;

    assert(sha256Hex(sha256("abc")) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    assert(sha256Hex(sha256("")) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    EngineCore core;
    assert(core.config().maxCombatants == 32);
    assert(core.diagnostics().journalValid);
    assert(core.diagnostics().retainedEvents == 1);

    assert(!core.setActiveCombatants(33));
    assert(core.snapshot().activeCombatants == 0);
    assert(core.setActiveCombatants(16));
    assert(core.snapshot().activeCombatants == 16);

    core.advance(1.0 / 60.0);
    assert(core.snapshot().simulationTick == 1);
    core.setMovementInput(1.0, 0.0);
    for (int i = 0; i < 60; ++i) core.advance(1.0 / 60.0);
    assert(core.snapshot().playerZ > 4.0 && core.snapshot().playerZ < 5.0);

    BlackBoxFrame beforeInvalid{};
    assert(core.newestBlackBoxFrame(0, beforeInvalid));
    core.setMovementInput(std::numeric_limits<double>::quiet_NaN(), 1.0);
    const double zBefore = core.snapshot().playerZ;
    for (int i = 0; i < 10; ++i) core.advance(1.0 / 60.0);
    assert(core.snapshot().playerZ > zBefore);

    core.setMovementInput(0.0, 0.0);
    const double stoppedZ = core.snapshot().playerZ;
    for (int i = 0; i < 10; ++i) core.advance(1.0 / 60.0);
    assert(std::abs(core.snapshot().playerZ - stoppedZ) < 1e-9);

    core.addLookInput(100.0, 100.0);
    assert(core.snapshot().playerPitch <= 1.15);
    core.triggerFire();
    assert(core.snapshot().shotsFired == 1);

    const auto beforeRollback = core.snapshot();
    assert(!core.testOnlyExecuteInvariantViolation());
    const auto afterRollback = core.snapshot();
    assert(std::isfinite(afterRollback.playerX));
    assert(afterRollback.playerX == beforeRollback.playerX);
    assert(afterRollback.playerZ == beforeRollback.playerZ);

    const auto diagnostics = core.diagnostics();
    assert(diagnostics.integrity.commandsAdmitted >= 7);
    assert(diagnostics.integrity.commandsCommitted >= 5);
    assert(diagnostics.integrity.commandsRejected >= 2);
    assert(diagnostics.integrity.commandsRolledBack >= 1);
    assert(diagnostics.journalValid);
    assert(diagnostics.retainedBlackBoxFrames > 0);

    EventRecord newest{};
    assert(core.newestEvent(0, newest));
    assert(newest.sequence == diagnostics.integrity.eventSequence);
    assert(newest.hash == diagnostics.journalHead);

    for (int i = 0; i < 400; ++i) core.triggerFire();
    for (int i = 0; i < 900; ++i) core.advance(1.0 / 60.0);
    const auto bounded = core.diagnostics();
    assert(bounded.retainedCommands == IntegrityCore::kCommandCapacity);
    assert(bounded.retainedEvents == IntegrityCore::kEventCapacity);
    assert(bounded.retainedBlackBoxFrames == EngineCore::kBlackBoxCapacity);
    assert(bounded.journalValid);

    BlackBoxFrame latest{};
    BlackBoxFrame oldest{};
    assert(core.newestBlackBoxFrame(0, latest));
    assert(core.newestBlackBoxFrame(EngineCore::kBlackBoxCapacity - 1, oldest));
    assert(latest.simulationTick >= oldest.simulationTick);

    std::cout << "METSE Engine Core + Runtime Integrity Tests: PASS\n";
    return 0;
}
