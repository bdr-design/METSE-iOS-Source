#pragma once
#include "CoreMinimal.h"
#include "METSEPerformancePolicy.generated.h"
UENUM(BlueprintType)
enum class EMETSEPerformanceState : uint8 { Headroom, Balanced, Sustained, Protection };
struct FMETSEPerformancePolicy
{
    static EMETSEPerformanceState Evaluate(float CpuMs, float GpuMs, int32 ThermalLevel)
    {
        if (ThermalLevel >= 3) return EMETSEPerformanceState::Protection;
        if (ThermalLevel >= 2 || CpuMs > 12.f || GpuMs > 15.f) return EMETSEPerformanceState::Sustained;
        if (CpuMs < 7.f && GpuMs < 9.f) return EMETSEPerformanceState::Headroom;
        return EMETSEPerformanceState::Balanced;
    }
};
