#include "Combat/METSEImpactResolver.h"

#include "Combat/METSEBallistics.h"
#include "Combat/METSEDamageProfile.h"
#include "Combat/METSEPenetrationProfile.h"
#include "Weapons/METSEWeaponDefinition.h"

FMETSEImpactResult FMETSEImpactResolver::Resolve(
    const FMETSEBallisticProfile& Ballistics,
    const UMETSEPenetrationProfile* PenetrationProfile,
    const UMETSEDamageProfile* DamageProfile,
    const FMETSEImpactContext& Context)
{
    FMETSEImpactResult Result;

    if (!FMETSEBallistics::IsValidProfile(Ballistics) ||
        !FMETSEPenetrationModel::IsProfileValid(PenetrationProfile) ||
        !FMETSEDamageModel::IsProfileValid(DamageProfile) ||
        !FMath::IsFinite(Context.ProjectileVelocityMps) || Context.ProjectileVelocityMps <= 0.0f ||
        !FMath::IsFinite(Context.MaterialThicknessCm) || Context.MaterialThicknessCm < 0.0f ||
        !FMath::IsFinite(Context.ImpactAngleDegreesFromNormal) ||
        Context.ImpactAngleDegreesFromNormal < 0.0f || Context.ImpactAngleDegreesFromNormal >= 90.0f)
    {
        return Result;
    }

    Result.ImpactEnergyJoules = static_cast<float>(
        FMETSEBallistics::KineticEnergyJoules(Ballistics, Context.ProjectileVelocityMps));
    if (!FMath::IsFinite(Result.ImpactEnergyJoules) || Result.ImpactEnergyJoules <= 0.0f)
    {
        Result.ImpactEnergyJoules = 0.0f;
        return Result;
    }

    FMETSEPenetrationContext PenetrationContext;
    PenetrationContext.ImpactEnergyJoules = Result.ImpactEnergyJoules;
    PenetrationContext.MaterialThicknessCm = Context.MaterialThicknessCm;
    PenetrationContext.ImpactAngleDegreesFromNormal = Context.ImpactAngleDegreesFromNormal;
    Result.Penetration = FMETSEPenetrationModel::Resolve(PenetrationProfile, PenetrationContext);

    FMETSEDamageContext DamageContext;
    DamageContext.BodyRegion = Context.BodyRegion;
    DamageContext.bArmorDefeated = Result.Penetration.bPenetrated;
    DamageContext.ResidualEnergyJoules = Result.Penetration.bPenetrated
        ? Result.Penetration.ResidualEnergyJoules
        : Result.ImpactEnergyJoules;

    Result.Damage = FMETSEDamageModel::Resolve(DamageProfile, DamageContext);
    Result.bValid = true;
    return Result;
}
