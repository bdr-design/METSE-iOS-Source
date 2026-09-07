#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "METSEWeaponDefinition.generated.h"

USTRUCT(BlueprintType)
struct FMETSEBallisticProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ballistics")
    float ProjectileMassGrams = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ballistics")
    float MuzzleVelocityMps = 800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ballistics")
    float BallisticCoefficient = 0.30f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ballistics")
    float GravityScale = 1.0f;
};

UCLASS(BlueprintType)
class METSE_API UMETSEWeaponDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
    FName WeaponId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta=(ClampMin="0.01"))
    float FireIntervalSeconds = 0.10f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon", meta=(ClampMin="1"))
    int32 MagazineCapacity = 30;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Handling", meta=(ClampMin="0.0"))
    float AdsTransitionSeconds = 0.20f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Handling")
    float RecoilPitchImpulse = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Handling")
    float RecoilYawImpulse = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ballistics")
    FMETSEBallisticProfile Ballistics;
};
