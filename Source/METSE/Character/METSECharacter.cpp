#include "Character/METSECharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Combat/METSEHealthComponent.h"
AMETSECharacter::AMETSECharacter()
{
    PrimaryActorTick.bCanEverTick=false;
    bReplicates=true;
    GetCharacterMovement()->MaxWalkSpeed=400.f;
    SpringArm=CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm")); SpringArm->SetupAttachment(GetRootComponent()); SpringArm->TargetArmLength=0.f; SpringArm->bUsePawnControlRotation=true;
    Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("Camera")); Camera->SetupAttachment(SpringArm); Camera->bUsePawnControlRotation=false;
    Health=CreateDefaultSubobject<UMETSEHealthComponent>(TEXT("Health"));
}
void AMETSECharacter::SetupPlayerInputComponent(UInputComponent* IC)
{
    Super::SetupPlayerInputComponent(IC);
    IC->BindAxis("MoveForward",this,&AMETSECharacter::MoveForward); IC->BindAxis("MoveRight",this,&AMETSECharacter::MoveRight);
    IC->BindAxis("Turn",this,&AMETSECharacter::LookYaw); IC->BindAxis("LookUp",this,&AMETSECharacter::LookPitch);
    IC->BindAction("Sprint",IE_Pressed,this,&AMETSECharacter::SprintPressed); IC->BindAction("Sprint",IE_Released,this,&AMETSECharacter::SprintReleased);
    IC->BindAction("Crouch",IE_Pressed,this,&AMETSECharacter::CrouchPressed); IC->BindAction("Aim",IE_Pressed,this,&AMETSECharacter::AimPressed); IC->BindAction("Aim",IE_Released,this,&AMETSECharacter::AimReleased);
}
void AMETSECharacter::ApplyIntentLocally(){ GetCharacterMovement()->MaxWalkSpeed=Intent.bSprint?650.f:400.f; }
void AMETSECharacter::MoveForward(float V){ Intent.Move.Y=FMath::Clamp(V,-1.f,1.f); ApplyIntentLocally(); if(Controller&&V!=0.f) AddMovementInput(FRotationMatrix(FRotator(0,Controller->GetControlRotation().Yaw,0)).GetUnitAxis(EAxis::X),V); if(!HasAuthority()) ServerSetMovementIntent(Intent); }
void AMETSECharacter::MoveRight(float V){ Intent.Move.X=FMath::Clamp(V,-1.f,1.f); if(Controller&&V!=0.f) AddMovementInput(FRotationMatrix(FRotator(0,Controller->GetControlRotation().Yaw,0)).GetUnitAxis(EAxis::Y),V); if(!HasAuthority()) ServerSetMovementIntent(Intent); }
void AMETSECharacter::LookYaw(float V){ AddControllerYawInput(V); }
void AMETSECharacter::LookPitch(float V){ AddControllerPitchInput(V); }
void AMETSECharacter::SprintPressed(){ Intent.bSprint=true; ApplyIntentLocally(); if(!HasAuthority()) ServerSetMovementIntent(Intent); }
void AMETSECharacter::SprintReleased(){ Intent.bSprint=false; ApplyIntentLocally(); if(!HasAuthority()) ServerSetMovementIntent(Intent); }
void AMETSECharacter::CrouchPressed(){ if(Intent.Stance==EMETSEStance::Crouched){Intent.Stance=EMETSEStance::Standing;UnCrouch();}else{Intent.Stance=EMETSEStance::Crouched;Crouch();} if(!HasAuthority()) ServerSetMovementIntent(Intent); }
void AMETSECharacter::AimPressed(){Intent.bAim=true;if(!HasAuthority())ServerSetMovementIntent(Intent);} void AMETSECharacter::AimReleased(){Intent.bAim=false;if(!HasAuthority())ServerSetMovementIntent(Intent);}
void AMETSECharacter::ServerSetMovementIntent_Implementation(FMETSEMovementIntent NewIntent){ Intent=NewIntent; ApplyIntentLocally(); }
