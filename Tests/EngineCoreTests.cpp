#include "../Engine/Core/METSEEngineCore.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
int main(){
    metse::EngineCore core; assert(core.config().maxCombatants==32); assert(!core.setActiveCombatants(33)); assert(core.setActiveCombatants(16));
    core.advance(1.0/60.0); assert(core.snapshot().simulationTick==1);
    core.setMovementInput(1.0,0.0); for(int i=0;i<60;++i) core.advance(1.0/60.0); assert(core.snapshot().playerZ>4.0 && core.snapshot().playerZ<5.0);
    core.setMovementInput(0.0,0.0); const double z=core.snapshot().playerZ; for(int i=0;i<10;++i) core.advance(1.0/60.0); assert(std::abs(core.snapshot().playerZ-z)<1e-9);
    core.addLookInput(100.0,100.0); assert(core.snapshot().playerPitch<=1.15); core.triggerFire(); assert(core.snapshot().shotsFired==1);
    std::cout<<"METSE Engine Core Tests: PASS\n"; return 0;
}
