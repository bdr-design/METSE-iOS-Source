#include "Performance/METSEPerformanceGovernor.h"
void UMETSEPerformanceGovernor::Initialize(FSubsystemCollectionBase& Collection) { Super::Initialize(Collection); State = EMETSEPerformanceState::Balanced; }
void UMETSEPerformanceGovernor::ReportFrameTimes(float CpuMs, float GpuMs)
{
    constexpr float Alpha=.05f;
    if (!bHasFrameSample) { CpuEMA=CpuMs; GpuEMA=GpuMs; bHasFrameSample=true; }
    else { CpuEMA=FMath::Lerp(CpuEMA,CpuMs,Alpha); GpuEMA=FMath::Lerp(GpuEMA,GpuMs,Alpha); }
    Reevaluate();
}
void UMETSEPerformanceGovernor::ReportThermalLevel(int32 Level) { ThermalLevel=FMath::Clamp(Level,0,3); Reevaluate(); }
void UMETSEPerformanceGovernor::Reevaluate()
{
    const auto NewState=FMETSEPerformancePolicy::Evaluate(CpuEMA,GpuEMA,ThermalLevel);
    if(NewState!=State){ State=NewState; OnStateChanged.Broadcast(State); }
}
