#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "METSEUpdateTypes.h"
#include "METSEUpdateCenterSubsystem.generated.h"
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMETSEUpdateStateChanged, EMETSEUpdateState, State, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMETSEUpdateProgress, float, Progress01);
UCLASS()
class METSE_API UMETSEUpdateCenterSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    UFUNCTION(BlueprintCallable, Category="METSE|Update") void CheckForUpdates(const FString& ManifestUrl);
    UFUNCTION(BlueprintCallable, Category="METSE|Update") void InstallLastCheckedUpdate();
    UFUNCTION(BlueprintCallable, Category="METSE|Update") bool RollbackToPrevious();
    UFUNCTION(BlueprintPure, Category="METSE|Update") EMETSEUpdateState GetState() const { return State; }
    UFUNCTION(BlueprintPure, Category="METSE|Update") FString GetActiveContentVersion() const { return ActiveContentVersion; }
    UPROPERTY(BlueprintAssignable) FMETSEUpdateStateChanged OnStateChanged;
    UPROPERTY(BlueprintAssignable) FMETSEUpdateProgress OnProgress;
private:
    bool ParseManifest(const FString& JsonText, FMETSEUpdateManifest& OutManifest, FString& OutError) const;
    bool ValidateManifest(const FMETSEUpdateManifest& Manifest, FString& OutError) const;
    bool VerifySha256(const TArray<uint8>& Data, const FString& ExpectedHex, FString& OutError) const;
    bool AtomicWrite(const FString& FinalPath, const TArray<uint8>& Data, FString& OutError) const;
    bool MountPakFailClosed(const FString& PakPath, FString& OutError) const;
    void SetState(EMETSEUpdateState NewState, const FString& Message);
    void LoadLocalState();
    void SaveLocalState() const;
    void DownloadPackage();
    EMETSEUpdateState State = EMETSEUpdateState::Idle;
    FMETSEUpdateManifest PendingManifest;
    FString ActiveContentVersion = TEXT("base");
    FString PreviousContentVersion;
    FString ActivePakPath;
    FString PreviousPakPath;
};
