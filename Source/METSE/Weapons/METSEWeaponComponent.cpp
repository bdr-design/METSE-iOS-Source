#include "Weapons/METSEWeaponComponent.h"
#include "Net/UnrealNetwork.h"
#include "Weapons/METSEWeaponDefinition.h"
#include "Weapons/METSEWeaponRuntimePolicy.h"

UMETSEWeaponComponent::UMETSEWeaponComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UMETSEWeaponComponent::BeginPlay()
{
    Super::BeginPlay();

    AActor* Owner = GetOwner();
    if (Owner && Owner->HasAuthority() && FMETSEWeaponRuntimePolicy::IsDefinitionValid(WeaponDefinition))
    {
        CurrentMagazineRounds = WeaponDefinition->MagazineCapacity;
    }
}

bool UMETSEWeaponComponent::TryCommitShotAuthoritative()
{
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!Owner || !World || !Owner->HasAuthority())
    {
        return false;
    }

    const double ServerTimeSeconds = static_cast<double>(World->GetTimeSeconds());
    if (!FMETSEWeaponRuntimePolicy::CanCommitShot(
            WeaponDefinition,
            CurrentMagazineRounds,
            ServerTimeSeconds,
            LastCommittedShotTimeSeconds))
    {
        return false;
    }

    --CurrentMagazineRounds;
    LastCommittedShotTimeSeconds = ServerTimeSeconds;
    OnShotCommitted.Broadcast(WeaponDefinition->WeaponId, CurrentMagazineRounds);
    return true;
}

bool UMETSEWeaponComponent::ReloadMagazineAuthoritative()
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority() || !FMETSEWeaponRuntimePolicy::IsDefinitionValid(WeaponDefinition))
    {
        return false;
    }

    if (CurrentMagazineRounds >= WeaponDefinition->MagazineCapacity)
    {
        return false;
    }

    CurrentMagazineRounds = WeaponDefinition->MagazineCapacity;
    return true;
}

void UMETSEWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UMETSEWeaponComponent, CurrentMagazineRounds);
}
