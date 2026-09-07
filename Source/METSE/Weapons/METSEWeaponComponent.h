#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "METSEWeaponComponent.generated.h"

class UMETSEWeaponDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FMETSEShotCommitted,
    FName, WeaponId,
    int32, RemainingMagazineRounds);

UCLASS(ClassGroup=(METSE), meta=(BlueprintSpawnableComponent))
class METSE_API UMETSEWeaponComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMETSEWeaponComponent();

    UFUNCTION(BlueprintCallable, Category="METSE|Weapon")
    bool TryCommitShotAuthoritative();

    UFUNCTION(BlueprintCallable, Category="METSE|Weapon")
    bool ReloadMagazineAuthoritative();

    UFUNCTION(BlueprintPure, Category="METSE|Weapon")
    int32 GetCurrentMagazineRounds() const { return CurrentMagazineRounds; }

    UFUNCTION(BlueprintPure, Category="METSE|Weapon")
    const UMETSEWeaponDefinition* GetWeaponDefinition() const { return WeaponDefinition; }

    UPROPERTY(BlueprintAssignable, Category="METSE|Weapon")
    FMETSEShotCommitted OnShotCommitted;

protected:
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="METSE|Weapon")
    TObjectPtr<UMETSEWeaponDefinition> WeaponDefinition;

    UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category="METSE|Weapon")
    int32 CurrentMagazineRounds = 0;

private:
    double LastCommittedShotTimeSeconds = -1.0;
};
