#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "METSEPenetrationProfile.generated.h"

UENUM(BlueprintType)
enum class EMETSESurfaceClass : uint8
{
    Wood,
    Glass,
    Concrete,
    Brick,
    Steel,
    VehicleBody,
    Sand,
    Drywall,
    Custom
};

UCLASS(BlueprintType)
class METSE_API UMETSEPenetrationProfile : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Surface")
    EMETSESurfaceClass SurfaceClass = EMETSESurfaceClass::Custom;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Penetration", meta=(ClampMin="0.0"))
    float ResistanceJoulesPerCm = 500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Penetration", meta=(ClampMin="0.0", ClampMax="1.0"))
    float MinimumCosineForThickness = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ricochet")
    bool bCanRicochet = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ricochet", meta=(ClampMin="0.0", ClampMax="89.0"))
    float RicochetAngleDegreesFromNormal = 70.0f;
};
