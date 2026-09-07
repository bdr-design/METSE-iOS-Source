#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/METSEMovementIntent.h"
#include "METSECharacter.generated.h"
class UCameraComponent; class USpringArmComponent; class UMETSEHealthComponent;
UCLASS()
class METSE_API AMETSECharacter : public ACharacter
{
    GENERATED_BODY()
public:
    AMETSECharacter();
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
protected:
    void MoveForward(float Value); void MoveRight(float Value); void LookYaw(float Value); void LookPitch(float Value);
    void SprintPressed(); void SprintReleased(); void CrouchPressed(); void AimPressed(); void AimReleased();
    void ApplyIntentLocally();
    void MaybeSendIntent(bool bForce=false);
    UFUNCTION(Server,Unreliable) void ServerSetMovementIntent(FMETSEMovementIntent NewIntent);
    UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> SpringArm;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UMETSEHealthComponent> Health;
    FMETSEMovementIntent Intent;
    double LastIntentSendSeconds=-1.0;
    static constexpr double IntentSendIntervalSeconds=0.05;
};
