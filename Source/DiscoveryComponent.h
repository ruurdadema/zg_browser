#pragma once

#include <functional>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "MuscleJuce.h"

#include "util/Hashtable.h"
#include "zg/discovery/client/IDiscoveryNotificationTarget.h"
#include "zg/discovery/client/SystemDiscoveryClient.h"

/** Startup screen: the list of ZG systems currently visible on the LAN. */
class DiscoveryComponent final : public juce::Component,
                                 public juce::ListBoxModel,
                                 private zg::IDiscoveryNotificationTarget
{
public:
   explicit DiscoveryComponent(zg::SystemDiscoveryClient & discoveryClient);
   ~DiscoveryComponent() override;

   /** Called with (signaturePattern, systemName) when the user picks a system to browse. */
   std::function<void(const muscle::String &, const muscle::String &)> onSystemChosen;

   void resized() override;
   void paint(juce::Graphics & g) override;

   // ListBoxModel
   int getNumRows() override;
   void paintListBoxItem(int rowNumber, juce::Graphics & g, int width, int height, bool rowIsSelected) override;
   void listBoxItemClicked(int rowNumber, const juce::MouseEvent & e) override;
   void returnKeyPressed(int lastRowSelected) override;

private:
   // IDiscoveryNotificationTarget
   void DiscoveryUpdate(const muscle::String & systemName, const muscle::MessageRef & optSystemInfo) override;

   void rebuildRows();
   void chooseRow(int rowNumber);
   void connectToTypedSystemName();

   struct SystemRow
   {
      muscle::String _systemName;
      muscle::String _signature;
      juce::String _detail;
   };

   muscle::Hashtable<muscle::String, muscle::MessageRef> _systems;
   std::vector<SystemRow> _rows;

   juce::Label _titleLabel;
   juce::Label _hintLabel;
   juce::ListBox _listBox {"systems", this};
   juce::Label _manualLabel;
   juce::TextEditor _manualName;
   juce::TextButton _manualConnect {"Connect"};

   JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DiscoveryComponent)
};
