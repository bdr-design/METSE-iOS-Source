#pragma once

#include "CoreMinimal.h"
#include "Combat/METSEDamageModel.h"
#include "Combat/METSEPenetrationModel.h"
#include "METSEImpactResolver.generated.h"

struct FMETSEBallisticProfile;
class UMETSEDamageProfile;
class UMETSEPenetrationProfile;

USTRUCT(BlueprintType)
struct FMETSEImpactContext
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite) float ProjectileVelocityMps = 0.0f;
    UPROPERTY(BlueprintReadWrite) float MaterialThicknessCm = 0.0f;
    UPROPERTY(BlueprintReadWrite) float ImpactAngleDegreesFromNormal = 0.0f;
    UPROPERTY(BlueprintReadWrite) EMETSEBodyRegion BodyRegion = EMETSEBodyRegion::Chest;
};

USTRUCT(BlueprintType)
struct FMETSEImpactResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) float ImpactEnergyJoules = 0.0f;
    UPROPERTY(BlueprintReadOnly) FMETSEPenetrationResult Penetration;
    UPROPERTY(BlueprintReadOnly) FMETSEDamageResult Damage;
    UPROPERTY(BlueprintReadOnly) bool bValid = false;
};

struct FMETSEImpactResolver
{
    static FMETSEImpactResult Resolve(
        const FMETSEBallisticProfile& Ballistics,
        const UMETSEPenetrationProfile* PenetrationProfile,
        const UMETSEDamageProfile* DamageProfile,
        const FMETSEImpactContext& Context);
};
