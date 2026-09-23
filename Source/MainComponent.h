#pragma once

#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "BrowserComponent.h"
#include "DiscoveryComponent.h"
#include "MuscleJuce.h"

#include "platform/juce/JUCECallbackMechanism.h"
#include "zg/discovery/client/SystemDiscoveryClient.h"

/** Top-level view: either the discovery list or the browser for one system (or peer). */
class MainComponent final : public juce::Component
{
public:
   MainComponent();
   ~MainComponent() override;

   void resized() override;
   void paint(juce::Graphics & g) override;

private:
   void showSystemBrowser(const muscle::String & signaturePattern, const muscle::String & systemName);
   void showPeerBrowser(const muscle::String & signature, const muscle::String & systemName, const zg::ZGPeerID & peerID, const juce::String & peerAddress);
   void showBrowser(std::unique_ptr<NodeSource> source, const juce::String & title, const juce::String & targetDescription);
   void showDiscovery();

   // Declaration order matters:  the callback mechanism has to outlive everything
   // that posts callbacks through it, and the discovery client has to outlive the
   // view that registers with it.
   muscle::JUCECallbackMechanism _callbackMechanism;
   zg::SystemDiscoveryClient _discoveryClient;
   DiscoveryComponent _discoveryView;
   std::unique_ptr<BrowserComponent> _browserView;

   JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
