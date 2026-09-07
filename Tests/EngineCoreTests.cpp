#include "../Engine/Core/METSECharacterMotor.hpp"
#include "../Engine/Core/METSEEngineCore.hpp"
#include "../Engine/Core/METSEIntegrityCore.hpp"
#include "../Engine/Core/METSEObservatoryCore.hpp"
#include "../Engine/Core/METSEWorldCollision.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

int main() {
    using namespace metse;

    assert(sha256Hex(sha256("abc")) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    assert(sha256Hex(sha256("")) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    WorldCollisionCore world;
    assert(world.validate());
    assert(world.obstacleCount() == 5);
    const auto freeMove = world.resolve(0.0, 0.0, 1.0, 1.0, 0.34);
    assert(!freeMove.hitX && !freeMove.hitZ);
    const auto wallHit = world.resolve(5.0, 14.0, 7.0, 14.0, 0.34);
    assert(wallHit.hitX && wallHit.x <= 5.661);
    const auto boundaryHit = world.resolve(47.0, 0.0, 60.0, 0.0, 0.34);
    assert(boundaryHit.hitX && boundaryHit.x <= 47.661);

    ObservatoryCore observatory;
    observatory.reset();
    ObservatoryFrameInput observed{};
    observed.grounded = true;
    observed.stance = CharacterStance::Standing;
    observed.gait = CharacterGait::Jog;
    for (int i = 0; i < 700; ++i) {
        observed.simulationTick = static_cast<std::uint64_t>(i);
        observed.realDeltaSeconds = (i == 350) ? 0.050 : (1.0 / 60.0);
        observed.playerZ += 0.06;
        observed.horizontalSpeed = 4.0;
        observed.catchUpClamped = (i == 350);
        observed.collisionContacts = (i == 400) ? 1u : 0u;
        observatory.observe(observed);
    }
    const auto obsReport = observatory.report();
    assert(observatory.validate());
    assert(obsReport.observedFrames == 700);
    assert(obsReport.retainedFrames == ObservatoryCore::kFrameCapacity);
    assert(obsReport.estimatedFPS > 50.0 && obsReport.estimatedFPS < 65.0);
    assert(obsReport.maxFrameMilliseconds >= 49.9);
    assert(obsReport.catchUpClampedFrames == 1);
    assert(obsReport.totalCollisionContacts == 1);
    assert(obsReport.distanceTravelled > 35.0);

    CharacterMotor motor;
    assert(motor.validate());
    CharacterInput input{};
    input.forward = 0.2;
    for (int i = 0; i < 90; ++i) motor.fixedStep(1.0 / 60.0, input);
    assert(motor.state().gait == CharacterGait::Walk);
    assert(motor.horizontalSpeed() <= 1.56);

    input.forward = 0.55;
    for (int i = 0; i < 60; ++i) motor.fixedStep(1.0 / 60.0, input);
    assert(motor.state().gait == CharacterGait::Tactical);
    assert(motor.horizontalSpeed() > 2.5 && motor.horizontalSpeed() <= 2.66);

    input.forward = 1.0;
    for (int i = 0; i < 60; ++i) motor.fixedStep(1.0 / 60.0, input);
    assert(motor.state().gait == CharacterGait::Jog);
    assert(motor.horizontalSpeed() > 4.0 && motor.horizontalSpeed() <= 4.16);
    assert(std::abs(motor.state().cameraBobY) < 0.05);
    assert(std::abs(motor.state().cameraRoll) < 0.04);

    input.sprintHeld = true;
    for (int i = 0; i < 60; ++i) motor.fixedStep(1.0 / 60.0, input);
    assert(motor.state().gait == CharacterGait::Sprint && motor.state().sprinting);
    assert(motor.horizontalSpeed() > 5.8 && motor.horizontalSpeed() <= 6.01);

    motor.cycleStance();
    for (int i = 0; i < 30; ++i) motor.fixedStep(1.0 / 60.0, input);
    assert(motor.state().stance == CharacterStance::Crouched);
    assert(motor.state().gait == CharacterGait::Crouch);
    assert(!motor.state().sprinting && motor.horizontalSpeed() <= 2.01);

    motor.addLookInput(0.35, 0.2);
    assert(std::abs(motor.state().bodyYaw) < 1e-9);
    assert(motor.cameraYaw() > 0.34);
    motor.addLookInput(0.35, 0.0);
    const double bodyBefore = motor.state().bodyYaw;
    motor.fixedStep(1.0 / 60.0, {});
    assert(motor.state().bodyYaw > bodyBefore);

    motor.testOnlySetAirborne(2.0, 0.0);
    for (int i = 0; i < 120; ++i) motor.fixedStep(1.0 / 60.0, {});
    assert(motor.state().grounded && std::abs(motor.state().y) < 1e-9);
    assert(motor.state().landingOffset <= 0.0 && motor.validate());

    EngineCore core;
    assert(core.config().maxCombatants == 32);
    assert(core.diagnostics().journalValid);
    assert(core.diagnostics().worldValid);
    assert(core.diagnostics().observatoryValid);
    assert(core.worldObstacleCount() == 5);
    assert(!core.setActiveCombatants(33));
    assert(core.setActiveCombatants(16));

    core.advance(1.0 / 60.0);
    core.setMovementInput(1.0, 0.0);
    for (int i = 0; i < 60; ++i) core.advance(1.0 / 60.0);
    assert(core.snapshot().gait == CharacterGait::Jog);
    assert(core.snapshot().horizontalSpeed > 4.0);

    const double jogSpeed = core.snapshot().horizontalSpeed;
    core.setSprintHeld(true);
    for (int i = 0; i < 60; ++i) core.advance(1.0 / 60.0);
    assert(core.snapshot().gait == CharacterGait::Sprint);
    assert(core.snapshot().horizontalSpeed > jogSpeed + 1.5);

    core.cycleStance();
    for (int i = 0; i < 30; ++i) core.advance(1.0 / 60.0);
    assert(core.snapshot().stance == CharacterStance::Crouched && !core.snapshot().sprinting);
    core.cycleStance();
    for (int i = 0; i < 30; ++i) core.advance(1.0 / 60.0);
    assert(core.snapshot().stance == CharacterStance::Prone && core.snapshot().cameraHeight < 0.6);
    core.cycleStance();
    for (int i = 0; i < 30; ++i) core.advance(1.0 / 60.0);
    assert(core.snapshot().stance == CharacterStance::Standing);

    core.setMovementInput(std::numeric_limits<double>::quiet_NaN(), 1.0);
    core.setMovementInput(0.0, 0.0);
    core.setSprintHeld(false);
    for (int i = 0; i < 60; ++i) core.advance(1.0 / 60.0);
    assert(core.snapshot().horizontalSpeed < 0.001);

    core.testOnlySetAirborne(2.0, 0.0);
    for (int i = 0; i < 120; ++i) core.advance(1.0 / 60.0);
    assert(core.snapshot().grounded && std::abs(core.snapshot().playerY) < 1e-9);

    core.triggerFire();
    assert(core.snapshot().shotsFired == 1);
    const auto beforeRollback = core.snapshot();
    assert(!core.testOnlyExecuteInvariantViolation());
    const auto afterRollback = core.snapshot();
    assert(afterRollback.playerX == beforeRollback.playerX);
    assert(afterRollback.playerZ == beforeRollback.playerZ);

    const auto diagnostics = core.diagnostics();
    assert(diagnostics.integrity.commandsRejected >= 2);
    assert(diagnostics.integrity.commandsRolledBack >= 1);
    assert(diagnostics.journalValid && diagnostics.worldValid && diagnostics.observatoryValid);
    assert(diagnostics.observatory.observedFrames > 0);

    for (int i = 0; i < 400; ++i) core.triggerFire();
    for (int i = 0; i < 900; ++i) core.advance(1.0 / 60.0);
    const auto bounded = core.diagnostics();
    assert(bounded.retainedCommands == IntegrityCore::kCommandCapacity);
    assert(bounded.retainedEvents == IntegrityCore::kEventCapacity);
    assert(bounded.retainedBlackBoxFrames == EngineCore::kBlackBoxCapacity);
    assert(bounded.observatory.retainedFrames == ObservatoryCore::kFrameCapacity);
    assert(bounded.journalValid);

    BlackBoxFrame latest{}, oldest{};
    assert(core.newestBlackBoxFrame(0, latest));
    assert(core.newestBlackBoxFrame(EngineCore::kBlackBoxCapacity - 1, oldest));
    assert(latest.simulationTick >= oldest.simulationTick);
    assert(std::isfinite(latest.cameraRoll));

    std::cout << "METSE Build 007 World + Character Feel + Observatory Tests: PASS\n";
    return 0;
}
