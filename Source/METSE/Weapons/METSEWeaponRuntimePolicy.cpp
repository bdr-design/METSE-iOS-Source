#include "Weapons/METSEWeaponRuntimePolicy.h"
#include "Combat/METSEBallistics.h"
#include "Weapons/METSEWeaponDefinition.h"

bool FMETSEWeaponRuntimePolicy::IsDefinitionValid(const UMETSEWeaponDefinition* Definition)
{
    return Definition != nullptr &&
        !Definition->WeaponId.IsNone() &&
        FMath::IsFinite(Definition->FireIntervalSeconds) &&
        Definition->FireIntervalSeconds >= 0.03f &&
        Definition->FireIntervalSeconds <= 10.0f &&
        Definition->MagazineCapacity > 0 &&
        Definition->MagazineCapacity <= 500 &&
        FMath::IsFinite(Definition->AdsTransitionSeconds) &&
        Definition->AdsTransitionSeconds >= 0.0f &&
        Definition->AdsTransitionSeconds <= 5.0f &&
        FMETSEBallistics::IsValidProfile(Definition->Ballistics);
}

bool FMETSEWeaponRuntimePolicy::CanCommitShot(
    const UMETSEWeaponDefinition* Definition,
    const int32 MagazineRounds,
    const double ServerTimeSeconds,
    const double LastCommittedShotTimeSeconds)
{
    if (!IsDefinitionValid(Definition) || MagazineRounds <= 0 ||
        !FMath::IsFinite(ServerTimeSeconds) || !FMath::IsFinite(LastCommittedShotTimeSeconds) ||
        ServerTimeSeconds < 0.0)
    {
        return false;
    }

    if (LastCommittedShotTimeSeconds < 0.0)
    {
        return true;
    }

    const double Elapsed = ServerTimeSeconds - LastCommittedShotTimeSeconds;
    return Elapsed >= static_cast<double>(Definition->FireIntervalSeconds) - UE_DOUBLE_SMALL_NUMBER;
}
