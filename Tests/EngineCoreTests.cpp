#include "../Engine/Core/METSECharacterMotor.hpp"
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

    CharacterMotor motor; assert(motor.validate()); assert(motor.state().grounded); assert(motor.state().stance == CharacterStance::Standing);
    CharacterInput motorInput{}; motorInput.forward = 1.0; for (int i=0;i<60;++i) motor.fixedStep(1.0/60.0,motorInput);
    assert(motor.horizontalSpeed()>4.0 && motor.horizontalSpeed()<=4.21); assert(motor.state().z>3.0 && motor.state().z<4.3);
    motorInput.sprintHeld=true; for(int i=0;i<60;++i) motor.fixedStep(1.0/60.0,motorInput); assert(motor.state().sprinting); assert(motor.horizontalSpeed()>6.2 && motor.horizontalSpeed()<=6.41);
    motor.cycleStance(); for(int i=0;i<30;++i) motor.fixedStep(1.0/60.0,motorInput); assert(motor.state().stance==CharacterStance::Crouched); assert(!motor.state().sprinting); assert(motor.horizontalSpeed()<=2.11); assert(motor.state().eyeHeight<1.2 && motor.state().eyeHeight>1.0);
    motor.addLookInput(0.35,0.2); assert(std::abs(motor.state().bodyYaw)<1e-9); assert(motor.cameraYaw()>0.34); motor.addLookInput(0.35,0.0); const double bodyBefore=motor.state().bodyYaw; motor.fixedStep(1.0/60.0,{}); assert(motor.state().bodyYaw>bodyBefore);
    motor.testOnlySetAirborne(2.0,0.0); assert(!motor.state().grounded); for(int i=0;i<120;++i) motor.fixedStep(1.0/60.0,{}); assert(motor.state().grounded); assert(std::abs(motor.state().y)<1e-9); assert(motor.validate());

    EngineCore core; assert(core.config().maxCombatants==32); assert(core.diagnostics().journalValid); assert(core.diagnostics().retainedEvents==1); assert(core.snapshot().grounded); assert(core.snapshot().cameraHeight>1.6);
    assert(!core.setActiveCombatants(33)); assert(core.snapshot().activeCombatants==0); assert(core.setActiveCombatants(16));
    core.advance(1.0/60.0); assert(core.snapshot().simulationTick==1); core.setMovementInput(1.0,0.0); for(int i=0;i<60;++i) core.advance(1.0/60.0); assert(core.snapshot().playerZ>3.0 && core.snapshot().playerZ<4.3); assert(core.snapshot().horizontalSpeed>4.0);
    const double jogSpeed=core.snapshot().horizontalSpeed; core.setSprintHeld(true); for(int i=0;i<60;++i) core.advance(1.0/60.0); assert(core.snapshot().sprinting); assert(core.snapshot().horizontalSpeed>jogSpeed+1.5);
    core.cycleStance(); for(int i=0;i<30;++i) core.advance(1.0/60.0); assert(core.snapshot().stance==CharacterStance::Crouched); assert(!core.snapshot().sprinting); assert(core.snapshot().cameraHeight<1.2);
    core.cycleStance(); for(int i=0;i<30;++i) core.advance(1.0/60.0); assert(core.snapshot().stance==CharacterStance::Prone); assert(core.snapshot().cameraHeight<0.6);
    core.cycleStance(); for(int i=0;i<30;++i) core.advance(1.0/60.0); assert(core.snapshot().stance==CharacterStance::Standing);
    core.addLookInput(0.35,0.15); const double bodyYawBefore=core.snapshot().playerBodyYaw; const double cameraYawBefore=core.snapshot().playerYaw; assert(std::abs(cameraYawBefore-bodyYawBefore)>0.2); core.addLookInput(0.35,0.0); core.advance(1.0/60.0); assert(core.snapshot().playerBodyYaw>bodyYawBefore);

    BlackBoxFrame beforeInvalid{}; assert(core.newestBlackBoxFrame(0,beforeInvalid)); core.setMovementInput(std::numeric_limits<double>::quiet_NaN(),1.0); const double zBefore=core.snapshot().playerZ; for(int i=0;i<10;++i) core.advance(1.0/60.0); assert(core.snapshot().playerZ>zBefore);
    core.setMovementInput(0.0,0.0); core.setSprintHeld(false); for(int i=0;i<60;++i) core.advance(1.0/60.0); assert(core.snapshot().horizontalSpeed<0.001);
    core.testOnlySetAirborne(2.0,0.0); assert(!core.snapshot().grounded); for(int i=0;i<120;++i) core.advance(1.0/60.0); assert(core.snapshot().grounded); assert(std::abs(core.snapshot().playerY)<1e-9);
    core.triggerFire(); assert(core.snapshot().shotsFired==1);
    const auto beforeRollback=core.snapshot(); assert(!core.testOnlyExecuteInvariantViolation()); const auto afterRollback=core.snapshot(); assert(std::isfinite(afterRollback.playerX)); assert(afterRollback.playerX==beforeRollback.playerX); assert(afterRollback.playerZ==beforeRollback.playerZ);

    const auto diagnostics=core.diagnostics(); assert(diagnostics.integrity.commandsAdmitted>=12); assert(diagnostics.integrity.commandsCommitted>=9); assert(diagnostics.integrity.commandsRejected>=2); assert(diagnostics.integrity.commandsRolledBack>=1); assert(diagnostics.journalValid); assert(diagnostics.retainedBlackBoxFrames>0);
    EventRecord newest{}; assert(core.newestEvent(0,newest)); assert(newest.sequence==diagnostics.integrity.eventSequence); assert(newest.hash==diagnostics.journalHead);
    for(int i=0;i<400;++i) core.triggerFire(); for(int i=0;i<900;++i) core.advance(1.0/60.0); const auto bounded=core.diagnostics(); assert(bounded.retainedCommands==IntegrityCore::kCommandCapacity); assert(bounded.retainedEvents==IntegrityCore::kEventCapacity); assert(bounded.retainedBlackBoxFrames==EngineCore::kBlackBoxCapacity); assert(bounded.journalValid);
    BlackBoxFrame latest{},oldest{}; assert(core.newestBlackBoxFrame(0,latest)); assert(core.newestBlackBoxFrame(EngineCore::kBlackBoxCapacity-1,oldest)); assert(latest.simulationTick>=oldest.simulationTick); assert(std::isfinite(latest.velocityX)); assert(std::isfinite(latest.cameraHeight));
    std::cout << "METSE Character + Camera Foundation + Integrity Tests: PASS\n"; return 0;
}
