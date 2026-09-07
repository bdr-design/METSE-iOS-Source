#include "Combat/METSEHealthComponent.h"
#include "Net/UnrealNetwork.h"

UMETSEHealthComponent::UMETSEHealthComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

bool UMETSEHealthComponent::ApplyAuthoritativeDamage(const float Damage)
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority() || !FMath::IsFinite(Damage) || Damage <= 0.0f)
    {
        return false;
    }

    const float OldHealth = Health;
    Health = FMath::Clamp(Health - Damage, 0.0f, MaxHealth);
    if (FMath::IsNearlyEqual(OldHealth, Health))
    {
        return false;
    }

    OnHealthChanged.Broadcast(Health, Health - OldHealth);
    return true;
}

bool UMETSEHealthComponent::ApplyAuthoritativeDamageResult(const FMETSEDamageResult& DamageResult)
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority() ||
        !FMath::IsFinite(DamageResult.HealthDamage) || DamageResult.HealthDamage < 0.0f ||
        !FMath::IsFinite(DamageResult.Bleeding01) || DamageResult.Bleeding01 < 0.0f || DamageResult.Bleeding01 > 1.0f ||
        !FMath::IsFinite(DamageResult.MobilityPenalty01) || DamageResult.MobilityPenalty01 < 0.0f || DamageResult.MobilityPenalty01 > 1.0f)
    {
        return false;
    }

    const float OldHealth = Health;
    const float OldBleeding = Bleeding01;
    const float OldMobilityPenalty = MobilityPenalty01;

    const float NewHealth = FMath::Clamp(Health - DamageResult.HealthDamage, 0.0f, MaxHealth);
    const float NewBleeding = FMath::Max(Bleeding01, DamageResult.Bleeding01);
    const float NewMobilityPenalty = FMath::Max(MobilityPenalty01, DamageResult.MobilityPenalty01);

    const bool bHealthChanged = !FMath::IsNearlyEqual(OldHealth, NewHealth);
    const bool bConditionChanged = !FMath::IsNearlyEqual(OldBleeding, NewBleeding) ||
        !FMath::IsNearlyEqual(OldMobilityPenalty, NewMobilityPenalty);
    if (!bHealthChanged && !bConditionChanged)
    {
        return false;
    }

    // Commit all validated combat-state changes together.
    Health = NewHealth;
    Bleeding01 = NewBleeding;
    MobilityPenalty01 = NewMobilityPenalty;

    if (bHealthChanged)
    {
        OnHealthChanged.Broadcast(Health, Health - OldHealth);
    }
    if (bConditionChanged)
    {
        OnCombatConditionChanged.Broadcast(Bleeding01, MobilityPenalty01);
    }
    return true;
}

void UMETSEHealthComponent::OnRep_Health(const float OldHealth)
{
    OnHealthChanged.Broadcast(Health, Health - OldHealth);
}

void UMETSEHealthComponent::OnRep_CombatCondition()
{
    OnCombatConditionChanged.Broadcast(Bleeding01, MobilityPenalty01);
}

void UMETSEHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UMETSEHealthComponent, Health);
    DOREPLIFETIME(UMETSEHealthComponent, Bleeding01);
    DOREPLIFETIME(UMETSEHealthComponent, MobilityPenalty01);
}
