#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Core/METSEMovementIntent.h"
#include "Performance/METSEPerformancePolicy.h"
#include "Performance/METSEWorldBudgetSubsystem.h"

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
#endif
