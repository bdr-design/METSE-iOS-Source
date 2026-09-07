#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "METSEWorldBudgetSubsystem.generated.h"
USTRUCT(BlueprintType)
struct FMETSEWorldBudget
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 MaxCombatants = 32;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float FullAnimationRadiusM = 30.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float ReducedIKRadiusM = 80.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float LowUpdateRadiusM = 150.f;
};
UCLASS()
class METSE_API UMETSEWorldBudgetSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    const FMETSEWorldBudget& GetBudget() const { return Budget; }
    bool IsCombatantCountAllowed(int32 Count) const { return Count >= 0 && Count <= Budget.MaxCombatants; }
private:
    UPROPERTY() FMETSEWorldBudget Budget;
};
