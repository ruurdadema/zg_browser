#pragma once

#include <functional>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "MuscleJuce.h"

#include "util/Hashtable.h"
#include "zg/ZGPeerID.h"
#include "zg/discovery/client/IDiscoveryNotificationTarget.h"
#include "zg/discovery/client/SystemDiscoveryClient.h"

/** Startup screen: the ZG systems currently visible on the LAN, each with its
  * peers listed (indented) beneath it.
  */
class DiscoveryComponent final : public juce::Component,
                                 public juce::ListBoxModel,
                                 private zg::IDiscoveryNotificationTarget
{
public:
   explicit DiscoveryComponent(zg::SystemDiscoveryClient & discoveryClient);
   ~DiscoveryComponent() override;

   /** Called with (signaturePattern, systemName) when the user picks a system to browse. */
   std::function<void(const muscle::String &, const muscle::String &)> onSystemChosen;

   /** Called with (signature, systemName, peerID, peerAddress) when the user picks one peer to browse. */
   std::function<void(const muscle::String &, const muscle::String &, const zg::ZGPeerID &, const juce::String &)> onPeerChosen;

   void resized() override;
   void paint(juce::Graphics & g) override;

   // ListBoxModel
   int getNumRows() override;
   void paintListBoxItem(int rowNumber, juce::Graphics & g, int width, int height, bool rowIsSelected) override;
   void listBoxItemDoubleClicked(int rowNumber, const juce::MouseEvent & e) override;
   void returnKeyPressed(int lastRowSelected) override;

private:
   // IDiscoveryNotificationTarget
   void DiscoveryUpdate(const muscle::String & systemName, const muscle::MessageRef & optSystemInfo) override;

   void rebuildRows();
   void chooseRow(int rowNumber);
   void connectToTypedSystemName();

   /** One line of the list:  a system, or (indented beneath it) one of its peers. */
   struct Row
   {
      bool _isPeer = false;
      muscle::String _systemName;
      muscle::String _signature;
      juce::String _heading;
      juce::String _detail;

      // Peer rows only
      zg::ZGPeerID _peerID;
      juce::String _peerAddress;   // "ip:port" of its MUSCLE server, or "" if it didn't advertise one

      /** Identifies the row across rebuilds, so the selection can follow it. */
      juce::String getKey() const {return zgb::toJuce(_systemName) + (_isPeer ? ("\n" + zgb::toJuce(_peerID.ToString())) : juce::String());}
   };

   muscle::Hashtable<muscle::String, muscle::MessageRef> _systems;
   std::vector<Row> _rows;

   juce::Label _titleLabel;
   juce::Label _hintLabel;
   juce::ListBox _listBox {"systems", this};
   juce::Label _manualLabel;
   juce::TextEditor _manualName;
   juce::TextButton _manualConnect {"Connect"};

   JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DiscoveryComponent)
};
