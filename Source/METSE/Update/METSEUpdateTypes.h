#pragma once
#include "CoreMinimal.h"
#include "METSEUpdateTypes.generated.h"

UENUM(BlueprintType)
enum class EMETSEUpdateChannel : uint8 { Stable, Development, Experimental };

UENUM(BlueprintType)
enum class EMETSEUpdateState : uint8 { Idle, Checking, Downloading, Verifying, Staged, Mounting, Active, Failed };

USTRUCT(BlueprintType)
struct FMETSEUpdateManifest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString UpdateVersion;
    UPROPERTY(BlueprintReadOnly) FString Channel;
    UPROPERTY(BlueprintReadOnly) FString PackageUrl;
    UPROPERTY(BlueprintReadOnly) FString PackageSha256;
    UPROPERTY(BlueprintReadOnly) FString PackageType;
    UPROPERTY(BlueprintReadOnly) FString MinimumCoreVersion;
    UPROPERTY(BlueprintReadOnly) int32 RequiresAppBuild = 1;
    UPROPERTY(BlueprintReadOnly) int32 ContentSchema = 1;
    UPROPERTY(BlueprintReadOnly) int64 Sequence = 0;
    UPROPERTY(BlueprintReadOnly) int64 SizeBytes = 0;
    UPROPERTY(BlueprintReadOnly) bool bRestartRequired = false;
    UPROPERTY(BlueprintReadOnly) FString MountPoint;
};
