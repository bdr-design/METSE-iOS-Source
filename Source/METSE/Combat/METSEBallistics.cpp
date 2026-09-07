#include "Combat/METSEBallistics.h"
#include "Weapons/METSEWeaponDefinition.h"

bool FMETSEBallistics::IsValidProfile(const FMETSEBallisticProfile& Profile)
{
    return FMath::IsFinite(Profile.ProjectileMassGrams) &&
        FMath::IsFinite(Profile.MuzzleVelocityMps) &&
        FMath::IsFinite(Profile.BallisticCoefficient) &&
        FMath::IsFinite(Profile.GravityScale) &&
        Profile.ProjectileMassGrams > 0.0f && Profile.ProjectileMassGrams <= 100.0f &&
        Profile.MuzzleVelocityMps > 0.0f && Profile.MuzzleVelocityMps <= 2000.0f &&
        Profile.BallisticCoefficient > 0.0f && Profile.BallisticCoefficient <= 2.0f &&
        Profile.GravityScale >= 0.0f && Profile.GravityScale <= 4.0f;
}

double FMETSEBallistics::KineticEnergyJoules(const FMETSEBallisticProfile& Profile, const double VelocityMps)
{
    if (!IsValidProfile(Profile) || !FMath::IsFinite(VelocityMps) || VelocityMps <= 0.0)
    {
        return 0.0;
    }

    const double MassKg = static_cast<double>(Profile.ProjectileMassGrams) / 1000.0;
    return 0.5 * MassKg * VelocityMps * VelocityMps;
}

double FMETSEBallistics::TimeToDistanceSeconds(const double DistanceMeters, const double HorizontalVelocityMps)
{
    if (!FMath::IsFinite(DistanceMeters) || !FMath::IsFinite(HorizontalVelocityMps) ||
        DistanceMeters < 0.0 || HorizontalVelocityMps <= 0.0)
    {
        return 0.0;
    }

    return DistanceMeters / HorizontalVelocityMps;
}

double FMETSEBallistics::VacuumDropMeters(
    const FMETSEBallisticProfile& Profile,
    const double DistanceMeters,
    const double HorizontalVelocityMps)
{
    if (!IsValidProfile(Profile))
    {
        return 0.0;
    }

    const double TimeSeconds = TimeToDistanceSeconds(DistanceMeters, HorizontalVelocityMps);
    if (TimeSeconds <= 0.0)
    {
        return 0.0;
    }

    return 0.5 * StandardGravityMps2 * static_cast<double>(Profile.GravityScale) * TimeSeconds * TimeSeconds;
}
