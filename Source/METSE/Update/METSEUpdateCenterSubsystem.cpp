#include "Update/METSEUpdateCenterSubsystem.h"

#include "GenericPlatform/GenericPlatformMisc.h"
#include "HAL/FileManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace METSEUpdate
{
    static constexpr int32 CoreBuild = 1;
    static constexpr int32 ContentSchema = 1;
    static constexpr int64 MaxPackageBytes = 32ll * 1024ll * 1024ll;
    static constexpr int32 ActivePakOrder = 120;
    static constexpr int32 RollbackPakOrder = 130;

    static bool IsSafeVersionString(const FString& Value)
    {
        if (Value.IsEmpty() || Value.Len() > 64) return false;
        for (const TCHAR C : Value)
        {
            if (!(FChar::IsAlnum(C) || C == TCHAR('.') || C == TCHAR('-') || C == TCHAR('_'))) return false;
        }
        return true;
    }

    static bool IsHexSha256(const FString& Value)
    {
        if (Value.Len() != 64) return false;
        for (const TCHAR C : Value) if (!FChar::IsHexDigit(C)) return false;
        return true;
    }
}

void UMETSEUpdateCenterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    LoadLocalState();

    if (ActivePakPath.IsEmpty()) return;

    FString Error;
    if (IFileManager::Get().FileExists(*ActivePakPath) && MountPakFailClosed(ActivePakPath, Error))
    {
        SetState(EMETSEUpdateState::Active, FString::Printf(TEXT("Restored content update %s."), *ActiveContentVersion));
        return;
    }

    // Last active package is unusable. Attempt the previous known-good package before falling back to base.
    if (!PreviousPakPath.IsEmpty() && IFileManager::Get().FileExists(*PreviousPakPath) && FCoreDelegates::MountPak.IsBound())
    {
        if (FCoreDelegates::MountPak.Execute(PreviousPakPath, METSEUpdate::RollbackPakOrder) != nullptr)
        {
            ActivePakPath = PreviousPakPath;
            ActiveContentVersion = PreviousContentVersion.IsEmpty() ? TEXT("previous") : PreviousContentVersion;
            PreviousPakPath.Reset();
            PreviousContentVersion.Reset();
            SaveLocalState();
            SetState(EMETSEUpdateState::Active, FString::Printf(TEXT("Recovered previous content %s."), *ActiveContentVersion));
            return;
        }
    }

    ActivePakPath.Reset();
    PreviousPakPath.Reset();
    ActiveContentVersion = TEXT("base");
    PreviousContentVersion.Reset();
    SaveLocalState();
    SetState(EMETSEUpdateState::Failed, Error.IsEmpty() ? TEXT("Saved update could not be restored; using base content.") : Error);
}

void UMETSEUpdateCenterSubsystem::SetState(const EMETSEUpdateState NewState, const FString& Message)
{
    State = NewState;
    OnStateChanged.Broadcast(State, Message);
}

void UMETSEUpdateCenterSubsystem::CheckForUpdates(const FString& ManifestUrl)
{
    if (State == EMETSEUpdateState::Downloading || State == EMETSEUpdateState::Mounting)
    {
        SetState(EMETSEUpdateState::Failed, TEXT("Update operation already in progress."));
        return;
    }
    if (!ManifestUrl.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase))
    {
        SetState(EMETSEUpdateState::Failed, TEXT("Only HTTPS manifests are accepted."));
        return;
    }

    SetState(EMETSEUpdateState::Checking, TEXT("Checking manifest..."));
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetVerb(TEXT("GET"));
    Request->SetURL(ManifestUrl);
    Request->SetTimeout(30.0f);
    Request->OnProcessRequestComplete().BindWeakLambda(this, [this](FHttpRequestPtr, FHttpResponsePtr Response, bool bSucceeded)
    {
        if (!bSucceeded || !Response.IsValid() || Response->GetResponseCode() != 200 || !Response->GetURL().StartsWith(TEXT("https://"), ESearchCase::IgnoreCase))
        {
            SetState(EMETSEUpdateState::Failed, TEXT("Manifest download failed."));
            return;
        }

        FMETSEUpdateManifest Parsed;
        FString Error;
        if (!ParseManifest(Response->GetContentAsString(), Parsed, Error) || !ValidateManifest(Parsed, Error))
        {
            SetState(EMETSEUpdateState::Failed, Error);
            return;
        }
        if (Parsed.UpdateVersion == ActiveContentVersion)
        {
            SetState(EMETSEUpdateState::Idle, TEXT("This content update is already active."));
            return;
        }

        PendingManifest = MoveTemp(Parsed);
        SetState(EMETSEUpdateState::Idle, FString::Printf(TEXT("Update %s is ready to download."), *PendingManifest.UpdateVersion));
    });
    if (!Request->ProcessRequest()) SetState(EMETSEUpdateState::Failed, TEXT("Failed to start manifest request."));
}

void UMETSEUpdateCenterSubsystem::InstallLastCheckedUpdate()
{
    FString Error;
    if (!ValidateManifest(PendingManifest, Error))
    {
        SetState(EMETSEUpdateState::Failed, Error);
        return;
    }
    if (PendingManifest.UpdateVersion == ActiveContentVersion)
    {
        SetState(EMETSEUpdateState::Failed, TEXT("Update is already active."));
        return;
    }
    DownloadPackage();
}

void UMETSEUpdateCenterSubsystem::DownloadPackage()
{
    SetState(EMETSEUpdateState::Downloading, TEXT("Downloading staged content..."));
    OnProgress.Broadcast(0.0f);

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetVerb(TEXT("GET"));
    Request->SetURL(PendingManifest.PackageUrl);
    Request->SetTimeout(120.0f);
    Request->OnProcessRequestComplete().BindWeakLambda(this, [this](FHttpRequestPtr, FHttpResponsePtr Response, bool bSucceeded)
    {
        if (!bSucceeded || !Response.IsValid() || Response->GetResponseCode() != 200 || !Response->GetURL().StartsWith(TEXT("https://"), ESearchCase::IgnoreCase))
        {
            SetState(EMETSEUpdateState::Failed, TEXT("Package download failed."));
            return;
        }

        const TArray<uint8>& Bytes = Response->GetContent();
        if (Bytes.Num() <= 0 || Bytes.Num() > METSEUpdate::MaxPackageBytes)
        {
            SetState(EMETSEUpdateState::Failed, TEXT("Package size is invalid or exceeds the 32 MiB V0.1 safety cap."));
            return;
        }
        if (PendingManifest.SizeBytes <= 0 || PendingManifest.SizeBytes != Bytes.Num())
        {
            SetState(EMETSEUpdateState::Failed, TEXT("Downloaded size does not match manifest."));
            return;
        }

        SetState(EMETSEUpdateState::Verifying, TEXT("Verifying SHA-256..."));
        FString Error;
        if (!VerifySha256(Bytes, PendingManifest.PackageSha256, Error))
        {
            SetState(EMETSEUpdateState::Failed, Error);
            return;
        }

        const FString UpdatesDir = FPaths::Combine(FPaths::ProjectPersistentDownloadDir(), TEXT("METSEUpdates"));
        IFileManager::Get().MakeDirectory(*UpdatesDir, true);
        const FString FinalPath = FPaths::Combine(UpdatesDir, FString::Printf(TEXT("content_%s_p.pak"), *PendingManifest.UpdateVersion));
        if (!AtomicWrite(FinalPath, Bytes, Error))
        {
            SetState(EMETSEUpdateState::Failed, Error);
            return;
        }

        if (!ActivePakPath.IsEmpty() && !FCoreDelegates::OnUnmountPak.IsBound())
        {
            IFileManager::Get().Delete(*FinalPath, false, true, true);
            SetState(EMETSEUpdateState::Failed, TEXT("Cannot safely replace active PAK because unmount handler is unavailable."));
            return;
        }

        SetState(EMETSEUpdateState::Staged, TEXT("Package verified and staged."));
        if (!MountPakFailClosed(FinalPath, Error))
        {
            IFileManager::Get().Delete(*FinalPath, false, true, true);
            SetState(EMETSEUpdateState::Failed, Error);
            return;
        }

        if (!ActivePakPath.IsEmpty() && !FCoreDelegates::OnUnmountPak.Execute(ActivePakPath))
        {
            if (FCoreDelegates::OnUnmountPak.IsBound()) FCoreDelegates::OnUnmountPak.Execute(FinalPath);
            IFileManager::Get().Delete(*FinalPath, false, true, true);
            SetState(EMETSEUpdateState::Failed, TEXT("Previous PAK could not be safely unmounted; update rolled back."));
            return;
        }

        PreviousContentVersion = ActiveContentVersion;
        PreviousPakPath = ActivePakPath;
        ActiveContentVersion = PendingManifest.UpdateVersion;
        ActivePakPath = FinalPath;
        SaveLocalState();
        SetState(EMETSEUpdateState::Active, FString::Printf(TEXT("Content update %s is active."), *ActiveContentVersion));
        OnProgress.Broadcast(1.0f);
    });
    if (!Request->ProcessRequest()) SetState(EMETSEUpdateState::Failed, TEXT("Failed to start package request."));
}

bool UMETSEUpdateCenterSubsystem::ParseManifest(const FString& JsonText, FMETSEUpdateManifest& OutManifest, FString& OutError) const
{
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        OutError = TEXT("Manifest JSON is invalid.");
        return false;
    }

    auto RequiredString = [&Root, &OutError](const TCHAR* Name, FString& Out) -> bool
    {
        if (!Root->TryGetStringField(Name, Out) || Out.IsEmpty())
        {
            OutError = FString::Printf(TEXT("Manifest field '%s' is missing."), Name);
            return false;
        }
        return true;
    };

    if (!RequiredString(TEXT("updateVersion"), OutManifest.UpdateVersion) ||
        !RequiredString(TEXT("channel"), OutManifest.Channel) ||
        !RequiredString(TEXT("packageUrl"), OutManifest.PackageUrl) ||
        !RequiredString(TEXT("packageSha256"), OutManifest.PackageSha256) ||
        !RequiredString(TEXT("packageType"), OutManifest.PackageType) ||
        !RequiredString(TEXT("minimumCoreVersion"), OutManifest.MinimumCoreVersion)) return false;

    double Number = 0.0;
    if (Root->TryGetNumberField(TEXT("requiresAppBuild"), Number)) OutManifest.RequiresAppBuild = static_cast<int32>(Number);
    if (Root->TryGetNumberField(TEXT("contentSchema"), Number)) OutManifest.ContentSchema = static_cast<int32>(Number);
    if (Root->TryGetNumberField(TEXT("sizeBytes"), Number)) OutManifest.SizeBytes = static_cast<int64>(Number);
    Root->TryGetBoolField(TEXT("restartRequired"), OutManifest.bRestartRequired);
    Root->TryGetStringField(TEXT("mountPoint"), OutManifest.MountPoint);
    return true;
}

bool UMETSEUpdateCenterSubsystem::ValidateManifest(const FMETSEUpdateManifest& Manifest, FString& OutError) const
{
    if (!METSEUpdate::IsSafeVersionString(Manifest.UpdateVersion)) { OutError = TEXT("Update version is missing or contains unsafe characters."); return false; }
    if (!Manifest.PackageUrl.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase)) { OutError = TEXT("Package URL must use HTTPS."); return false; }
    if (Manifest.PackageType != TEXT("content_patch")) { OutError = TEXT("Only content_patch packages are allowed."); return false; }
    if (!(Manifest.Channel == TEXT("stable") || Manifest.Channel == TEXT("development") || Manifest.Channel == TEXT("experimental"))) { OutError = TEXT("Update channel is invalid."); return false; }
    if (Manifest.MinimumCoreVersion != TEXT("0.1.0")) { OutError = TEXT("Patch minimumCoreVersion is incompatible."); return false; }
    if (Manifest.RequiresAppBuild != METSEUpdate::CoreBuild) { OutError = TEXT("Patch requires a different app/core build."); return false; }
    if (Manifest.ContentSchema != METSEUpdate::ContentSchema) { OutError = TEXT("Patch content schema is incompatible."); return false; }
    if (!METSEUpdate::IsHexSha256(Manifest.PackageSha256)) { OutError = TEXT("Manifest SHA-256 is invalid."); return false; }
    if (Manifest.SizeBytes <= 0 || Manifest.SizeBytes > METSEUpdate::MaxPackageBytes) { OutError = TEXT("Manifest package size violates the 32 MiB V0.1 safety cap."); return false; }
    return true;
}

bool UMETSEUpdateCenterSubsystem::VerifySha256(const TArray<uint8>& Data, const FString& ExpectedHex, FString& OutError) const
{
    FSHA256Signature Signature;
    if (!FPlatformMisc::GetSHA256Signature(Data.GetData(), static_cast<uint32>(Data.Num()), Signature))
    {
        OutError = TEXT("Platform SHA-256 calculation failed.");
        return false;
    }
    if (!Signature.ToString().Equals(ExpectedHex, ESearchCase::IgnoreCase))
    {
        OutError = TEXT("SHA-256 mismatch. Staged update rejected.");
        return false;
    }
    return true;
}

bool UMETSEUpdateCenterSubsystem::AtomicWrite(const FString& FinalPath, const TArray<uint8>& Data, FString& OutError) const
{
    const FString TempPath = FinalPath + TEXT(".staging");
    IFileManager::Get().Delete(*TempPath, false, true, true);
    if (!FFileHelper::SaveArrayToFile(Data, *TempPath))
    {
        OutError = TEXT("Could not write staging package.");
        return false;
    }
    if (IFileManager::Get().FileSize(*TempPath) != Data.Num())
    {
        IFileManager::Get().Delete(*TempPath, false, true, true);
        OutError = TEXT("Staging file size verification failed.");
        return false;
    }
    IFileManager::Get().Delete(*FinalPath, false, true, true);
    if (!IFileManager::Get().Move(*FinalPath, *TempPath, true, true, false, true))
    {
        IFileManager::Get().Delete(*TempPath, false, true, true);
        OutError = TEXT("Atomic commit of staged package failed.");
        return false;
    }
    return true;
}

bool UMETSEUpdateCenterSubsystem::MountPakFailClosed(const FString& PakPath, FString& OutError) const
{
    const_cast<UMETSEUpdateCenterSubsystem*>(this)->SetState(EMETSEUpdateState::Mounting, TEXT("Mounting verified package..."));
    if (!FCoreDelegates::MountPak.IsBound())
    {
        OutError = TEXT("Unreal PAK mount handler is unavailable. Update not activated.");
        return false;
    }
    if (FCoreDelegates::MountPak.Execute(PakPath, METSEUpdate::ActivePakOrder) == nullptr)
    {
        OutError = TEXT("PAK mount failed. Active content remains unchanged.");
        return false;
    }
    return true;
}

bool UMETSEUpdateCenterSubsystem::RollbackToPrevious()
{
    if (PreviousPakPath.IsEmpty() || !IFileManager::Get().FileExists(*PreviousPakPath))
    {
        SetState(EMETSEUpdateState::Failed, TEXT("No previous content package is available."));
        return false;
    }
    if (!FCoreDelegates::MountPak.IsBound() || !FCoreDelegates::OnUnmountPak.IsBound())
    {
        SetState(EMETSEUpdateState::Failed, TEXT("PAK mount/unmount handlers are unavailable."));
        return false;
    }
    if (FCoreDelegates::MountPak.Execute(PreviousPakPath, METSEUpdate::RollbackPakOrder) == nullptr)
    {
        SetState(EMETSEUpdateState::Failed, TEXT("Rollback mount failed."));
        return false;
    }
    if (!ActivePakPath.IsEmpty() && !FCoreDelegates::OnUnmountPak.Execute(ActivePakPath))
    {
        FCoreDelegates::OnUnmountPak.Execute(PreviousPakPath);
        SetState(EMETSEUpdateState::Failed, TEXT("Active package could not be unmounted; rollback cancelled."));
        return false;
    }

    Swap(ActivePakPath, PreviousPakPath);
    Swap(ActiveContentVersion, PreviousContentVersion);
    SaveLocalState();
    SetState(EMETSEUpdateState::Active, FString::Printf(TEXT("Rolled back to %s."), *ActiveContentVersion));
    return true;
}

void UMETSEUpdateCenterSubsystem::LoadLocalState()
{
    const FString StatePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("METSEUpdateState.ini"));
    GConfig->GetString(TEXT("Update"), TEXT("ActiveContentVersion"), ActiveContentVersion, StatePath);
    GConfig->GetString(TEXT("Update"), TEXT("PreviousContentVersion"), PreviousContentVersion, StatePath);
    GConfig->GetString(TEXT("Update"), TEXT("ActivePakPath"), ActivePakPath, StatePath);
    GConfig->GetString(TEXT("Update"), TEXT("PreviousPakPath"), PreviousPakPath, StatePath);
    if (ActiveContentVersion.IsEmpty()) ActiveContentVersion = TEXT("base");
}

void UMETSEUpdateCenterSubsystem::SaveLocalState() const
{
    const FString StatePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("METSEUpdateState.ini"));
    GConfig->SetString(TEXT("Update"), TEXT("ActiveContentVersion"), *ActiveContentVersion, StatePath);
    GConfig->SetString(TEXT("Update"), TEXT("PreviousContentVersion"), *PreviousContentVersion, StatePath);
    GConfig->SetString(TEXT("Update"), TEXT("ActivePakPath"), *ActivePakPath, StatePath);
    GConfig->SetString(TEXT("Update"), TEXT("PreviousPakPath"), *PreviousPakPath, StatePath);
    GConfig->Flush(false, StatePath);
}
