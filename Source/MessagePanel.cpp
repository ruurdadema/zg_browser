#include "MessagePanel.h"

#include "Theme.h"

#include "zlib/ZLibUtilityFunctions.h"   // for IsMessageDeflated()/InflateMessage()

using namespace muscle;

MessagePanel :: MessagePanel()
{
   _pathLabel.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
   addAndMakeVisible(_pathLabel);

   _contents.setMultiLine(true, false);
   _contents.setReadOnly(true);
   _contents.setScrollbarsShown(true);
   _contents.setCaretVisible(false);
   _contents.setPopupMenuEnabled(true);
   _contents.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 12.5f, juce::Font::plain)));
   _contents.setColour(juce::TextEditor::backgroundColourId, zgb::theme::contentBackground);
   _contents.setColour(juce::TextEditor::outlineColourId,    juce::Colours::transparentBlack);  // no box around the dump
   addAndMakeVisible(_contents);

   clear();
}

void MessagePanel :: clear()
{
   _pathLabel.setText("No node selected", juce::dontSendNotification);
   _contents.setText("Select a node in the tree to see its Message.", juce::dontSendNotification);
}

void MessagePanel :: showNode(const String & nodePath, const ConstMessageRef & optPayload)
{
   _pathLabel.setText(zgb::toJuce(nodePath.WithPrepend("/")), juce::dontSendNotification);

   juce::String text;
   if (optPayload())
   {
      text << zgb::toJuce(optPayload()->ToString());

      if (IsMessageDeflated(optPayload))
      {
         const ConstMessageRef inflated = InflateMessage(optPayload);
         text << juce::newLine << juce::newLine << "--- inflates to: ---" << juce::newLine << juce::newLine;
         if (inflated()) text << zgb::toJuce(inflated()->ToString());
                    else text << "[inflate error: " << juce::String(inflated.GetStatus()()) << "]";
      }
   }
   else text = "This node's payload isn't known (it may have just been deleted).";

   _contents.setText(text, juce::dontSendNotification);
   _contents.moveCaretToTop(false);
}

void MessagePanel :: paint(juce::Graphics & g)
{
   g.fillAll(zgb::theme::contentBackground);
}

void MessagePanel :: resized()
{
   auto r = getLocalBounds().reduced(8, 6);
   _pathLabel.setBounds(r.removeFromTop(22));
   r.removeFromTop(4);
   _contents.setBounds(r);
}
