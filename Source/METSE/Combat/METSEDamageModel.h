#pragma once

#include "CoreMinimal.h"
#include "Combat/METSEDamageProfile.h"
#include "METSEDamageModel.generated.h"

USTRUCT(BlueprintType)
struct FMETSEDamageContext
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite) EMETSEBodyRegion BodyRegion = EMETSEBodyRegion::Chest;
    UPROPERTY(BlueprintReadWrite) float ResidualEnergyJoules = 0.0f;
    UPROPERTY(BlueprintReadWrite) bool bArmorDefeated = true;
};

USTRUCT(BlueprintType)
struct FMETSEDamageResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) float HealthDamage = 0.0f;
    UPROPERTY(BlueprintReadOnly) float Bleeding01 = 0.0f;
    UPROPERTY(BlueprintReadOnly) float MobilityPenalty01 = 0.0f;
    UPROPERTY(BlueprintReadOnly) bool bCriticalRegion = false;
};

struct FMETSEDamageModel
{
    static bool IsProfileValid(const UMETSEDamageProfile* Profile);
    static float GetRegionMultiplier(const UMETSEDamageProfile* Profile, EMETSEBodyRegion Region);
    static FMETSEDamageResult Resolve(const UMETSEDamageProfile* Profile, const FMETSEDamageContext& Context);
};
