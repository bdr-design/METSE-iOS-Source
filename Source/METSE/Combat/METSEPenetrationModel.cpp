#include "Combat/METSEPenetrationModel.h"

bool FMETSEPenetrationModel::IsProfileValid(const UMETSEPenetrationProfile* Profile)
{
    return Profile != nullptr &&
        FMath::IsFinite(Profile->ResistanceJoulesPerCm) &&
        Profile->ResistanceJoulesPerCm >= 0.0f && Profile->ResistanceJoulesPerCm <= 1000000.0f &&
        FMath::IsFinite(Profile->MinimumCosineForThickness) &&
        Profile->MinimumCosineForThickness > 0.0f && Profile->MinimumCosineForThickness <= 1.0f &&
        FMath::IsFinite(Profile->RicochetAngleDegreesFromNormal) &&
        Profile->RicochetAngleDegreesFromNormal >= 0.0f && Profile->RicochetAngleDegreesFromNormal < 90.0f;
}

FMETSEPenetrationResult FMETSEPenetrationModel::Resolve(
    const UMETSEPenetrationProfile* Profile,
    const FMETSEPenetrationContext& Context)
{
    FMETSEPenetrationResult Result;
    if (!IsProfileValid(Profile) ||
        !FMath::IsFinite(Context.ImpactEnergyJoules) || Context.ImpactEnergyJoules <= 0.0f ||
        !FMath::IsFinite(Context.MaterialThicknessCm) || Context.MaterialThicknessCm < 0.0f ||
        !FMath::IsFinite(Context.ImpactAngleDegreesFromNormal) ||
        Context.ImpactAngleDegreesFromNormal < 0.0f || Context.ImpactAngleDegreesFromNormal >= 90.0f)
    {
        return Result;
    }

    const float AngleRadians = FMath::DegreesToRadians(Context.ImpactAngleDegreesFromNormal);
    const float AngleCosine = FMath::Max(FMath::Cos(AngleRadians), Profile->MinimumCosineForThickness);
    Result.EffectiveThicknessCm = Context.MaterialThicknessCm / AngleCosine;

    const float RequiredEnergy = Profile->ResistanceJoulesPerCm * Result.EffectiveThicknessCm;
    Result.EnergyAbsorbedJoules = FMath::Min(Context.ImpactEnergyJoules, RequiredEnergy);
    Result.ResidualEnergyJoules = FMath::Max(0.0f, Context.ImpactEnergyJoules - RequiredEnergy);
    Result.bPenetrated = Result.ResidualEnergyJoules > 0.0f || FMath::IsNearlyZero(RequiredEnergy);
    Result.bRicochetSuggested = !Result.bPenetrated &&
        Profile->bCanRicochet &&
        Context.ImpactAngleDegreesFromNormal >= Profile->RicochetAngleDegreesFromNormal;
    return Result;
}
