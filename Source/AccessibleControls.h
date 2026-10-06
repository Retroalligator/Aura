// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Aura contributors
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace aura
{
// Retain JUCE's popup and keyboard behavior, and allow assistive clients to
// select an existing choice by its name rather than inserting arbitrary text.
class ChoiceBox final : public juce::ComboBox
{
    class Handler final : public juce::AccessibilityHandler
    {
        class Value final : public juce::AccessibilityTextValueInterface
        {
        public:
            explicit Value(ChoiceBox& c) : choice(c) {}
            bool isReadOnly() const override { return false; }
            juce::String getCurrentValueAsString() const override { return choice.getText(); }
            void setValueAsString(const juce::String& choiceName) override
            {
                for (int i = 0; i < choice.getNumItems(); ++i)
                    if (choice.getItemText(i).equalsIgnoreCase(choiceName))
                    { choice.setSelectedId(choice.getItemId(i), juce::sendNotificationSync); return; }
            }
        private:
            ChoiceBox& choice;
        };
    public:
        explicit Handler(ChoiceBox& c)
            : AccessibilityHandler(c, juce::AccessibilityRole::comboBox,
                juce::AccessibilityActions().addAction(juce::AccessibilityActionType::press, [&c] { c.showPopup(); })
                    .addAction(juce::AccessibilityActionType::showMenu, [&c] { c.showPopup(); }),
                { std::make_unique<Value>(c) }), choice(c) {}
        juce::AccessibleState getCurrentState() const override
        {
            auto state = AccessibilityHandler::getCurrentState().withExpandable();
            return choice.isPopupActive() ? state.withExpanded() : state.withCollapsed();
        }
        juce::String getTitle() const override { return choice.getTitle(); }
        juce::String getHelp() const override { return choice.getTooltip(); }
    private:
        ChoiceBox& choice;
    };
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    { return std::make_unique<Handler>(*this); }
};
}
