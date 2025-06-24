#include "settings/controller.hpp"
#include "settings/config.hpp"
#include "logger.hpp"

std::vector<std::string_view> TrickSaber::UI::RightControllerButtonChoices = {
    "None",
    "A",
    "B",
    "Right Trigger",
    "Right Grip"
};

std::vector<std::string_view> TrickSaber::UI::LeftControllerButtonChoices = {
    "None",
    "X",
    "Y",
    "Left Trigger",
    "Left Grip"
};


namespace TrickSaber::UI {
    void SettingsViewControllerDidActivate(
        HMUI::ViewController* self,
        bool firstActivation,
        bool addedToHierarchy,
        bool screenSystemEnabling
    ) {
        if (!firstActivation) {
            return;
        }

        if (!self->get_gameObject()->GetComponent<HMUI::Touchable*>()) {
            self->get_gameObject()->AddComponent<HMUI::Touchable*>();
        }

        UnityEngine::GameObject* container = BSML::Lite::CreateScrollableSettingsContainer(self->get_transform());
        UnityEngine::Transform* parent = container->get_transform();

        BSML::Lite::CreateToggle(parent, "Enable TriickSaber",
         getTrickSaberConfig().ModEnabled.GetValue(), [](bool value){
            getTrickSaberConfig().ModEnabled.SetValue(value);
        });


        // Right Saber Settings:
        BSML::Lite::CreateText(parent, "--- Right Saber Controls ---");

        BSML::Lite::CreateDropdown(parent, "Spin Button",
            RightControllerButtonChoices[getTrickSaberConfig().RightSaberSpinButton.GetValue()],
            RightControllerButtonChoices,
            [](StringW value) {
                int selectedIndex = std::find(RightControllerButtonChoices.begin(),
                 RightControllerButtonChoices.end(), value) - RightControllerButtonChoices.begin();
                getTrickSaberConfig().RightSaberSpinButton.SetValue(selectedIndex);
            }
        );

        BSML::Lite::CreateToggle(parent, "Spin Clockwise",
         getTrickSaberConfig().RightSaberSpinClockwise.GetValue(), [](bool value){
            getTrickSaberConfig().RightSaberSpinClockwise.SetValue(value);
        });

        BSML::Lite::CreateIncrementSetting(parent, "Spin Speed", 1, 50.0f,
            getTrickSaberConfig().RightSaberSpinSpeed.GetValue(),
            100.0f, 10000.0f,
            [](float value){
                getTrickSaberConfig().RightSaberSpinSpeed.SetValue(value);
        });

        BSML::Lite::CreateIncrementSetting(parent, "Spin Anchor Z-Offset", 2, 0.05f,
            getTrickSaberConfig().RightSaberSpinAnchorZOffset.GetValue(),
            -1.0f, 1.0f,
            [](float value){
                getTrickSaberConfig().RightSaberSpinAnchorZOffset.SetValue(value);
        });

        BSML::Lite::CreateIncrementSetting(parent, "Throw Velocity Multiplier", 1, 0.1f,
            getTrickSaberConfig().RightSaberThrowVelocityMultiplier.GetValue(),
            0.5f, 5.0f,
            [](float value){
                getTrickSaberConfig().RightSaberThrowVelocityMultiplier.SetValue(value);
        });

        BSML::Lite::CreateIncrementSetting(parent, "Saber Return Duration", 2, 0.02f,
            getTrickSaberConfig().RightSaberReturnDuration.GetValue(),
            0.02f, 1.5f,
            [](float value){
                if (value < 0.01f) value = 0.02f;
                getTrickSaberConfig().RightSaberReturnDuration.SetValue(value);
        });

        BSML::Lite::CreateDropdown(parent, "Throw Button",
            RightControllerButtonChoices[getTrickSaberConfig().RightSaberThrowButton.GetValue()],
            RightControllerButtonChoices,
            [](StringW value) {
                int selectedIndex = std::find(RightControllerButtonChoices.begin(),
                 RightControllerButtonChoices.end(), value) - RightControllerButtonChoices.begin();
                getTrickSaberConfig().RightSaberThrowButton.SetValue(selectedIndex);
            }
        );


        // Left Saber Settings:
        BSML::Lite::CreateText(parent, "--- Left Saber Controls ---");

        BSML::Lite::CreateDropdown(parent, "Spin Button",
            LeftControllerButtonChoices[getTrickSaberConfig().LeftSaberSpinButton.GetValue()],
            LeftControllerButtonChoices,
            [](StringW value) {
                int selectedIndex = std::find(LeftControllerButtonChoices.begin(),
                 LeftControllerButtonChoices.end(), value) - LeftControllerButtonChoices.begin();
                getTrickSaberConfig().LeftSaberSpinButton.SetValue(selectedIndex);
            }
        );

        BSML::Lite::CreateToggle(parent, "Spin Clockwise",
         getTrickSaberConfig().LeftSaberSpinClockwise.GetValue(), [](bool value){
            getTrickSaberConfig().LeftSaberSpinClockwise.SetValue(value);
        });
        
        BSML::Lite::CreateIncrementSetting(parent, "Spin Speed", 1, 50.0f,
            getTrickSaberConfig().LeftSaberSpinSpeed.GetValue(),
            100.0f, 10000.0f,
            [](float value){
                getTrickSaberConfig().LeftSaberSpinSpeed.SetValue(value);
        });

        BSML::Lite::CreateIncrementSetting(parent, "Spin Anchor Z-Offset", 2, 0.05f,
            getTrickSaberConfig().LeftSaberSpinAnchorZOffset.GetValue(),
            -1.0f, 1.0f,
            [](float value){
                getTrickSaberConfig().LeftSaberSpinAnchorZOffset.SetValue(value);
        });

        BSML::Lite::CreateIncrementSetting(parent, "Throw Velocity Mult.", 1, 0.1f,
            getTrickSaberConfig().LeftSaberThrowVelocityMultiplier.GetValue(),
            0.5f, 5.0f,
            [](float value){
                getTrickSaberConfig().LeftSaberThrowVelocityMultiplier.SetValue(value);
        });

        BSML::Lite::CreateDropdown(parent, "Throw Button",
            LeftControllerButtonChoices[getTrickSaberConfig().LeftSaberThrowButton.GetValue()],
            LeftControllerButtonChoices,
            [](StringW value) {
                int selectedIndex = std::find(LeftControllerButtonChoices.begin(),
                 LeftControllerButtonChoices.end(), value) - LeftControllerButtonChoices.begin();
                getTrickSaberConfig().LeftSaberThrowButton.SetValue(selectedIndex);
            }
        );

        BSML::Lite::CreateIncrementSetting(parent, "Saber Return Duration", 2, 0.02f,
            getTrickSaberConfig().LeftSaberReturnDuration.GetValue(),
            0.02f, 1.5f,
            [](float value){
                if (value < 0.01f) value = 0.02f;
                getTrickSaberConfig().LeftSaberReturnDuration.SetValue(value);
        });

        getLogger().info("[TS] [Settings] UI Created");
    }
}