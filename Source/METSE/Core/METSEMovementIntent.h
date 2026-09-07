#pragma once
#include "CoreMinimal.h"
#include "METSEMovementIntent.generated.h"

UENUM(BlueprintType)
enum class EMETSEStance : uint8 { Standing, Crouched, Prone };

USTRUCT(BlueprintType)
struct FMETSEMovementIntent
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite) FVector2D Move = FVector2D::ZeroVector;
    UPROPERTY(BlueprintReadWrite) FVector2D Look = FVector2D::ZeroVector;
    UPROPERTY(BlueprintReadWrite) EMETSEStance Stance = EMETSEStance::Standing;
    UPROPERTY(BlueprintReadWrite) bool bSprint = false;
    UPROPERTY(BlueprintReadWrite) bool bAim = false;
    UPROPERTY(BlueprintReadWrite) bool bFire = false;

    bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
    {
        int16 MX = Ar.IsSaving() ? FMath::RoundToInt(FMath::Clamp(Move.X, -1.f, 1.f) * 32767.f) : 0;
        int16 MY = Ar.IsSaving() ? FMath::RoundToInt(FMath::Clamp(Move.Y, -1.f, 1.f) * 32767.f) : 0;
        int16 LX = Ar.IsSaving() ? FMath::RoundToInt(FMath::Clamp(Look.X, -8.f, 8.f) * 4095.f) : 0;
        int16 LY = Ar.IsSaving() ? FMath::RoundToInt(FMath::Clamp(Look.Y, -8.f, 8.f) * 4095.f) : 0;
        uint8 Packed = Ar.IsSaving() ? ((uint8(Stance)&0x3u) | (bSprint?0x4u:0u) | (bAim?0x8u:0u) | (bFire?0x10u:0u)) : 0u;
        Ar << MX << MY << LX << LY << Packed;
        if (Ar.IsLoading()) {
            Move = FVector2D(float(MX)/32767.f, float(MY)/32767.f);
            Look = FVector2D(float(LX)/4095.f, float(LY)/4095.f);
            Stance = EMETSEStance(Packed & 0x3u); bSprint=(Packed&0x4u)!=0; bAim=(Packed&0x8u)!=0; bFire=(Packed&0x10u)!=0;
        }
        bOutSuccess = !Ar.IsError(); return true;
    }
};
template<> struct TStructOpsTypeTraits<FMETSEMovementIntent> : public TStructOpsTypeTraitsBase2<FMETSEMovementIntent> { enum { WithNetSerializer = true }; };
