#include "main.hpp"
#include "logger.hpp"
#include "util.hpp"

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

const float DEG2RAD_CONSTANT = M_PI / 180.0f;
const float RAD2DEG_CONSTANT = 180.0f / M_PI;

// --- Global Saber Transform References using SafePtrUnity ---
static SafePtrUnity<UnityEngine::Transform> leftSaberTransform;
static SafePtrUnity<UnityEngine::Transform> rightSaberTransform;

// --- Original Saber Local Poses (Quaternions and Vector3 are value types) ---
static UnityEngine::Quaternion originalLeftSaberLocalRotation;
static UnityEngine::Vector3 originalLeftSaberLocalPosition;
static UnityEngine::Quaternion originalRightSaberLocalRotation;
static UnityEngine::Vector3 originalRightSaberLocalPosition;

// --- Saber Interaction States ---
enum class SaberInteractionState { Held, Thrown, Returning };
static SaberInteractionState leftSaberState = SaberInteractionState::Held;
static SaberInteractionState rightSaberState = SaberInteractionState::Held;

// --- Original Parent References ---
static SafePtrUnity<UnityEngine::Transform> leftSaberOriginalParent;
static SafePtrUnity<UnityEngine::Transform> rightSaberOriginalParent;

// --- Controller Velocity & Simulated Throw State ---
static UnityEngine::Vector3 leftControllerVelocity;
static UnityEngine::Vector3 rightControllerVelocity;
static UnityEngine::Vector3 prevLeftHandPos;
static UnityEngine::Vector3 prevRightHandPos;

static UnityEngine::Vector3 leftSaberSimulatedWorldVelocity; // For thrown state
static UnityEngine::Vector3 rightSaberSimulatedWorldVelocity;
static UnityEngine::Vector3 leftSaberSimulatedWorldAngularVelocity; // Radians/sec
static UnityEngine::Vector3 rightSaberSimulatedWorldAngularVelocity;

// --- Player-Controlled Spin States ---
static bool leftSpinActive = false;
static bool rightSpinActive = false;

// --- Throw Button Edge Detection ---
static bool leftThrowButtonPressedLastFrame = false;
static bool rightThrowButtonPressedLastFrame = false;

// --- Saber Return Tweening ---
static float leftSaberReturnTime = 0.0f;
static float rightSaberReturnTime = 0.0f;
static UnityEngine::Vector3 leftSaberThrowReleasePosition; // World position when recall starts
static UnityEngine::Quaternion leftSaberThrowReleaseRotation; // World rotation when recall starts
static UnityEngine::Vector3 rightSaberThrowReleasePosition;
static UnityEngine::Quaternion rightSaberThrowReleaseRotation;

// --- Hand/Controller Transforms (for re-attachment and velocity calc) ---
static SafePtrUnity<UnityEngine::Transform> leftHandTransform;
static SafePtrUnity<UnityEngine::Transform> rightHandTransform;

// --- Misc Flags & Constants ---
static bool mainMenuHasLoaded = false; // Optional safety for saber init
const float MIN_NATURAL_ROTATION_RAD_PER_SEC = 3.14f;                 // Approx 0.5 RPS (base spin for any throw)
const float THROW_VELOCITY_TO_ROTATION_SCALE = 2.5f;                  // How much throw speed (m/s) contributes to rotation speed (rad/s).
const float MAX_NATURAL_ROTATION_FROM_VELOCITY_RAD_PER_SEC = 70.0f;   // Cap to prevent insane spins from velocity
const float MIN_THROW_SPEED_FOR_CROSS_PRODUCT_ROTATION = 0.2f;        // Min throw speed (m/s) to attempt cross-product rotation, else min spin.


// --- Helper function to map configured index to OVRInput::Button ---
GlobalNamespace::OVRInput::Button GetOVRButtonForConfig(int configuredButtonIndex, bool isLeftController) {
    if (configuredButtonIndex == 0) return GlobalNamespace::OVRInput::Button::None;
    if (isLeftController) {
        switch (configuredButtonIndex) {
            case 1: return GlobalNamespace::OVRInput::Button::One;
            case 2: return GlobalNamespace::OVRInput::Button::Two;
            case 3: return GlobalNamespace::OVRInput::Button::PrimaryIndexTrigger;
            case 4: return GlobalNamespace::OVRInput::Button::PrimaryHandTrigger;
            default: return GlobalNamespace::OVRInput::Button::None;
        }
    } else { // Right Controller
        switch (configuredButtonIndex) {
            case 1: return GlobalNamespace::OVRInput::Button::One;
            case 2: return GlobalNamespace::OVRInput::Button::Two;
            case 3: return GlobalNamespace::OVRInput::Button::PrimaryIndexTrigger;
            case 4: return GlobalNamespace::OVRInput::Button::PrimaryHandTrigger;
            default: return GlobalNamespace::OVRInput::Button::None;
        }
    }
}

// --- Hook for MainMenuViewController ---
MAKE_HOOK_MATCH(MainMenuViewController_DidActivate_Hook, &GlobalNamespace::MainMenuViewController::DidActivate, void,
    GlobalNamespace::MainMenuViewController *self, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
    MainMenuViewController_DidActivate_Hook(self, firstActivation, addedToHierarchy, screenSystemEnabling);
    if (firstActivation) {
        mainMenuHasLoaded = true;
    }

    if (leftSaberState != SaberInteractionState::Held || leftSpinActive) {
        if (leftSaberTransform && leftSaberOriginalParent) {
            leftSaberTransform->SetParent(leftSaberOriginalParent.ptr(), false);
            leftSaberTransform->set_localPosition(originalLeftSaberLocalPosition);
            leftSaberTransform->set_localRotation(originalLeftSaberLocalRotation);
        }
        leftSaberState = SaberInteractionState::Held;
        leftSpinActive = false;
        getLogger().info("[TS] [Menu] Left Saber state reset on Main Menu.");
    }
    if (rightSaberState != SaberInteractionState::Held || rightSpinActive) {
         if (rightSaberTransform && rightSaberOriginalParent) {
            rightSaberTransform->SetParent(rightSaberOriginalParent.ptr(), false);
            rightSaberTransform->set_localPosition(originalRightSaberLocalPosition);
            rightSaberTransform->set_localRotation(originalRightSaberLocalRotation);
        }
        rightSaberState = SaberInteractionState::Held;
        rightSpinActive = false;
        getLogger().info("[TS] [Menu] Right Saber state reset on Main Menu.");
    }
}

// --- Hook to find Sabers via SaberModelController::Init ---
MAKE_HOOK_MATCH(SaberModelController_Init_Hook, &GlobalNamespace::SaberModelController::Init, void,
    GlobalNamespace::SaberModelController* self, UnityEngine::Transform* parent, GlobalNamespace::Saber* saber, UnityEngine::Color color) {
    SaberModelController_Init_Hook(self, parent, saber, color);
    if (!saber) { getLogger().error("[TS] [SMC] Saber is null"); return; }
    UnityEngine::Transform* currentSaberActualTransform = saber->get_transform();
    if (!currentSaberActualTransform) { getLogger().error("[TS] [SMC] Saber transform is null"); return; }

    GlobalNamespace::SaberType saberType = saber->get_saberType();
    if (saberType == GlobalNamespace::SaberType::SaberA) { // Left
        leftSaberTransform = currentSaberActualTransform;
        if (leftSaberTransform) {
            originalLeftSaberLocalRotation = leftSaberTransform->get_localRotation();
            originalLeftSaberLocalPosition = leftSaberTransform->get_localPosition();
            leftSaberOriginalParent = leftSaberTransform->get_parent();
            leftHandTransform = leftSaberOriginalParent.ptr();
            leftSaberState = SaberInteractionState::Held;
            leftSpinActive = false;
            getLogger().info("[TS] [SMC] Found/Updated Left Saber (SaberA)");
        }
    } else if (saberType == GlobalNamespace::SaberType::SaberB) { // Right
        rightSaberTransform = currentSaberActualTransform;
        if (rightSaberTransform) {
            originalRightSaberLocalRotation = rightSaberTransform->get_localRotation();
            originalRightSaberLocalPosition = rightSaberTransform->get_localPosition();
            rightSaberOriginalParent = rightSaberTransform->get_parent();
            rightHandTransform = rightSaberOriginalParent.ptr();
            rightSaberState = SaberInteractionState::Held;
            rightSpinActive = false;
            getLogger().info("[TS] [SMC] Found/Updated Right Saber (SaberB)");
        }
    }
}

// --- Input Hook with Spin and Throw Logic ---
MAKE_HOOK_MATCH(TrickSaberInputUpdateHook, &GlobalNamespace::OculusVRHelper::FixedUpdate, void, GlobalNamespace::OculusVRHelper* self) {
    TrickSaberInputUpdateHook(self);

    // --- Mod Enabled Check ---
    if (!getTrickSaberConfig().ModEnabled.GetValue()) {
        // Rest states, why not..
        if (leftSaberState != SaberInteractionState::Held || leftSpinActive) {
            if (leftSaberTransform && leftSaberOriginalParent) {
                leftSaberTransform->SetParent(leftSaberOriginalParent.ptr(), false);
                leftSaberTransform->set_localPosition(originalLeftSaberLocalPosition);
                leftSaberTransform->set_localRotation(originalLeftSaberLocalRotation);
            }
            leftSaberState = SaberInteractionState::Held; leftSpinActive = false;
        }
        if (rightSaberState != SaberInteractionState::Held || rightSpinActive) {
            if (rightSaberTransform && rightSaberOriginalParent) {
                rightSaberTransform->SetParent(rightSaberOriginalParent.ptr(), false);
                rightSaberTransform->set_localPosition(originalRightSaberLocalPosition);
                rightSaberTransform->set_localRotation(originalRightSaberLocalRotation);
            }
            rightSaberState = SaberInteractionState::Held; rightSpinActive = false;
        }
        return;  // Exit if disabled
    }

    float deltaTime = UnityEngine::Time::get_deltaTime();
    if (deltaTime <= 0.00001f) deltaTime = 1.0f / 90.0f;

    // --- Calculate Controller Linear Velocities ---
    if (leftHandTransform) {
        leftControllerVelocity = UnityEngine::Vector3::op_Division(
            UnityEngine::Vector3::op_Subtraction(leftHandTransform->get_position(), prevLeftHandPos), deltaTime);
        prevLeftHandPos = leftHandTransform->get_position();
    }
    if (rightHandTransform) {
        rightControllerVelocity = UnityEngine::Vector3::op_Division(
            UnityEngine::Vector3::op_Subtraction(rightHandTransform->get_position(), prevRightHandPos), deltaTime);
        prevRightHandPos = rightHandTransform->get_position();
    }

    // --- START [Left Saber Logic] ---
    if (leftSaberTransform && leftHandTransform && leftSaberOriginalParent) {
        // --- Handle Throw Button Input ---
        int leftThrowBtnIdx = getTrickSaberConfig().LeftSaberThrowButton.GetValue();
        bool leftThrowInputPressed = false;
        if (leftThrowBtnIdx > 0) {
            leftThrowInputPressed = GlobalNamespace::OVRInput::Get(GetOVRButtonForConfig(
                leftThrowBtnIdx, true), GlobalNamespace::OVRInput::Controller::LTouch);
        }

        if (leftThrowInputPressed && leftSaberState == SaberInteractionState::Held && !leftThrowButtonPressedLastFrame) {
            // --- Initiate Throw ---
            leftSaberState = SaberInteractionState::Thrown;
            getLogger().info("[TS] [FixedUpdate] [Left Saber] Throw Initiated");
            
            leftSaberTransform->SetParent(nullptr, true);

            float throwMultiplier = getTrickSaberConfig().LeftSaberThrowVelocityMultiplier.GetValue();
            leftSaberSimulatedWorldVelocity = UnityEngine::Vector3::op_Multiply(leftControllerVelocity, throwMultiplier); 
            if (leftSpinActive) {
                float speed = getTrickSaberConfig().LeftSaberSpinSpeed.GetValue();
                bool clockwise = getTrickSaberConfig().LeftSaberSpinClockwise.GetValue();
                float direction = clockwise ? 1.0f : -1.0f;
                UnityEngine::Vector3 localSpinAxis = UnityEngine::Vector3::get_right();
                leftSaberSimulatedWorldAngularVelocity = UnityEngine::Vector3::op_Multiply(
                    leftSaberTransform->TransformDirection(localSpinAxis),
                    direction * speed * DEG2RAD_CONSTANT
                );
            } else {
                UnityEngine::Vector3 throwVelocityWorld = leftSaberSimulatedWorldVelocity;
                float throwSpeedMagnitude = throwVelocityWorld.get_magnitude();
                
                if (throwSpeedMagnitude < 1.0f) { // Gentle throw -> Minimal 
                    UnityEngine::Vector3 worldRollAxis = leftSaberTransform->TransformDirection(
                        UnityEngine::Vector3::get_forward());
                    leftSaberSimulatedWorldAngularVelocity = UnityEngine::Vector3::op_Multiply(
                        worldRollAxis, throwSpeedMagnitude * MIN_NATURAL_ROTATION_RAD_PER_SEC);
                    getLogger().info("[TS] [FixedUpdate] [Left Saber] Gentle throw");
                } else {
                    UnityEngine::Vector3 throwDirectionWorldNormalized = throwVelocityWorld.get_normalized();
                    UnityEngine::Vector3 saberForwardWorld = leftSaberTransform->TransformDirection(
                        UnityEngine::Vector3::get_forward());

                    // Cross product to get a perpendicular spin axis
                    UnityEngine::Vector3 naturalSpinAxisWorld = UnityEngine::Vector3::Cross(saberForwardWorld,
                     throwDirectionWorldNormalized);

                    if (naturalSpinAxisWorld.get_sqrMagnitude() < 0.1f) {
                        naturalSpinAxisWorld = saberForwardWorld;
                        getLogger().info("[TS] [FixedUpdate] [Left Saber] Straight throws");
                    } else {
                        naturalSpinAxisWorld = naturalSpinAxisWorld.get_normalized();
                        getLogger().info("[TS] [FixedUpdate] [Left Saber] Normal throw");
                    }
                    
                    float spinSpeedFromVelocity = throwSpeedMagnitude * THROW_VELOCITY_TO_ROTATION_SCALE;
                    spinSpeedFromVelocity = UnityEngine::Mathf::Min(spinSpeedFromVelocity,
                     MAX_NATURAL_ROTATION_FROM_VELOCITY_RAD_PER_SEC);
                    
                    float totalNaturalSpinRadPerSec = MIN_NATURAL_ROTATION_RAD_PER_SEC + spinSpeedFromVelocity;

                    leftSaberSimulatedWorldAngularVelocity = UnityEngine::Vector3::op_Multiply(naturalSpinAxisWorld,
                     totalNaturalSpinRadPerSec);
                }
            }
            leftSpinActive = false;
        } else if (!leftThrowInputPressed && leftSaberState == SaberInteractionState::Thrown && leftThrowButtonPressedLastFrame) {
            // --- Initialte Recall ---
            leftSaberState = SaberInteractionState::Returning;
            getLogger().info("[TS] [FixedUpdate] [Left Saber] Recall Initiated");
            leftSaberReturnTime = 0.0f;
            leftSaberThrowReleasePosition = leftSaberTransform->get_position();
            leftSaberThrowReleaseRotation = leftSaberTransform->get_rotation();
        }
        leftThrowButtonPressedLastFrame = leftThrowInputPressed;


        // --- Handle Player-Controlled Spin ---
        int leftSpinBtnIdx = getTrickSaberConfig().LeftSaberSpinButton.GetValue();
        if (leftSpinBtnIdx > 0 && leftSaberState == SaberInteractionState::Held) {
            bool spinInputPressed = GlobalNamespace::OVRInput::Get(GetOVRButtonForConfig(
                leftSpinBtnIdx, true), GlobalNamespace::OVRInput::Controller::LTouch);
            if (spinInputPressed && !leftSpinActive) {
                leftSpinActive = true; getLogger().info("[TS] [FixedUpdate] [Left Saber] Spin Activated");
            } else if (!spinInputPressed && leftSpinActive) {
                leftSpinActive = false; getLogger().info("[TS] [FixedUpdate] [Left Saber] Spin Deactivated, restoring position");
                if(leftSaberTransform) {
                    leftSaberTransform->set_localRotation(originalLeftSaberLocalRotation);
                    leftSaberTransform->set_localPosition(originalLeftSaberLocalPosition);
                }
            }
        } else if (leftSaberState == SaberInteractionState::Held && leftSpinActive) {  // Edge: Button unassigned from config
            leftSpinActive = false; getLogger().info("[TS] [FixedUpdate] [Left Saber] Spin Deactivated, restoring position");
             if(leftSaberTransform) {
                leftSaberTransform->set_localRotation(originalLeftSaberLocalRotation);
                leftSaberTransform->set_localPosition(originalLeftSaberLocalPosition);
            }
        }


        // --- Apply Saber States ---
        if (leftSaberState == SaberInteractionState::Held) {
            if (leftSaberTransform->get_parent() != leftSaberOriginalParent) {
                leftSaberTransform->SetParent(leftSaberOriginalParent.ptr(), false);
                leftSaberTransform->set_localPosition(originalLeftSaberLocalPosition);
                leftSaberTransform->set_localRotation(originalLeftSaberLocalRotation);
                getLogger().info("[TS] [FixedUpdate] [Left Saber] Re-parented and reset in Held state");
            }
            // Apply player-controlled spin if active
            if (leftSpinActive) {
                float speed = getTrickSaberConfig().LeftSaberSpinSpeed.GetValue();
                bool clockwise = getTrickSaberConfig().LeftSaberSpinClockwise.GetValue();
                float direction = clockwise ? 1.0f : -1.0f;
                float zOffset = getTrickSaberConfig().LeftSaberSpinAnchorZOffset.GetValue();
                UnityEngine::Vector3 localSpinAxis = UnityEngine::Vector3::get_right();
                UnityEngine::Vector3 localPivotOffset = UnityEngine::Vector3::op_Multiply(UnityEngine::Vector3::get_forward(), zOffset);
                UnityEngine::Vector3 worldPivotPoint = leftSaberTransform->TransformPoint(localPivotOffset);
                UnityEngine::Vector3 worldSpinAxis = leftSaberTransform->TransformDirection(localSpinAxis);
                float rotationAngleThisFrame = direction * speed * deltaTime;
                leftSaberTransform->RotateAround(worldPivotPoint, worldSpinAxis, rotationAngleThisFrame);
            } else {
                // If not actively spinning by player
                leftSaberTransform->set_localPosition(originalLeftSaberLocalPosition);
                leftSaberTransform->set_localRotation(originalLeftSaberLocalRotation);
            }
        } else if (leftSaberState == SaberInteractionState::Thrown) {
            // --- Simulated Throw ---
            UnityEngine::Vector3 currentPos = leftSaberTransform->get_position();
            UnityEngine::Vector3 newPos = UnityEngine::Vector3::op_Addition(
                currentPos, UnityEngine::Vector3::op_Multiply(leftSaberSimulatedWorldVelocity, deltaTime));
            leftSaberTransform->set_position(newPos);

            if (leftSaberSimulatedWorldAngularVelocity.get_sqrMagnitude() > 0.0001f) {
                float angle = leftSaberSimulatedWorldAngularVelocity.get_magnitude() * deltaTime * RAD2DEG_CONSTANT;
                UnityEngine::Vector3 axis = leftSaberSimulatedWorldAngularVelocity.get_normalized();
                UnityEngine::Quaternion deltaRotation = UnityEngine::Quaternion::AngleAxis(angle, axis);
                leftSaberTransform->set_rotation(UnityEngine::Quaternion::op_Multiply(
                    deltaRotation, leftSaberTransform->get_rotation()));
            }
        } else if (leftSaberState == SaberInteractionState::Returning) {
            leftSaberReturnTime += deltaTime;
            float leftSaberReturnDuration = getTrickSaberConfig().LeftSaberReturnDuration.GetValue();
            if (leftSaberReturnDuration < 0.01f) leftSaberReturnDuration = 0.01f;

            float t = UnityEngine::Mathf::Clamp01(leftSaberReturnTime / leftSaberReturnDuration);
            UnityEngine::Vector3 targetPos_world = leftHandTransform->TransformPoint(originalLeftSaberLocalPosition);
            UnityEngine::Quaternion targetRot_world = UnityEngine::Quaternion::op_Multiply(
                leftHandTransform->get_rotation(), originalLeftSaberLocalRotation);
            leftSaberTransform->set_position(UnityEngine::Vector3::Lerp(leftSaberThrowReleasePosition, targetPos_world, t));
            
            if (leftSaberSimulatedWorldAngularVelocity.get_sqrMagnitude() > 0.0001f && t < 0.95f) {
                 float angle = leftSaberSimulatedWorldAngularVelocity.get_magnitude() * deltaTime * RAD2DEG_CONSTANT;
                 UnityEngine::Vector3 axis = leftSaberSimulatedWorldAngularVelocity.get_normalized();
                 UnityEngine::Quaternion deltaRotation = UnityEngine::Quaternion::AngleAxis(angle, axis);
                 leftSaberTransform->set_rotation(UnityEngine::Quaternion::op_Multiply(
                    deltaRotation, leftSaberTransform->get_rotation()));
                 leftSaberTransform->set_rotation(UnityEngine::Quaternion::Slerp(
                    leftSaberTransform->get_rotation(), targetRot_world, t*t*t));
            } else {
                leftSaberTransform->set_rotation(UnityEngine::Quaternion::Slerp(
                    leftSaberThrowReleaseRotation, targetRot_world, t));
            }

            if (t >= 1.0f) {
                leftSaberState = SaberInteractionState::Held;
                getLogger().info("[TS] [FixedUpdate] [Left Saber] Returned to hand");
                // The Held block will handle final parenting and position setting next frame
            }
        }
    } else if (leftSaberState != SaberInteractionState::Held && !leftSaberTransform) {
        leftSaberState = SaberInteractionState::Held; leftSpinActive = false;
        getLogger().error("[TS] [FixedUpdate] [Left Saber] Became invalid while not Held. Resetting state");
    }
    // --- END [Left Saber Logic] ---


    // --- START [Right Saber Logic] ---
    if (rightSaberTransform && rightHandTransform && rightSaberOriginalParent) {
        // --- Handle Throw Button Input ---
        int rightThrowBtnIdx = getTrickSaberConfig().RightSaberThrowButton.GetValue();
        bool rightThrowInputPressed = false;
        if (rightThrowBtnIdx > 0) {
            rightThrowInputPressed = GlobalNamespace::OVRInput::Get(GetOVRButtonForConfig(
                rightThrowBtnIdx, false), GlobalNamespace::OVRInput::Controller::RTouch);
        }

        if (rightThrowInputPressed && rightSaberState == SaberInteractionState::Held && !rightThrowButtonPressedLastFrame) {
            // --- Initiate Throw ---
            rightSaberState = SaberInteractionState::Thrown;
            getLogger().info("[TS] [FixedUpdate] [Right Saber] Throw Initiated");
            
            rightSaberTransform->SetParent(nullptr, true);

            float throwMultiplier = getTrickSaberConfig().RightSaberThrowVelocityMultiplier.GetValue();
            rightSaberSimulatedWorldVelocity = UnityEngine::Vector3::op_Multiply(rightControllerVelocity, throwMultiplier);
            if (rightSpinActive) { // If it was being actively spun by your mod
                float speed = getTrickSaberConfig().RightSaberSpinSpeed.GetValue();
                bool clockwise = getTrickSaberConfig().RightSaberSpinClockwise.GetValue();
                float direction = clockwise ? 1.0f : -1.0f;
                UnityEngine::Vector3 localSpinAxis = UnityEngine::Vector3::get_right();
                rightSaberSimulatedWorldAngularVelocity = UnityEngine::Vector3::op_Multiply(
                    rightSaberTransform->TransformDirection(localSpinAxis),
                    direction * speed * DEG2RAD_CONSTANT
                );
            } else {
                UnityEngine::Vector3 throwVelocityWorld = rightSaberSimulatedWorldVelocity;
                float throwSpeedMagnitude = throwVelocityWorld.get_magnitude();
                
                if (throwSpeedMagnitude < 1.0f) {
                    UnityEngine::Vector3 worldRollAxis = rightSaberTransform->TransformDirection(UnityEngine::Vector3::get_forward());
                    rightSaberSimulatedWorldAngularVelocity = UnityEngine::Vector3::op_Multiply(
                        worldRollAxis, throwSpeedMagnitude * MIN_NATURAL_ROTATION_RAD_PER_SEC);
                    getLogger().info("[TS] [FixedUpdate] [Right Saber] Gentle throw");
                } else {
                    UnityEngine::Vector3 throwDirectionWorldNormalized = throwVelocityWorld.get_normalized();
                    UnityEngine::Vector3 saberForwardWorld = rightSaberTransform->TransformDirection(
                        UnityEngine::Vector3::get_forward());

                    UnityEngine::Vector3 naturalSpinAxisWorld = UnityEngine::Vector3::Cross(
                        saberForwardWorld, throwDirectionWorldNormalized);

                    if (naturalSpinAxisWorld.get_sqrMagnitude() < 0.1f) {
                        naturalSpinAxisWorld = saberForwardWorld;
                        getLogger().info("[TS] [FixedUpdate] [Right Saber] Straight throw");
                    } else {
                        naturalSpinAxisWorld = naturalSpinAxisWorld.get_normalized();
                        getLogger().info("[TS] [FixedUpdate] [Right Saber] Normal throw");
                    }
                    
                    float spinSpeedFromVelocity = throwSpeedMagnitude * THROW_VELOCITY_TO_ROTATION_SCALE;
                    spinSpeedFromVelocity = UnityEngine::Mathf::Min(spinSpeedFromVelocity, MAX_NATURAL_ROTATION_FROM_VELOCITY_RAD_PER_SEC);
                    float totalNaturalSpinRadPerSec = MIN_NATURAL_ROTATION_RAD_PER_SEC + spinSpeedFromVelocity;

                    rightSaberSimulatedWorldAngularVelocity = UnityEngine::Vector3::op_Multiply(
                        naturalSpinAxisWorld, totalNaturalSpinRadPerSec);
                }
            }
            rightSpinActive = false;
        } else if (!rightThrowInputPressed && rightSaberState == SaberInteractionState::Thrown && rightThrowButtonPressedLastFrame) {
            // --- Initiate Recall ---
            rightSaberState = SaberInteractionState::Returning;
            getLogger().info("[TS] [FixedUpdate] [Right Saber] Recall Initiated");
            rightSaberReturnTime = 0.0f;
            rightSaberThrowReleasePosition = rightSaberTransform->get_position();
            rightSaberThrowReleaseRotation = rightSaberTransform->get_rotation();
        }
        rightThrowButtonPressedLastFrame = rightThrowInputPressed;

        // --- Handle Player-Controlled Spin ---
        int rightSpinBtnIdx = getTrickSaberConfig().RightSaberSpinButton.GetValue();
        if (rightSpinBtnIdx > 0 && rightSaberState == SaberInteractionState::Held) {
            bool spinInputPressed = GlobalNamespace::OVRInput::Get(GetOVRButtonForConfig(
                rightSpinBtnIdx, false), GlobalNamespace::OVRInput::Controller::RTouch);
            if (spinInputPressed && !rightSpinActive) {
                rightSpinActive = true;
                getLogger().info("[TS] [FixedUpdate] [Right Saber] Spin Activated");
            } else if (!spinInputPressed && rightSpinActive) {
                rightSpinActive = false;
                getLogger().info("[TS] [FixedUpdate] [Right Saber] Spin Deactivated, restoring position");
                if(rightSaberTransform) {
                    rightSaberTransform->set_localRotation(originalRightSaberLocalRotation);
                    rightSaberTransform->set_localPosition(originalRightSaberLocalPosition);
                }
            }
        } else if (rightSaberState == SaberInteractionState::Held && rightSpinActive) {
            rightSpinActive = false;
            getLogger().info("[TS] [FixedUpdate] [Right Saber] Spin Deactivated, restoring position");
            if(rightSaberTransform) {
                rightSaberTransform->set_localRotation(originalRightSaberLocalRotation);
                rightSaberTransform->set_localPosition(originalRightSaberLocalPosition);
            }
        }
        
        // --- Apply Saber States ---
        if (rightSaberState == SaberInteractionState::Held) {
            if (rightSaberTransform->get_parent() != rightSaberOriginalParent) {
                rightSaberTransform->SetParent(rightSaberOriginalParent.ptr(), false);
                rightSaberTransform->set_localPosition(originalRightSaberLocalPosition);
                rightSaberTransform->set_localRotation(originalRightSaberLocalRotation);
                getLogger().info("[TS] [FixedUpdate] [Right Saber] Re-parented and reset in Held state");
            }
            if (rightSpinActive) {
                float speed = getTrickSaberConfig().RightSaberSpinSpeed.GetValue();
                bool clockwise = getTrickSaberConfig().RightSaberSpinClockwise.GetValue();
                float direction = clockwise ? 1.0f : -1.0f;
                float zOffset = getTrickSaberConfig().RightSaberSpinAnchorZOffset.GetValue();
                UnityEngine::Vector3 localSpinAxis = UnityEngine::Vector3::get_right();
                UnityEngine::Vector3 localPivotOffset = UnityEngine::Vector3::op_Multiply(UnityEngine::Vector3::get_forward(), zOffset);
                UnityEngine::Vector3 worldPivotPoint = rightSaberTransform->TransformPoint(localPivotOffset);
                UnityEngine::Vector3 worldSpinAxis = rightSaberTransform->TransformDirection(localSpinAxis);
                float rotationAngleThisFrame = direction * speed * deltaTime;
                rightSaberTransform->RotateAround(worldPivotPoint, worldSpinAxis, rotationAngleThisFrame);
            } else {
                rightSaberTransform->set_localPosition(originalRightSaberLocalPosition);
                rightSaberTransform->set_localRotation(originalRightSaberLocalRotation);
            }
        } else if (rightSaberState == SaberInteractionState::Thrown) {
            // --- Simulated ---
            UnityEngine::Vector3 currentPos = rightSaberTransform->get_position();
            UnityEngine::Vector3 newPos = UnityEngine::Vector3::op_Addition(currentPos, UnityEngine::Vector3::op_Multiply(
                rightSaberSimulatedWorldVelocity, deltaTime));
            rightSaberTransform->set_position(newPos);

            if (rightSaberSimulatedWorldAngularVelocity.get_sqrMagnitude() > 0.0001f) {
                float angle = rightSaberSimulatedWorldAngularVelocity.get_magnitude() * deltaTime * RAD2DEG_CONSTANT;
                UnityEngine::Vector3 axis = rightSaberSimulatedWorldAngularVelocity.get_normalized();
                UnityEngine::Quaternion deltaRotation = UnityEngine::Quaternion::AngleAxis(angle, axis);
                rightSaberTransform->set_rotation(UnityEngine::Quaternion::op_Multiply(
                    deltaRotation, rightSaberTransform->get_rotation()));
            }
        } else if (rightSaberState == SaberInteractionState::Returning) {
            rightSaberReturnTime += deltaTime;
            float rightSaberReturnDuration = getTrickSaberConfig().RightSaberReturnDuration.GetValue();
            if (rightSaberReturnDuration < 0.01f) rightSaberReturnDuration = 0.01f;
            float t = UnityEngine::Mathf::Clamp01(rightSaberReturnTime / rightSaberReturnDuration);
            UnityEngine::Vector3 targetPos_world = rightHandTransform->TransformPoint(originalRightSaberLocalPosition);
            UnityEngine::Quaternion targetRot_world = UnityEngine::Quaternion::op_Multiply(
                rightHandTransform->get_rotation(), originalRightSaberLocalRotation);
            rightSaberTransform->set_position(UnityEngine::Vector3::Lerp(rightSaberThrowReleasePosition, targetPos_world, t));
            
            if (rightSaberSimulatedWorldAngularVelocity.get_sqrMagnitude() > 0.0001f && t < 0.95f) {
                 float angle = rightSaberSimulatedWorldAngularVelocity.get_magnitude() * deltaTime * RAD2DEG_CONSTANT;
                 UnityEngine::Vector3 axis = rightSaberSimulatedWorldAngularVelocity.get_normalized();
                 UnityEngine::Quaternion deltaRotation = UnityEngine::Quaternion::AngleAxis(angle, axis);
                 rightSaberTransform->set_rotation(UnityEngine::Quaternion::op_Multiply(
                    deltaRotation, rightSaberTransform->get_rotation()));
                 rightSaberTransform->set_rotation(UnityEngine::Quaternion::Slerp(
                    rightSaberTransform->get_rotation(), targetRot_world, t*t*t));
            } else {
                rightSaberTransform->set_rotation(UnityEngine::Quaternion::Slerp(
                    rightSaberThrowReleaseRotation, targetRot_world, t));
            }

            if (t >= 1.0f) {
                rightSaberState = SaberInteractionState::Held;
                getLogger().info("[TS] [FixedUpdate] [Right Saber] Returned to hand");
            }
        }
    } else if (rightSaberState != SaberInteractionState::Held && !rightSaberTransform) {
        rightSaberState = SaberInteractionState::Held; rightSpinActive = false;
        getLogger().error("[TS] [FixedUpdate] [Right Saber] Became invalid while not Held. Resetting state");
    }
}


extern "C" __attribute__((visibility("default"))) void setup(CModInfo& info) {
    info.id = MOD_ID;
    info.version = VERSION;
}

extern "C" __attribute__((visibility("default"))) void late_load() {
    il2cpp_functions::Init();

    getTrickSaberConfig().Init(modInfo);


    getLogger().info("Deactivated Score Submission safely !!");
    TrickUtils::Utils::DisableScoreSubmission();

    bool registrationSuccess = BSML::Register::RegisterSettingsMenu("TrickSaber Lite Settings",
     TrickSaber::UI::SettingsViewControllerDidActivate, false);

    auto logger = Paper::ConstLoggerContext("TrickSaberLite");
    getLogger().info("Installing Hooks..");
    INSTALL_HOOK(logger, MainMenuViewController_DidActivate_Hook);
    INSTALL_HOOK(logger, SaberModelController_Init_Hook);
    INSTALL_HOOK(logger, TrickSaberInputUpdateHook);  
    getLogger().info("Hooks installed!!");
}