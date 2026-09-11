#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Flat, dark styling for the stock JUCE controls (buttons, text fields), so
  * they sit in the same visual language as the custom-drawn tree and lists.
  */
class ZGLookAndFeel final : public juce::LookAndFeel_V4
{
public:
   ZGLookAndFeel();

   juce::Font getTextButtonFont(juce::TextButton & button, int buttonHeight) override;

   void drawButtonBackground(juce::Graphics & g, juce::Button & button, const juce::Colour & backgroundColour,
                             bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

   void drawButtonText(juce::Graphics & g, juce::TextButton & button,
                       bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

   void fillTextEditorBackground(juce::Graphics & g, int width, int height, juce::TextEditor & editor) override;
   void drawTextEditorOutline(juce::Graphics & g, int width, int height, juce::TextEditor & editor) override;

private:
   static constexpr float _cornerRadius = 5.0f;

   JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZGLookAndFeel)
};
