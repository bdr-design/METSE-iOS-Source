#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Combat/METSEBallistics.h"
#include "Core/METSEMovementIntent.h"
#include "Performance/METSEPerformancePolicy.h"
#include "Performance/METSEWorldBudgetSubsystem.h"
#include "Weapons/METSEWeaponDefinition.h"
#include "Weapons/METSEWeaponRuntimePolicy.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMETSEPerformancePolicyTest,
    "METSE.Core.Performance.Policy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMETSEPerformancePolicyTest::RunTest(const FString&)
{
    TestEqual(TEXT("Headroom"), FMETSEPerformancePolicy::Evaluate(5.f, 7.f, 0), EMETSEPerformanceState::Headroom);
    TestEqual(TEXT("Balanced"), FMETSEPerformancePolicy::Evaluate(8.f, 10.f, 0), EMETSEPerformanceState::Balanced);
    TestEqual(TEXT("Sustained thermal"), FMETSEPerformancePolicy::Evaluate(5.f, 7.f, 2), EMETSEPerformanceState::Sustained);
    TestEqual(TEXT("Protection thermal"), FMETSEPerformancePolicy::Evaluate(5.f, 7.f, 3), EMETSEPerformanceState::Protection);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMETSEBudgetInvariantTest,
    "METSE.Core.Budget.MaxCombatants",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMETSEBudgetInvariantTest::RunTest(const FString&)
{
    FMETSEWorldBudget Budget;
    TestEqual(TEXT("Max combatants invariant"), Budget.MaxCombatants, 32);
    TestTrue(
        TEXT("LOD radii monotonic"),
        Budget.FullAnimationRadiusM < Budget.ReducedIKRadiusM &&
        Budget.ReducedIKRadiusM < Budget.LowUpdateRadiusM);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMETSEMovementIntentSerializationTest,
    "METSE.Core.Network.MovementIntentRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMETSEMovementIntentSerializationTest::RunTest(const FString&)
{
    FMETSEMovementIntent Source;
    Source.Move = FVector2D(0.625f, -0.375f);
    Source.Look = FVector2D(2.25f, -1.75f);
    Source.Stance = EMETSEStance::Crouched;
    Source.bSprint = true;
    Source.bAim = true;
    Source.bFire = false;

    TArray<uint8> Bytes;
    FMemoryWriter Writer(Bytes, true);
    bool bWriteSuccess = false;
    Source.NetSerialize(Writer, nullptr, bWriteSuccess);
    Writer.Close();

    TestTrue(TEXT("Movement intent serialization succeeds"), bWriteSuccess);
    TestTrue(TEXT("Movement intent payload is bounded"), Bytes.Num() > 0 && Bytes.Num() <= 16);

    FMETSEMovementIntent Restored;
    FMemoryReader Reader(Bytes, true);
    bool bReadSuccess = false;
    Restored.NetSerialize(Reader, nullptr, bReadSuccess);
    Reader.Close();

    TestTrue(TEXT("Movement intent deserialization succeeds"), bReadSuccess);
    TestTrue(TEXT("Move X survives quantization"), FMath::IsNearlyEqual(Restored.Move.X, Source.Move.X, 0.001f));
    TestTrue(TEXT("Move Y survives quantization"), FMath::IsNearlyEqual(Restored.Move.Y, Source.Move.Y, 0.001f));
    TestTrue(TEXT("Look X survives quantization"), FMath::IsNearlyEqual(Restored.Look.X, Source.Look.X, 0.001f));
    TestTrue(TEXT("Look Y survives quantization"), FMath::IsNearlyEqual(Restored.Look.Y, Source.Look.Y, 0.001f));
    TestEqual(TEXT("Stance survives round trip"), Restored.Stance, Source.Stance);
    TestEqual(TEXT("Sprint survives round trip"), Restored.bSprint, Source.bSprint);
    TestEqual(TEXT("Aim survives round trip"), Restored.bAim, Source.bAim);
    TestEqual(TEXT("Fire survives round trip"), Restored.bFire, Source.bFire);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMETSEBallisticsMathTest,
    "METSE.Core.Combat.BallisticsMath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMETSEBallisticsMathTest::RunTest(const FString&)
{
    FMETSEBallisticProfile Profile;
    Profile.ProjectileMassGrams = 10.0f;
    Profile.MuzzleVelocityMps = 500.0f;
    Profile.BallisticCoefficient = 0.30f;
    Profile.GravityScale = 1.0f;

    TestTrue(TEXT("Baseline ballistic profile is valid"), FMETSEBallistics::IsValidProfile(Profile));
    TestTrue(TEXT("Zero-mass profile is rejected"), [&Profile]() { FMETSEBallisticProfile Invalid = Profile; Invalid.ProjectileMassGrams = 0.0f; return !FMETSEBallistics::IsValidProfile(Invalid); }());

    const double Energy = FMETSEBallistics::KineticEnergyJoules(Profile, 500.0);
    TestTrue(TEXT("Kinetic energy calculation"), FMath::IsNearlyEqual(Energy, 1250.0, 0.001));

    const double Time = FMETSEBallistics::TimeToDistanceSeconds(100.0, 500.0);
    TestTrue(TEXT("Time-to-distance calculation"), FMath::IsNearlyEqual(Time, 0.2, 0.000001));

    const double Drop = FMETSEBallistics::VacuumDropMeters(Profile, 100.0, 500.0);
    TestTrue(TEXT("Vacuum drop is deterministic and positive"), Drop > 0.19 && Drop < 0.20);
    TestEqual(TEXT("Invalid velocity returns zero energy"), FMETSEBallistics::KineticEnergyJoules(Profile, -1.0), 0.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMETSEWeaponRuntimePolicyTest,
    "METSE.Core.Combat.WeaponRuntimePolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMETSEWeaponRuntimePolicyTest::RunTest(const FString&)
{
    UMETSEWeaponDefinition* Definition = NewObject<UMETSEWeaponDefinition>();
    Definition->WeaponId = TEXT("test_rifle");
    Definition->FireIntervalSeconds = 0.10f;
    Definition->MagazineCapacity = 30;
    Definition->AdsTransitionSeconds = 0.20f;
    Definition->Ballistics.ProjectileMassGrams = 8.0f;
    Definition->Ballistics.MuzzleVelocityMps = 800.0f;
    Definition->Ballistics.BallisticCoefficient = 0.30f;
    Definition->Ballistics.GravityScale = 1.0f;

    TestTrue(TEXT("Valid weapon definition accepted"), FMETSEWeaponRuntimePolicy::IsDefinitionValid(Definition));
    TestTrue(TEXT("First shot is accepted"), FMETSEWeaponRuntimePolicy::CanCommitShot(Definition, 30, 10.0, -1.0));
    TestFalse(TEXT("Empty magazine is rejected"), FMETSEWeaponRuntimePolicy::CanCommitShot(Definition, 0, 10.0, -1.0));
    TestFalse(TEXT("Early follow-up shot is rejected"), FMETSEWeaponRuntimePolicy::CanCommitShot(Definition, 29, 10.05, 10.0));
    TestTrue(TEXT("Cadence-compliant follow-up shot is accepted"), FMETSEWeaponRuntimePolicy::CanCommitShot(Definition, 29, 10.10, 10.0));
    TestFalse(TEXT("Clock rollback is rejected"), FMETSEWeaponRuntimePolicy::CanCommitShot(Definition, 29, 9.0, 10.0));

    Definition->MagazineCapacity = 0;
    TestFalse(TEXT("Invalid magazine capacity rejected"), FMETSEWeaponRuntimePolicy::IsDefinitionValid(Definition));
    return true;
}
#endif
