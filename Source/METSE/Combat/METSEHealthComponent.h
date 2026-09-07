#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/METSEDamageModel.h"
#include "METSEHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMETSEHealthChanged, float, Health, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMETSECombatConditionChanged, float, Bleeding01, float, MobilityPenalty01);

UCLASS(ClassGroup=(METSE), meta=(BlueprintSpawnableComponent))
class METSE_API UMETSEHealthComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UMETSEHealthComponent();

    UFUNCTION(BlueprintCallable, Category="METSE|Health")
    bool ApplyAuthoritativeDamage(float Damage);

    UFUNCTION(BlueprintCallable, Category="METSE|Health")
    bool ApplyAuthoritativeDamageResult(const FMETSEDamageResult& DamageResult);

    UFUNCTION(BlueprintPure, Category="METSE|Health") float GetHealth() const { return Health; }
    UFUNCTION(BlueprintPure, Category="METSE|Health") float GetBleeding01() const { return Bleeding01; }
    UFUNCTION(BlueprintPure, Category="METSE|Health") float GetMobilityPenalty01() const { return MobilityPenalty01; }

    UPROPERTY(BlueprintAssignable) FMETSEHealthChanged OnHealthChanged;
    UPROPERTY(BlueprintAssignable) FMETSECombatConditionChanged OnCombatConditionChanged;

protected:
    UPROPERTY(EditDefaultsOnly, Category="METSE|Health") float MaxHealth = 100.0f;
    UPROPERTY(ReplicatedUsing=OnRep_Health) float Health = 100.0f;
    UPROPERTY(ReplicatedUsing=OnRep_CombatCondition) float Bleeding01 = 0.0f;
    UPROPERTY(ReplicatedUsing=OnRep_CombatCondition) float MobilityPenalty01 = 0.0f;

    UFUNCTION() void OnRep_Health(float OldHealth);
    UFUNCTION() void OnRep_CombatCondition();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
