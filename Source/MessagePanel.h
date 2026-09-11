#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "MuscleJuce.h"
#include "message/Message.h"

/** Right-hand pane: shows the Message payload of the selected data node. */
class MessagePanel final : public juce::Component
{
public:
   MessagePanel();

   /** @param nodePath session-relative path of the selected node
     * @param optPayload the node's payload, or a NULL reference if we have no data for it
     */
   void showNode(const muscle::String & nodePath, const muscle::ConstMessageRef & optPayload);

   /** Shows the "nothing selected" state. */
   void clear();

   void resized() override;
   void paint(juce::Graphics & g) override;

private:
   juce::Label _pathLabel;
   juce::TextEditor _contents;

   JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MessagePanel)
};
