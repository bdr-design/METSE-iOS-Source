#include "../Engine/Core/METSEEngineCore.hpp"
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>

namespace { bool track=false; std::size_t allocations=0; }
void* operator new(std::size_t n) {
    if(track) ++allocations;
    if(void* p=std::malloc(n==0?1:n)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }

int main(int argc,char** argv) {
    const int seconds=argc==2?std::atoi(argv[1]):7200;
    assert(seconds>=120 && seconds<=7200);
    metse::EngineCore first,second;
    assert(first.setActiveCombatants(32) && second.setActiveCombatants(32));
    const auto started=std::chrono::steady_clock::now();
    std::uint64_t rounds=1,loadedFrames=0,fullFrames=0;
    metse::EngineDiagnosticsCapture captured;
    track=true;
    for(int tick=0;tick<seconds*60;++tick){
        // Explicit 120-second rounds preserve deaths; no invulnerability or health bypass.
        if(tick>0 && tick%7200==0){
            first.reset(); second.reset();
            assert(first.setActiveCombatants(32) && second.setActiveCombatants(32));
            ++rounds;
        }
        for(auto* core:{&first,&second}){
            if(tick%120==0){ assert(core->setMovementInput(0.5,0.25)); assert(core->addLookInput(0.15,0.0)); }
            if(tick%12==0) assert(core->triggerFire());
            // Synthetic bounded projectile occupancy, not representative device rendering.
            for(std::uint32_t n=core->snapshot().activeProjectiles;n<96;++n){
                metse::ShotSolution shot{};
                shot.origin={5.0,100.0,0.0}; shot.direction={0.0,0.0,1.0};
                shot.muzzleVelocity=100.0; shot.massKg=0.004;
                shot.correlationId=1000000+static_cast<std::uint64_t>(tick)*128+n;
                assert(core->testOnlySpawnProjectile(shot));
            }
            core->advance(1.0/60.0);
            assert(core->snapshot().activeProjectiles<=128);
            assert(core->snapshot().combatantCount<=32);
        }
        const auto& s=first.snapshot();
        if(s.activeProjectiles>=64) ++loadedFrames;
        if(s.tacticalAI.activeAgents==31 && (s.playerCombatState==metse::CombatState::Effective ||
           s.playerCombatState==metse::CombatState::Wounded)) ++fullFrames;
        if(tick%600==0 || tick==seconds*60-1){
            first.captureDiagnostics(captured);
            const auto d=captured.finish();
            assert(d.stateHash==second.deterministicStateHash());
            assert(d.journalValid && d.worldValid && d.observatoryValid && d.inputQueueValid &&
                   d.weaponValid && d.ballisticsValid && d.damageValid && d.visibilityValid &&
                   d.tacticalAIValid && d.audioFXValid);
            assert(d.simulationInvariantRollbacks==0);
        }
    }
    track=false;
    assert(allocations==0);
    std::cout<<"HOST SOAK PASS simulated_seconds="<<seconds<<" rounds="<<rounds
             <<" projectile_load_seconds="<<loadedFrames/60.0<<" full_combat_load_seconds="<<fullFrames/60.0
             <<" ordinary_cpp_new_calls="<<allocations
             <<" wall_seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()
             <<"\nTwo deterministic real-core instances; no iPhone FPS/thermal/GPU/leak certification.\n";
}
