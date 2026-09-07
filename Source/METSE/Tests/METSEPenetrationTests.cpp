#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/METSEPenetrationModel.h"
#include "Combat/METSEPenetrationProfile.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMETSEPenetrationModelTest,
    "METSE.Core.Combat.PenetrationModel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMETSEPenetrationModelTest::RunTest(const FString&)
{
    UMETSEPenetrationProfile* Profile = NewObject<UMETSEPenetrationProfile>();
    Profile->ResistanceJoulesPerCm = 100.0f;
    Profile->MinimumCosineForThickness = 0.15f;
    Profile->bCanRicochet = true;
    Profile->RicochetAngleDegreesFromNormal = 70.0f;

    TestTrue(TEXT("Penetration profile is valid"), FMETSEPenetrationModel::IsProfileValid(Profile));

    FMETSEPenetrationContext Context;
    Context.ImpactEnergyJoules = 1000.0f;
    Context.MaterialThicknessCm = 2.0f;
    Context.ImpactAngleDegreesFromNormal = 0.0f;
    const FMETSEPenetrationResult Normal = FMETSEPenetrationModel::Resolve(Profile, Context);
    TestTrue(TEXT("Sufficient energy penetrates"), Normal.bPenetrated);
    TestTrue(TEXT("Penetrating result preserves residual energy"), Normal.ResidualEnergyJoules > 0.0f);

    Context.ImpactAngleDegreesFromNormal = 60.0f;
    const FMETSEPenetrationResult Oblique = FMETSEPenetrationModel::Resolve(Profile, Context);
    TestTrue(TEXT("Oblique impact increases effective thickness"), Oblique.EffectiveThicknessCm > Normal.EffectiveThicknessCm);
    TestTrue(TEXT("Oblique impact retains less energy"), Oblique.ResidualEnergyJoules < Normal.ResidualEnergyJoules);

    Context.ImpactEnergyJoules = 100.0f;
    Context.MaterialThicknessCm = 5.0f;
    Context.ImpactAngleDegreesFromNormal = 75.0f;
    const FMETSEPenetrationResult Stopped = FMETSEPenetrationModel::Resolve(Profile, Context);
    TestFalse(TEXT("Insufficient energy does not penetrate"), Stopped.bPenetrated);
    TestTrue(TEXT("High-angle stopped impact can suggest ricochet"), Stopped.bRicochetSuggested);
    TestEqual(TEXT("Stopped projectile has zero residual energy"), Stopped.ResidualEnergyJoules, 0.0f);

    Context.ImpactEnergyJoules = -1.0f;
    const FMETSEPenetrationResult Invalid = FMETSEPenetrationModel::Resolve(Profile, Context);
    TestFalse(TEXT("Invalid impact is rejected"), Invalid.bPenetrated);
    TestEqual(TEXT("Invalid impact has zero residual energy"), Invalid.ResidualEnergyJoules, 0.0f);
    return true;
}
#endif
