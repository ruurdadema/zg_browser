#include "ZGLookAndFeel.h"

#include "Theme.h"

using namespace zgb;

ZGLookAndFeel :: ZGLookAndFeel()
{
   setColourScheme(getDarkColourScheme());

   setColour(juce::ResizableWindow::backgroundColourId, theme::background);
   setColour(juce::DocumentWindow::textColourId,        theme::text);

   setColour(juce::TextButton::buttonColourId,   theme::control);
   setColour(juce::TextButton::buttonOnColourId, theme::accent);
   setColour(juce::TextButton::textColourOffId,  theme::text);
   setColour(juce::TextButton::textColourOnId,   juce::Colours::white);

   setColour(juce::TextEditor::backgroundColourId,      theme::contentBackground);
   setColour(juce::TextEditor::textColourId,            theme::textBody);
   setColour(juce::TextEditor::outlineColourId,         theme::border);
   setColour(juce::TextEditor::focusedOutlineColourId,  theme::accent.brighter(0.25f));
   setColour(juce::TextEditor::highlightColourId,       theme::accent);
   setColour(juce::TextEditor::highlightedTextColourId, juce::Colours::white);
   setColour(juce::CaretComponent::caretColourId,       theme::accent.brighter(0.6f));

   setColour(juce::Label::textColourId,    theme::text);
   setColour(juce::ListBox::backgroundColourId, theme::contentBackground);
   setColour(juce::TreeView::backgroundColourId, theme::background);
   setColour(juce::ScrollBar::thumbColourId, theme::border.brighter(0.25f));
}

juce::Font ZGLookAndFeel :: getTextButtonFont(juce::TextButton &, int buttonHeight)
{
   return juce::Font(juce::FontOptions(juce::jlimit(12.0f, 15.0f, (float) buttonHeight * 0.48f)));
}

void ZGLookAndFeel :: drawButtonBackground(juce::Graphics & g, juce::Button & button, const juce::Colour & backgroundColour,
                                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
   const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);

   juce::Colour face = backgroundColour;
   if (shouldDrawButtonAsDown)             face = face.darker(0.3f);
   else if (shouldDrawButtonAsHighlighted) face = face.brighter(0.18f);
   if (button.isEnabled() == false)        face = face.withMultipliedAlpha(0.5f);

   g.setColour(face);
   g.fillRoundedRectangle(bounds, _cornerRadius);

   g.setColour(face.brighter(0.35f).withAlpha(0.9f));
   g.drawRoundedRectangle(bounds, _cornerRadius, 1.0f);
}

void ZGLookAndFeel :: drawButtonText(juce::Graphics & g, juce::TextButton & button, bool shouldDrawButtonAsHighlighted, bool)
{
   const auto colourId = button.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId;

   juce::Colour textColour = button.findColour(colourId);
   if (button.isEnabled() == false)          textColour = textColour.withMultipliedAlpha(0.5f);
   else if (shouldDrawButtonAsHighlighted)   textColour = textColour.brighter(0.2f);

   g.setColour(textColour);
   g.setFont(getTextButtonFont(button, button.getHeight()));
   g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(9, 0), juce::Justification::centred, 1);
}

void ZGLookAndFeel :: fillTextEditorBackground(juce::Graphics & g, int width, int height, juce::TextEditor & editor)
{
   g.setColour(editor.findColour(juce::TextEditor::backgroundColourId));
   g.fillRoundedRectangle(juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height), _cornerRadius);
}

void ZGLookAndFeel :: drawTextEditorOutline(juce::Graphics & g, int width, int height, juce::TextEditor & editor)
{
   // A transparent outline colour means "never draw a box" -- that's how the
   // read-only Message dump asks to be left alone.
   const auto resting = editor.findColour(juce::TextEditor::outlineColourId);
   if (resting.isTransparent()) return;

   const bool focused = editor.hasKeyboardFocus(true) && (editor.isReadOnly() == false);

   g.setColour(focused ? editor.findColour(juce::TextEditor::focusedOutlineColourId) : resting);
   g.drawRoundedRectangle(juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(0.5f),
                          _cornerRadius, focused ? 1.5f : 1.0f);
}
