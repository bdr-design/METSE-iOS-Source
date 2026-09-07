#pragma once

#include "CoreMinimal.h"

struct FMETSEBallisticProfile;

struct FMETSEBallistics
{
    static constexpr double StandardGravityMps2 = 9.80665;

    static bool IsValidProfile(const FMETSEBallisticProfile& Profile);
    static double KineticEnergyJoules(const FMETSEBallisticProfile& Profile, double VelocityMps);
    static double TimeToDistanceSeconds(double DistanceMeters, double HorizontalVelocityMps);
    static double VacuumDropMeters(const FMETSEBallisticProfile& Profile, double DistanceMeters, double HorizontalVelocityMps);
};
