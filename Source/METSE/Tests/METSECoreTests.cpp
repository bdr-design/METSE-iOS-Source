#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Performance/METSEPerformancePolicy.h"
#include "Performance/METSEWorldBudgetSubsystem.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMETSEPerformancePolicyTest,"METSE.Core.Performance.Policy",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMETSEPerformancePolicyTest::RunTest(const FString&){ TestEqual(TEXT("Headroom"),FMETSEPerformancePolicy::Evaluate(5.f,7.f,0),EMETSEPerformanceState::Headroom); TestEqual(TEXT("Balanced"),FMETSEPerformancePolicy::Evaluate(8.f,10.f,0),EMETSEPerformanceState::Balanced); TestEqual(TEXT("Sustained thermal"),FMETSEPerformancePolicy::Evaluate(5.f,7.f,2),EMETSEPerformanceState::Sustained); TestEqual(TEXT("Protection thermal"),FMETSEPerformancePolicy::Evaluate(5.f,7.f,3),EMETSEPerformanceState::Protection); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMETSEBudgetInvariantTest,"METSE.Core.Budget.MaxCombatants",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FMETSEBudgetInvariantTest::RunTest(const FString&){ FMETSEWorldBudget B; TestEqual(TEXT("Max combatants invariant"),B.MaxCombatants,32); TestTrue(TEXT("LOD radii monotonic"),B.FullAnimationRadiusM < B.ReducedIKRadiusM && B.ReducedIKRadiusM < B.LowUpdateRadiusM); return true; }
#endif
