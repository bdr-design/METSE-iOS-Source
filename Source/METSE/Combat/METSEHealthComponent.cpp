#include "Combat/METSEHealthComponent.h"
#include "Net/UnrealNetwork.h"
UMETSEHealthComponent::UMETSEHealthComponent(){ PrimaryComponentTick.bCanEverTick=false; SetIsReplicatedByDefault(true); }
bool UMETSEHealthComponent::ApplyAuthoritativeDamage(float Damage)
{
    AActor* Owner=GetOwner(); if(!Owner || !Owner->HasAuthority() || Damage<=0.f) return false;
    const float Old=Health; Health=FMath::Clamp(Health-Damage,0.f,MaxHealth); if(!FMath::IsNearlyEqual(Old,Health)) OnHealthChanged.Broadcast(Health,Health-Old); return true;
}
void UMETSEHealthComponent::OnRep_Health(float OldHealth){ OnHealthChanged.Broadcast(Health,Health-OldHealth); }
void UMETSEHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const { Super::GetLifetimeReplicatedProps(Out); DOREPLIFETIME(UMETSEHealthComponent,Health); }
