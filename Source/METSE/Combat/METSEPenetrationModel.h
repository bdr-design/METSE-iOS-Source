#pragma once

#include "CoreMinimal.h"
#include "Combat/METSEPenetrationProfile.h"
#include "METSEPenetrationModel.generated.h"

USTRUCT(BlueprintType)
struct FMETSEPenetrationContext
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite) float ImpactEnergyJoules = 0.0f;
    UPROPERTY(BlueprintReadWrite) float MaterialThicknessCm = 0.0f;
    UPROPERTY(BlueprintReadWrite) float ImpactAngleDegreesFromNormal = 0.0f;
};

USTRUCT(BlueprintType)
struct FMETSEPenetrationResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) bool bPenetrated = false;
    UPROPERTY(BlueprintReadOnly) bool bRicochetSuggested = false;
    UPROPERTY(BlueprintReadOnly) float EffectiveThicknessCm = 0.0f;
    UPROPERTY(BlueprintReadOnly) float EnergyAbsorbedJoules = 0.0f;
    UPROPERTY(BlueprintReadOnly) float ResidualEnergyJoules = 0.0f;
};

struct FMETSEPenetrationModel
{
    static bool IsProfileValid(const UMETSEPenetrationProfile* Profile);
    static FMETSEPenetrationResult Resolve(const UMETSEPenetrationProfile* Profile, const FMETSEPenetrationContext& Context);
};
