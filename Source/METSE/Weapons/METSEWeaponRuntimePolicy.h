#pragma once

#include "CoreMinimal.h"

class UMETSEWeaponDefinition;

struct FMETSEWeaponRuntimePolicy
{
    static bool IsDefinitionValid(const UMETSEWeaponDefinition* Definition);
    static bool CanCommitShot(
        const UMETSEWeaponDefinition* Definition,
        int32 MagazineRounds,
        double ServerTimeSeconds,
        double LastCommittedShotTimeSeconds);
};
