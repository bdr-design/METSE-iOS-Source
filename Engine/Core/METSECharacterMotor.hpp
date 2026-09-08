#pragma once
#include <cstdint>
namespace metse {
enum class CharacterStance:std::uint8_t{Standing=0,Crouched=1,Prone=2};
enum class CharacterGait:std::uint8_t{Idle=0,Walk=1,Tactical=2,Jog=3,Sprint=4,Crouch=5,Crawl=6};
struct CharacterConfig{double walkSpeed=1.55,tacticalSpeed=2.65,jogSpeed=4.15,sprintSpeed=6.0,crouchSpeed=2.0,proneSpeed=0.82,backwardMultiplier=0.76,groundAcceleration=15.0,groundDeceleration=19.0,airAcceleration=3.0,gravity=18.0,bodyTurnRate=3.6,viewYawSoftLimit=0.60,viewYawHardLimit=1.22,maxPitch=1.10,standingEyeHeight=1.64,crouchedEyeHeight=1.08,proneEyeHeight=0.42,eyeHeightTransitionSpeed=3.0,capsuleRadius=0.34,standingCapsuleHeight=1.78,crouchedCapsuleHeight=1.18,proneCapsuleHeight=0.52;};
struct CharacterInput{double forward=0,strafe=0;bool sprintHeld=false;};
struct CharacterState{double x=0,y=0,z=0,velocityX=0,velocityY=0,velocityZ=0,bodyYaw=0,viewYawOffset=0,pitch=0,eyeHeight=1.64,cameraBobY=0,cameraRoll=0,cameraLean=0,landingOffset=0,stepPhase=0,previousHorizontalSpeed=0;CharacterStance stance=CharacterStance::Standing;CharacterGait gait=CharacterGait::Idle;bool grounded=true,sprinting=false;};
class CharacterMotor final{public:explicit CharacterMotor(CharacterConfig config={})noexcept;void reset()noexcept;void addLookInput(double yaw,double pitch)noexcept;void cycleStance()noexcept;void fixedStep(double dt,const CharacterInput& input)noexcept;void applyHorizontalCollision(double x,double z,bool hitX,bool hitZ)noexcept;[[nodiscard]]const CharacterConfig& config()const noexcept{return config_;}[[nodiscard]]const CharacterState& state()const noexcept{return state_;}[[nodiscard]]double cameraYaw()const noexcept;[[nodiscard]]double cameraHeight()const noexcept;[[nodiscard]]double horizontalSpeed()const noexcept;[[nodiscard]]double capsuleHeight()const noexcept;[[nodiscard]]bool validate()const noexcept;
#ifdef METSE_TESTING
void testOnlySetAirborne(double h,double vy)noexcept;
#endif
private:static double moveToward(double c,double t,double d)noexcept;static double wrapAngle(double r)noexcept;double targetEyeHeight()const noexcept;void updateGait(double magnitude,double forward,bool sprintHeld)noexcept;void updateCameraFeel(double dt,double strafe,double previousSpeed)noexcept;CharacterConfig config_{};CharacterState state_{};};
} // namespace metse
