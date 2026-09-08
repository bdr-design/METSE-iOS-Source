#include "../Engine/Core/METSEBallisticsCore.hpp"
#include "../Engine/Core/METSECharacterMotor.hpp"
#include "../Engine/Core/METSEDamageCore.hpp"
#include "../Engine/Core/METSEEngineCore.hpp"
#include "../Engine/Core/METSEInputCommandQueue.hpp"
#include "../Engine/Core/METSEIntegrityCore.hpp"
#include "../Engine/Core/METSEObservatoryCore.hpp"
#include "../Engine/Core/METSETacticalAICore.hpp"
#include "../Engine/Core/METSEVisibilityCore.hpp"
#include "../Engine/Core/METSEWeaponCore.hpp"
#include "../Engine/Core/METSEWorldCollision.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

static double dist3(metse::Vec3 a,metse::Vec3 b){double x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;return std::sqrt(x*x+y*y+z*z);}

int main(){using namespace metse;
assert(sha256Hex(sha256("abc"))=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

InputCommandQueue q;assert(q.validate());for(int i=0;i<500;++i){assert(q.pushMove(1,0));assert(q.pushLook(.001,.001));}assert(q.size()<=4);assert(q.metrics().coalesced>900);assert(q.pushFire());assert(q.pushMove(.5,.2));assert(q.pushFire());InputCommand c{};std::uint64_t last=0;int fires=0;while(q.pop(c)){assert(c.sequence>last);last=c.sequence;if(c.kind==InputCommandKind::Fire)++fires;}assert(fires==2);
q.reset();for(std::size_t i=0;i<InputCommandQueue::kCapacity;++i)assert(q.pushFire());assert(!q.pushReload());assert(q.metrics().rejectedCritical==1);

WorldCollisionCore world;assert(world.validate());assert(world.obstacleCount()==6);auto standBlock=world.resolve(-3,8,0,8,.34,1.78);assert(standBlock.hitX||standBlock.hitZ);auto crouchPass=world.resolve(-3,8,0,8,.34,1.18);assert(!crouchPass.hitX&&!crouchPass.hitZ);assert(std::isfinite(world.clearanceHeightAt(0,8,.2)));
// low roof regression: crouch passes while standing is rejected.
auto rayHit=world.raycastSegment({13,1,-2},{27,1,-2});assert(rayHit.hit&&rayHit.material==WorldMaterial::Wood);

WeaponCore weapon;assert(weapon.validate());weapon.setAimHeld(true);for(int i=0;i<20;++i)weapon.fixedStep(1.0/60.0,0,0);assert(weapon.state().adsAlpha>.95);ShotSolution shot{};assert(weapon.fire({0,1.64,0},0,0,77,shot));assert(weapon.state().ammoInMagazine==29);assert(!weapon.fire({0,1.64,0},0,0,78,shot));for(int i=0;i<10;++i)weapon.fixedStep(1.0/60.0,0,0);assert(weapon.requestReload());for(int i=0;i<150;++i)weapon.fixedStep(1.0/60.0,0,0);assert(!weapon.state().reloading&&weapon.state().ammoInMagazine==30&&weapon.state().reserveAmmo==89);

// Aim Truth: center camera ray and projectile converge deterministically. Hidden
// visual recoil/sway must not change ballistic direction.
WeaponCore truth;ShotSolution preview{};Vec3 cam{0,1.64,0};assert(truth.previewShot(cam,0,0,9001,preview));assert(std::abs(preview.aimPoint.x)<1e-9&&std::abs(preview.aimPoint.y-1.64)<1e-9&&std::abs(preview.aimPoint.z-100.0)<1e-9);
for(double z: {25.0,50.0,100.0}){double lambda=(z-preview.origin.z)/preview.direction.z;Vec3 bullet{preview.origin.x+preview.direction.x*lambda,preview.origin.y+preview.direction.y*lambda,z};Vec3 crosshair{0,1.64,z};assert(dist3(bullet,crosshair)<0.36);}

DamageCore damage;assert(damage.targetCount()==2);const auto&t0=damage.targets()[0];const auto&t1=damage.targets()[1];assert(!world.raycastSegment(cam,{t0.position.x,1.05,t0.position.z}).hit);assert(!world.raycastSegment(cam,{t1.position.x,1.05,t1.position.z}).hit);
auto hit=damage.applySegment({t0.position.x,1.7,t0.position.z-1},{t0.position.x,1.7,t0.position.z+1},1300,1001);assert(hit.hit&&hit.region==HitRegion::Head&&hit.killed);assert(damage.totalKills()==1&&damage.validate());

// Build 009 Tactical AI contract: perception is LOS/FOV/hearing driven, memory
// decays deterministically, and agents never gain magical player knowledge.
TacticalAICore ai;const double facePlayer=std::atan2(-t0.position.x,-t0.position.z);assert(ai.syncAgent(0,1,t0.position,facePlayer,true));ai.fixedStep(1.0/60.0,world,{0,0,0},{0,0,0},0.0);auto air=ai.report();assert(air.activeAgents==1&&air.lineOfSightAgents==1&&air.engagedAgents==1&&air.squadOrder==AISquadOrder::Assault);assert(ai.validate());
ai.reset();assert(ai.syncAgent(0,1,t0.position,facePlayer+3.14159265358979323846,true));ai.fixedStep(1.0/60.0,world,{0,0,0},{0,0,0},0.0);air=ai.report();assert(air.lineOfSightAgents==0&&air.hearingAgents==0&&air.suspiciousAgents==0&&air.investigatingAgents==0&&air.engagedAgents==0&&air.squadOrder==AISquadOrder::Hold);
ai.fixedStep(1.0/60.0,world,{0,0,0},{0,0,0},1.0);air=ai.report();assert(air.hearingAgents==1&&air.investigatingAgents==1&&air.squadOrder==AISquadOrder::Search);
for(int i=0;i<600;++i)ai.fixedStep(1.0/60.0,world,{0,0,0},{0,0,0},0.0);air=ai.report();assert(air.suspiciousAgents==0&&air.investigatingAgents==0&&air.engagedAgents==0&&air.squadOrder==AISquadOrder::Hold);assert(ai.validate());

DamageCore aimDamage;BallisticsCore aimBallistics;const auto&aimTarget=aimDamage.targets()[0];double dx=aimTarget.position.x-cam.x,dz=aimTarget.position.z-cam.z,dy=1.2-cam.y;double yaw=std::atan2(dx,dz),pitch=std::atan2(dy,std::hypot(dx,dz));ShotSolution aimed{};assert(truth.previewShot(cam,yaw,pitch,9100,aimed));assert(aimBallistics.spawn(aimed));for(int i=0;i<20&&aimDamage.totalHits()==0;++i)aimBallistics.fixedStep(1.0/60.0,world,aimDamage);assert(aimDamage.totalHits()==1);assert(aimBallistics.metrics().targetImpacts==1);assert(aimBallistics.metrics().worldImpacts==0);

DamageCore ballisticDamage;BallisticsCore ballistics;ShotSolution fast{};fast.origin={-10,1.7,0};fast.direction={0,0,1};fast.muzzleVelocity=820;fast.massKg=.004;fast.correlationId=42;assert(ballistics.spawn(fast));for(int i=0;i<4;++i)ballistics.fixedStep(1.0/60.0,world,ballisticDamage);assert(ballistics.validate());assert(ballistics.metrics().terminalWorldImpacts<=ballistics.metrics().worldImpacts);

VisibilityCore vis;vis.syncTarget(0,1,{0,0,20},true);vis.syncTarget(1,2,{0,0,120},true);vis.update({0,1.6,0},0);auto vr=vis.report();assert(vr.full==1&&vr.minimal==1&&vis.validate());

ObservatoryCore obs;ObservatoryFrameInput oi{};for(int i=0;i<600;++i){oi.realDeltaSeconds=(i==599)?.05:1.0/60.0;oi.playerZ+=.05;oi.horizontalSpeed=3;oi.inputQueueDepth=i%8;oi.activeProjectiles=i%4;obs.observe(oi);}auto orp=obs.report();assert(orp.onePercentLowFPS>0&&orp.pointOnePercentLowFPS>0&&orp.p99FrameMilliseconds>=16.0&&orp.maxFrameMilliseconds>=49.9&&obs.validate());

EngineCore core;assert(core.config().maxCombatants==32);assert(!core.setActiveCombatants(33));assert(core.setActiveCombatants(16));for(int i=0;i<1000;++i){core.setMovementInput(1,0);core.addLookInput(.0002,0);}assert(core.diagnostics().inputQueue.highWatermark<=InputCommandQueue::kCapacity);for(int i=0;i<120;++i)core.advance(1.0/60.0);assert(core.snapshot().horizontalSpeed>4.0);
core.setAimHeld(true);for(int i=0;i<20;++i)core.advance(1.0/60.0);assert(core.snapshot().adsAlpha>.9);assert(core.triggerFire());core.advance(1.0/60.0);assert(core.snapshot().shotsFired==1&&core.snapshot().ammoInMagazine==29);
// Gameplay denials are normal domain outcomes, never journal/integrity failures.
auto rejectedBefore=core.diagnostics().integrity.commandsRejected;for(int i=0;i<4;++i){assert(core.triggerFire());core.advance(1.0/240.0);}auto denied=core.diagnostics();assert(denied.integrity.commandsRejected==rejectedBefore);assert(denied.gameplayDenials.fireCooldown>=1);
for(int i=0;i<20;++i)core.advance(1.0/60.0);assert(core.reloadWeapon());core.advance(1.0/60.0);assert(core.snapshot().reloading);for(int i=0;i<150;++i)core.advance(1.0/60.0);assert(!core.snapshot().reloading);

// Auto reload is decided only when the simulation owner drains Fire, not on UIKit.
EngineCore autoCore;for(int shotIndex=0;shotIndex<30;++shotIndex){assert(autoCore.triggerFire());autoCore.advance(1.0/60.0);for(int i=0;i<7;++i)autoCore.advance(1.0/60.0);}assert(autoCore.snapshot().ammoInMagazine==0);auto autoRejectedBefore=autoCore.diagnostics().integrity.commandsRejected;assert(autoCore.triggerFire());autoCore.advance(1.0/60.0);assert(autoCore.snapshot().reloading);assert(autoCore.diagnostics().gameplayDenials.autoReloadStarted==1);assert(autoCore.diagnostics().integrity.commandsRejected==autoRejectedBefore);for(int i=0;i<150;++i)autoCore.advance(1.0/60.0);assert(!autoCore.snapshot().reloading&&autoCore.snapshot().ammoInMagazine==30&&autoCore.snapshot().reserveAmmo==60);

core.reset();core.cycleStance();core.advance(1.0/60.0);assert(core.snapshot().stance==CharacterStance::Crouched);core.setMovementInput(1,0);core.addLookInput(0,0);for(int i=0;i<120;++i)core.advance(1.0/60.0);
assert(core.diagnostics().journalValid);assert(core.diagnostics().worldValid);assert(core.diagnostics().weaponValid);assert(core.diagnostics().inputQueueValid);

EngineCore a,b;for(int i=0;i<240;++i){if(i%40==0){a.triggerFire();b.triggerFire();}a.setMovementInput(.7,.2);b.setMovementInput(.7,.2);a.addLookInput(.001,-.0004);b.addLookInput(.001,-.0004);a.advance(1.0/60.0);b.advance(1.0/60.0);}assert(a.deterministicStateHash()==b.deterministicStateHash());

a.setMovementInput(std::numeric_limits<double>::quiet_NaN(),0);assert(a.diagnostics().inputQueue.rejectedInvalid>=1);auto before=a.snapshot();assert(!a.testOnlyExecuteInvariantViolation());auto after=a.snapshot();assert(before.playerX==after.playerX&&before.playerZ==after.playerZ&&a.diagnostics().integrity.commandsRolledBack>=1);
for(int i=0;i<900;++i)a.advance(1.0/60.0);assert(a.diagnostics().retainedBlackBoxFrames==EngineCore::kBlackBoxCapacity);assert(a.diagnostics().observatory.retainedFrames==ObservatoryCore::kFrameCapacity);
std::cout<<"METSE Build 009 Tactical Combat Foundation Tests: PASS\n";
std::cout<<"METSE Aim Truth + Integrity + Tactical AI Regression Tests: PASS\n";}
