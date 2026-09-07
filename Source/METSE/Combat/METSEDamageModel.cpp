#include "Combat/METSEDamageModel.h"

namespace METSEDamage
{
    static bool IsFiniteNonNegative(const float Value)
    {
        return FMath::IsFinite(Value) && Value >= 0.0f;
    }

    static float BleedingRegionFactor(const EMETSEBodyRegion Region)
    {
        switch (Region)
        {
        case EMETSEBodyRegion::Head: return 1.0f;
        case EMETSEBodyRegion::Neck: return 1.0f;
        case EMETSEBodyRegion::Chest: return 0.90f;
        case EMETSEBodyRegion::Abdomen: return 0.85f;
        case EMETSEBodyRegion::Pelvis: return 0.75f;
        case EMETSEBodyRegion::Arm: return 0.55f;
        case EMETSEBodyRegion::Leg: return 0.65f;
        case EMETSEBodyRegion::HandFoot: return 0.35f;
        default: return 0.0f;
        }
    }

    static float MobilityRegionFactor(const EMETSEBodyRegion Region)
    {
        switch (Region)
        {
        case EMETSEBodyRegion::Pelvis: return 0.80f;
        case EMETSEBodyRegion::Leg: return 1.0f;
        case EMETSEBodyRegion::HandFoot: return 0.60f;
        case EMETSEBodyRegion::Abdomen: return 0.15f;
        default: return 0.0f;
        }
    }
}

bool FMETSEDamageModel::IsProfileValid(const UMETSEDamageProfile* Profile)
{
    if (!Profile) return false;

    return FMath::IsFinite(Profile->ReferenceEnergyJoules) && Profile->ReferenceEnergyJoules > 0.0f &&
        FMath::IsFinite(Profile->BaseDamageAtReferenceEnergy) && Profile->BaseDamageAtReferenceEnergy >= 0.0f &&
        FMath::IsFinite(Profile->MaxDamagePerHit) && Profile->MaxDamagePerHit > 0.0f &&
        Profile->BaseDamageAtReferenceEnergy <= Profile->MaxDamagePerHit &&
        METSEDamage::IsFiniteNonNegative(Profile->HeadMultiplier) &&
        METSEDamage::IsFiniteNonNegative(Profile->NeckMultiplier) &&
        METSEDamage::IsFiniteNonNegative(Profile->ChestMultiplier) &&
        METSEDamage::IsFiniteNonNegative(Profile->AbdomenMultiplier) &&
        METSEDamage::IsFiniteNonNegative(Profile->PelvisMultiplier) &&
        METSEDamage::IsFiniteNonNegative(Profile->ArmMultiplier) &&
        METSEDamage::IsFiniteNonNegative(Profile->LegMultiplier) &&
        METSEDamage::IsFiniteNonNegative(Profile->HandFootMultiplier) &&
        FMath::IsFinite(Profile->NonPenetratingEnergyFraction) &&
        Profile->NonPenetratingEnergyFraction >= 0.0f && Profile->NonPenetratingEnergyFraction <= 1.0f;
}

float FMETSEDamageModel::GetRegionMultiplier(const UMETSEDamageProfile* Profile, const EMETSEBodyRegion Region)
{
    if (!IsProfileValid(Profile)) return 0.0f;

    switch (Region)
    {
    case EMETSEBodyRegion::Head: return Profile->HeadMultiplier;
    case EMETSEBodyRegion::Neck: return Profile->NeckMultiplier;
    case EMETSEBodyRegion::Chest: return Profile->ChestMultiplier;
    case EMETSEBodyRegion::Abdomen: return Profile->AbdomenMultiplier;
    case EMETSEBodyRegion::Pelvis: return Profile->PelvisMultiplier;
    case EMETSEBodyRegion::Arm: return Profile->ArmMultiplier;
    case EMETSEBodyRegion::Leg: return Profile->LegMultiplier;
    case EMETSEBodyRegion::HandFoot: return Profile->HandFootMultiplier;
    default: return 0.0f;
    }
}

FMETSEDamageResult FMETSEDamageModel::Resolve(const UMETSEDamageProfile* Profile, const FMETSEDamageContext& Context)
{
    FMETSEDamageResult Result;
    if (!IsProfileValid(Profile) || !FMath::IsFinite(Context.ResidualEnergyJoules) || Context.ResidualEnergyJoules <= 0.0f)
    {
        return Result;
    }

    const float TransferFraction = Context.bArmorDefeated ? 1.0f : Profile->NonPenetratingEnergyFraction;
    const float EffectiveEnergy = Context.ResidualEnergyJoules * TransferFraction;
    const float EnergyRatio = FMath::Clamp(EffectiveEnergy / Profile->ReferenceEnergyJoules, 0.0f, 3.0f);
    const float RegionMultiplier = GetRegionMultiplier(Profile, Context.BodyRegion);

    Result.HealthDamage = FMath::Clamp(
        Profile->BaseDamageAtReferenceEnergy * EnergyRatio * RegionMultiplier,
        0.0f,
        Profile->MaxDamagePerHit);

    const float DamageSeverity = FMath::Clamp(Result.HealthDamage / Profile->MaxDamagePerHit, 0.0f, 1.0f);
    Result.Bleeding01 = FMath::Clamp(DamageSeverity * METSEDamage::BleedingRegionFactor(Context.BodyRegion), 0.0f, 1.0f);
    Result.MobilityPenalty01 = FMath::Clamp(DamageSeverity * METSEDamage::MobilityRegionFactor(Context.BodyRegion), 0.0f, 1.0f);
    Result.bCriticalRegion = Context.BodyRegion == EMETSEBodyRegion::Head ||
        Context.BodyRegion == EMETSEBodyRegion::Neck ||
        Context.BodyRegion == EMETSEBodyRegion::Chest;
    return Result;
}
