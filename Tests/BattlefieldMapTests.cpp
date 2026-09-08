#include "../Engine/Core/METSETacticalAICore.hpp"
#include "../Engine/Core/METSEWorldCollision.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>

namespace {

bool sameObstacle(const metse::WorldObstacle& a,
                  double minX,double minY,double minZ,
                  double maxX,double maxY,double maxZ,
                  metse::WorldMaterial material) {
    return a.minX==minX && a.minY==minY && a.minZ==minZ &&
           a.maxX==maxX && a.maxY==maxY && a.maxZ==maxZ &&
           a.material==material;
}

void seedThirtyTwoAgents(metse::TacticalAICore& ai) {
    using namespace metse;
    for(std::size_t i=0;i<TacticalAICore::kMaxAgents;++i){
        const double z=-44.0+static_cast<double>(i)*2.35;
        const Vec3 position{5.0,0.0,z};
        const double facing=std::atan2(-position.x,-position.z);
        assert(ai.syncAgent(i,static_cast<std::uint32_t>(i+1),position,facing,true,true,1.0));
    }
}

} // namespace

int main(){
    using namespace metse;

    static_assert(WorldCollisionCore::kMaxObstacles==20);
    static_assert(WorldCollisionCore::kMaxCoverCandidates==80);
    static_assert(WorldCollisionCore::kMaxCoverCandidates<256);

    WorldCollisionCore world;
    assert(world.validate());
    assert(world.obstacleCount()==WorldCollisionCore::kMaxObstacles);
    assert(world.minWorldX()==-48.0 && world.maxWorldX()==48.0);
    assert(world.minWorldZ()==-48.0 && world.maxWorldZ()==48.0);

    // Legacy geometry identity is a hard compatibility contract for the existing
    // Aim Truth, crouch-clearance and material regressions.
    const auto& obstacles=world.obstacles();
    assert(sameObstacle(obstacles[0],6,0,12,10,2.8,17,WorldMaterial::Concrete));
    assert(sameObstacle(obstacles[1],-14,0,8,-12,2.2,24,WorldMaterial::Steel));
    assert(sameObstacle(obstacles[2],-4,0,24,3,3.4,31,WorldMaterial::Concrete));
    assert(sameObstacle(obstacles[3],14,0,-3,14.18,1.7,-1,WorldMaterial::Wood));
    assert(sameObstacle(obstacles[4],-20,0,-22,-11,3.0,-13,WorldMaterial::Concrete));
    assert(sameObstacle(obstacles[5],-2.8,1.34,6.0,2.8,1.65,10.5,WorldMaterial::Steel));

    // Every Material SSOT entry must be represented by real battlefield geometry.
    std::array<std::uint32_t,7> materialCounts{};
    for(std::size_t i=0;i<world.obstacleCount();++i){
        const auto index=static_cast<std::size_t>(obstacles[i].material);
        assert(index<materialCounts.size());
        ++materialCounts[index];
    }
    for(const auto count:materialCounts) assert(count>0);

    // The central x=5 lane is deliberately preserved as a long open traversal / LOS
    // lane across the whole map. A future geometry edit that silently closes it must fail.
    const auto openLane=world.raycastSegment({5.0,1.45,-44.0},{5.0,1.45,44.0});
    assert(!openLane.hit);

    // Sector truth: each authored battlefield zone must produce the expected nearest
    // material contact instead of merely existing as renderer decoration.
    const auto urban=world.raycastSegment({-44.0,1.40,20.0},{-24.0,1.40,20.0});
    assert(urban.hit && urban.obstacleIndex==6 && urban.material==WorldMaterial::Brick);

    const auto industrial=world.raycastSegment({18.0,1.30,-27.0},{40.0,1.30,-27.0});
    assert(industrial.hit && industrial.obstacleIndex==10 && industrial.material==WorldMaterial::Steel);

    const auto glass=world.raycastSegment({18.0,1.30,6.0},{28.0,1.30,6.0});
    assert(glass.hit && glass.obstacleIndex==13 && glass.material==WorldMaterial::Glass);

    const auto rocky=world.raycastSegment({-40.0,1.20,-31.5},{-10.0,1.20,-31.5});
    assert(rocky.hit && rocky.obstacleIndex==15 && rocky.material==WorldMaterial::Rock);

    const auto berm=world.raycastSegment({-12.0,1.00,-32.5},{6.0,1.00,-32.5});
    assert(berm.hit && berm.obstacleIndex==17 && berm.material==WorldMaterial::Soil);

    // Character collision must stop entry into the urban block at standing height.
    const auto blocked=world.resolve(-44.0,20.0,-34.0,20.0,0.34,1.72);
    assert(blocked.hitX || blocked.hitZ);
    assert(blocked.x<=-38.34+1e-8);

    // Preserve the two established camera-to-training-target rays from Build 008/009.
    assert(!world.raycastSegment({0.0,1.64,0.0},{-10.0,1.05,18.0}).hit);
    assert(!world.raycastSegment({0.0,1.64,0.0},{22.0,1.05,26.0}).hit);

    // Cover is derived from the expanded collision SSOT, not authored separately.
    assert(world.coverCandidateCount()>50);
    assert(world.coverCandidateCount()<=WorldCollisionCore::kMaxCoverCandidates);
    for(std::size_t i=0;i<world.coverCandidateCount();++i){
        const auto& candidate=world.coverCandidates()[i];
        assert(candidate.valid);
        assert(candidate.obstacleIndex<world.obstacleCount());
    }

    // 32-agent stress against the expanded map. Expensive decisions remain capped at
    // four per fixed step and repeated identical scenarios must be bit-for-bit stable.
    TacticalAICore a,b;
    seedThirtyTwoAgents(a);
    seedThirtyTwoAgents(b);
    constexpr int kSteps=240;
    for(int step=0;step<kSteps;++step){
        a.fixedStep(1.0/60.0,world,{0.0,0.0,0.0},{0.0,0.0,0.0},0.0);
        b.fixedStep(1.0/60.0,world,{0.0,0.0,0.0},{0.0,0.0,0.0},0.0);
    }
    assert(a.validate() && b.validate());
    const auto ar=a.report();
    const auto br=b.report();
    assert(ar.activeAgents==32 && br.activeAgents==32);
    assert(ar.decisionsExecuted<=static_cast<std::uint64_t>(kSteps)*TacticalAICore::kMaxDecisionsPerStep);
    assert(ar.decisionsExecuted==br.decisionsExecuted);
    for(std::size_t i=0;i<TacticalAICore::kMaxAgents;++i){
        const auto& lhs=a.agents()[i];
        const auto& rhs=b.agents()[i];
        assert(lhs.id==rhs.id);
        assert(lhs.position.x==rhs.position.x && lhs.position.y==rhs.position.y && lhs.position.z==rhs.position.z);
        assert(lhs.lastKnownPlayerPosition.x==rhs.lastKnownPlayerPosition.x);
        assert(lhs.lastKnownPlayerPosition.y==rhs.lastKnownPlayerPosition.y);
        assert(lhs.lastKnownPlayerPosition.z==rhs.lastKnownPlayerPosition.z);
        assert(lhs.action==rhs.action);
        assert(lhs.alert==rhs.alert);
        assert(lhs.perceptionSource==rhs.perceptionSource);
        assert(lhs.coverCandidateIndex==rhs.coverCandidateIndex);
        assert(lhs.actionSequence==rhs.actionSequence);
        assert(lhs.fireAuthorized==rhs.fireAuthorized);
    }

    std::cout<<"METSE Build 009-F Battlefield Map Tests: PASS\n";
}
