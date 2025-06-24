#pragma once

#include "HMUI/ViewController.hpp"
#include "HMUI/Touchable.hpp"

#include <vector>
#include <string_view>

namespace TrickSaber::UI {

    extern std::vector<std::string_view> RightControllerButtonChoices;
    extern std::vector<std::string_view> LeftControllerButtonChoices;

    void SettingsViewControllerDidActivate(
        HMUI::ViewController* self,
        bool firstActivation,
        bool addedToHierarchy,
        bool screenSystemEnabling
    );

}