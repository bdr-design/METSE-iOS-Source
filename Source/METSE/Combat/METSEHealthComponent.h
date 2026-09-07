#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "METSEHealthComponent.generated.h"
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMETSEHealthChanged,float,Health,float,Delta);
UCLASS(ClassGroup=(METSE),meta=(BlueprintSpawnableComponent))
class METSE_API UMETSEHealthComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UMETSEHealthComponent();
    UFUNCTION(BlueprintCallable) bool ApplyAuthoritativeDamage(float Damage);
    UFUNCTION(BlueprintPure) float GetHealth() const { return Health; }
    UPROPERTY(BlueprintAssignable) FMETSEHealthChanged OnHealthChanged;
protected:
    UPROPERTY(EditDefaultsOnly,Category="METSE|Health") float MaxHealth=100.f;
    UPROPERTY(ReplicatedUsing=OnRep_Health) float Health=100.f;
    UFUNCTION() void OnRep_Health(float OldHealth);
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
