#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/METSEDamageProfile.h"
#include "Combat/METSEImpactResolver.h"
#include "Combat/METSEPenetrationProfile.h"
#include "Weapons/METSEWeaponDefinition.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMETSEImpactResolverTest,
    "METSE.Core.Combat.ImpactResolverPipeline",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMETSEImpactResolverTest::RunTest(const FString&)
{
    FMETSEBallisticProfile Ballistics;
    Ballistics.ProjectileMassGrams = 10.0f;
    Ballistics.MuzzleVelocityMps = 500.0f;
    Ballistics.BallisticCoefficient = 0.30f;
    Ballistics.GravityScale = 1.0f;

    UMETSEPenetrationProfile* Penetration = NewObject<UMETSEPenetrationProfile>();
    Penetration->ResistanceJoulesPerCm = 100.0f;
    Penetration->MinimumCosineForThickness = 0.15f;

    UMETSEDamageProfile* Damage = NewObject<UMETSEDamageProfile>();
    Damage->ReferenceEnergyJoules = 1250.0f;
    Damage->BaseDamageAtReferenceEnergy = 40.0f;
    Damage->MaxDamagePerHit = 100.0f;

    FMETSEImpactContext Context;
    Context.ProjectileVelocityMps = 500.0f;
    Context.MaterialThicknessCm = 2.0f;
    Context.ImpactAngleDegreesFromNormal = 0.0f;
    Context.BodyRegion = EMETSEBodyRegion::Chest;

    const FMETSEImpactResult Penetrating = FMETSEImpactResolver::Resolve(Ballistics, Penetration, Damage, Context);
    TestTrue(TEXT("Pipeline result is valid"), Penetrating.bValid);
    TestTrue(TEXT("Impact energy is calculated first"), FMath::IsNearlyEqual(Penetrating.ImpactEnergyJoules, 1250.0f, 0.01f));
    TestTrue(TEXT("Surface is penetrated"), Penetrating.Penetration.bPenetrated);
    TestTrue(TEXT("Residual energy is below impact energy"), Penetrating.Penetration.ResidualEnergyJoules < Penetrating.ImpactEnergyJoules);
    TestTrue(TEXT("Residual energy produces anatomical damage"), Penetrating.Damage.HealthDamage > 0.0f);

    Penetration->ResistanceJoulesPerCm = 1000.0f;
    Context.MaterialThicknessCm = 3.0f;
    const FMETSEImpactResult Stopped = FMETSEImpactResolver::Resolve(Ballistics, Penetration, Damage, Context);
    TestTrue(TEXT("Stopped pipeline result is still valid"), Stopped.bValid);
    TestFalse(TEXT("High resistance stops penetration"), Stopped.Penetration.bPenetrated);
    TestTrue(TEXT("Non-penetrating trauma is lower than penetrating damage"), Stopped.Damage.HealthDamage < Penetrating.Damage.HealthDamage);

    Context.ProjectileVelocityMps = -1.0f;
    const FMETSEImpactResult Invalid = FMETSEImpactResolver::Resolve(Ballistics, Penetration, Damage, Context);
    TestFalse(TEXT("Invalid velocity fails closed"), Invalid.bValid);
    TestEqual(TEXT("Invalid pipeline produces zero damage"), Invalid.Damage.HealthDamage, 0.0f);
    return true;
}
#endif
