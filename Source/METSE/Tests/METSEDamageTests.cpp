#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/METSEDamageModel.h"
#include "Combat/METSEDamageProfile.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMETSEDamageModelTest,
    "METSE.Core.Combat.DamageModel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMETSEDamageModelTest::RunTest(const FString&)
{
    UMETSEDamageProfile* Profile = NewObject<UMETSEDamageProfile>();
    TestTrue(TEXT("Default damage profile is valid"), FMETSEDamageModel::IsProfileValid(Profile));

    FMETSEDamageContext Context;
    Context.ResidualEnergyJoules = Profile->ReferenceEnergyJoules;
    Context.bArmorDefeated = true;

    Context.BodyRegion = EMETSEBodyRegion::Head;
    const FMETSEDamageResult Head = FMETSEDamageModel::Resolve(Profile, Context);

    Context.BodyRegion = EMETSEBodyRegion::Chest;
    const FMETSEDamageResult Chest = FMETSEDamageModel::Resolve(Profile, Context);

    Context.BodyRegion = EMETSEBodyRegion::Arm;
    const FMETSEDamageResult Arm = FMETSEDamageModel::Resolve(Profile, Context);

    Context.BodyRegion = EMETSEBodyRegion::Leg;
    const FMETSEDamageResult Leg = FMETSEDamageModel::Resolve(Profile, Context);

    TestTrue(TEXT("Head damage exceeds chest damage"), Head.HealthDamage > Chest.HealthDamage);
    TestTrue(TEXT("Chest damage exceeds arm damage"), Chest.HealthDamage > Arm.HealthDamage);
    TestTrue(TEXT("Leg hit has stronger mobility effect than chest"), Leg.MobilityPenalty01 > Chest.MobilityPenalty01);
    TestTrue(TEXT("Head is a critical region"), Head.bCriticalRegion);
    TestFalse(TEXT("Leg is not a critical region"), Leg.bCriticalRegion);

    Context.BodyRegion = EMETSEBodyRegion::Chest;
    Context.bArmorDefeated = false;
    const FMETSEDamageResult NonPenetrating = FMETSEDamageModel::Resolve(Profile, Context);
    TestTrue(TEXT("Non-penetrating impact transfers less damage"), NonPenetrating.HealthDamage < Chest.HealthDamage);

    Context.ResidualEnergyJoules = -1.0f;
    const FMETSEDamageResult Invalid = FMETSEDamageModel::Resolve(Profile, Context);
    TestEqual(TEXT("Invalid energy resolves to zero damage"), Invalid.HealthDamage, 0.0f);
    TestEqual(TEXT("Invalid energy resolves to zero bleeding"), Invalid.Bleeding01, 0.0f);
    return true;
}
#endif
