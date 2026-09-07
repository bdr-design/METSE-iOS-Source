#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "METSEPerformancePolicy.h"
#include "METSEPerformanceGovernor.generated.h"
DECLARE_MULTICAST_DELEGATE_OneParam(FOnMETSEPerformanceStateChanged, EMETSEPerformanceState);
UCLASS()
class METSE_API UMETSEPerformanceGovernor : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    void ReportFrameTimes(float CpuMs, float GpuMs);
    void ReportThermalLevel(int32 Level);
    EMETSEPerformanceState GetState() const { return State; }
    FOnMETSEPerformanceStateChanged OnStateChanged;
private:
    void Reevaluate();
    EMETSEPerformanceState State = EMETSEPerformanceState::Balanced;
    float CpuEMA = 0.f, GpuEMA = 0.f;
    int32 ThermalLevel = 0;
    bool bHasFrameSample = false;
};
