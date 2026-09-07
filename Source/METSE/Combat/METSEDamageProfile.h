#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "METSEDamageProfile.generated.h"

UENUM(BlueprintType)
enum class EMETSEBodyRegion : uint8
{
    Head,
    Neck,
    Chest,
    Abdomen,
    Pelvis,
    Arm,
    Leg,
    HandFoot
};

UCLASS(BlueprintType)
class METSE_API UMETSEDamageProfile : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage", meta=(ClampMin="1.0"))
    float ReferenceEnergyJoules = 1800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage", meta=(ClampMin="0.0"))
    float BaseDamageAtReferenceEnergy = 35.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage", meta=(ClampMin="1.0"))
    float MaxDamagePerHit = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage|Regions", meta=(ClampMin="0.0"))
    float HeadMultiplier = 3.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage|Regions", meta=(ClampMin="0.0"))
    float NeckMultiplier = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage|Regions", meta=(ClampMin="0.0"))
    float ChestMultiplier = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage|Regions", meta=(ClampMin="0.0"))
    float AbdomenMultiplier = 1.15f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage|Regions", meta=(ClampMin="0.0"))
    float PelvisMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage|Regions", meta=(ClampMin="0.0"))
    float ArmMultiplier = 0.60f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage|Regions", meta=(ClampMin="0.0"))
    float LegMultiplier = 0.70f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage|Regions", meta=(ClampMin="0.0"))
    float HandFootMultiplier = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage|Armor", meta=(ClampMin="0.0", ClampMax="1.0"))
    float NonPenetratingEnergyFraction = 0.08f;
};
