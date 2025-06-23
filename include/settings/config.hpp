#pragma once

#include "config-utils/shared/config-utils.hpp"


DECLARE_CONFIG(TrickSaberConfig) {
    CONFIG_VALUE(ModEnabled, bool, "Enable TriickSaber Mod", true, "Toggles the entire mod on or off.");

    CONFIG_VALUE(LeftSaberSpinButton, int, "Left Saber Spin Button", 0, "Button to activate left saber spin.");
    CONFIG_VALUE(LeftSaberThrowButton, int, "Left Saber Throw Button", 0, "Button to activate left saber throw.");

    CONFIG_VALUE(LeftSaberSpinClockwise, bool, "Left Spin Clockwise", true, "If true, spins clockwise, else counter-clockwise.");
    CONFIG_VALUE(LeftSaberSpinSpeed, float, "Left Spin Speed", 2000.0, "Speed of left saber spin in degrees per second.");
    CONFIG_VALUE(LeftSaberSpinAnchorZOffset, float, "Left Spin Anchor Z-Offset", -0.2f, "Offset along saber's length (Z) for spin pivot. Positive is towards tip.");
    CONFIG_VALUE(LeftSaberThrowVelocityMultiplier, float, "Left Throw Velocity Multiplier", 3.0f, "Multiplier for the initial throw velocity of the left saber.");
    CONFIG_VALUE(LeftSaberReturnDuration, float, "Left Saber Return Duration (sec)", 0.2f, "Time it takes for a thrown saber to return. Shorter is faster.");

    CONFIG_VALUE(RightSaberSpinButton, int, "Right Saber Spin Button", 0, "Button to activate right saber spin.");
    CONFIG_VALUE(RightSaberThrowButton, int, "Right Saber Throw Button", 0, "Button to activate right saber throw.");

    CONFIG_VALUE(RightSaberSpinClockwise, bool, "Right Spin Clockwise", true, "If true, spins clockwise, else counter-clockwise.");
    CONFIG_VALUE(RightSaberSpinSpeed, float, "Right Spin Speed", 2000.0, "Speed of right saber spin in degrees per second.")
    CONFIG_VALUE(RightSaberSpinAnchorZOffset, float, "Right Spin Anchor Z-Offset", -0.2f, "Offset along saber's length (Z) for spin pivot. Positive is towards tip.")
    CONFIG_VALUE(RightSaberThrowVelocityMultiplier, float, "Right Throw Velocity Multiplier", 3.0f, "Multiplier for the initial throw velocity of the right saber."); 
    CONFIG_VALUE(RightSaberReturnDuration, float, "Right Saber Return Duration (sec)", 0.2f, "Time it takes for a thrown saber to return. Shorter is faster.");

};
