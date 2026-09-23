#include "MainComponent.h"

#include "MuscleNodeSource.h"
#include "Theme.h"
#include "ZGNodeSource.h"

using namespace muscle;

MainComponent :: MainComponent()
   : _discoveryClient(&_callbackMechanism, "*")   // "*" == every kind of ZG server
   , _discoveryView(_discoveryClient)
{
   _discoveryView.onSystemChosen = [this](const String & signaturePattern, const String & systemName)
   {
      showSystemBrowser(signaturePattern, systemName);
   };
   _discoveryView.onPeerChosen = [this](const String & signature, const String & systemName, const zg::ZGPeerID & peerID, const juce::String & peerAddress)
   {
      showPeerBrowser(signature, systemName, peerID, peerAddress);
   };
   addAndMakeVisible(_discoveryView);

   status_t ret;
   if (_discoveryClient.Start().IsError(ret))
   {
      LogTime(MUSCLE_LOG_ERROR, "Couldn't start the SystemDiscoveryClient [%s]\n", ret());
   }

   setSize(1100, 700);
}

MainComponent :: ~MainComponent()
{
   _browserView.reset();
   _discoveryClient.Stop();
}

void MainComponent :: showSystemBrowser(const String & signaturePattern, const String & systemName)
{
   const juce::String name = zgb::toJuce(systemName);
   showBrowser(std::make_unique<ZGNodeSource>(_callbackMechanism, signaturePattern, systemName),
               name, "system \"" + name + "\"");
}

void MainComponent :: showPeerBrowser(const String & signature, const String & systemName, const zg::ZGPeerID & peerID, const juce::String & peerAddress)
{
   const juce::String name = zgb::toJuce(systemName);
   showBrowser(std::make_unique<MuscleNodeSource>(_callbackMechanism, signature, systemName, peerID),
               name + juce::String::fromUTF8("  \u203a  ") + peerAddress + "  (MUSCLE)",
               "peer " + peerAddress + " of \"" + name + "\"");
}

void MainComponent :: showBrowser(std::unique_ptr<NodeSource> source, const juce::String & title, const juce::String & targetDescription)
{
   _browserView.reset();   // the old browser's connection goes away before the new one starts
   _browserView.reset(new BrowserComponent(std::move(source), title, targetDescription));
   _browserView->onBackButtonClicked = [this] {showDiscovery();};
   addAndMakeVisible(*_browserView);
   _browserView->setBounds(getLocalBounds());

   _discoveryView.setVisible(false);
}

void MainComponent :: showDiscovery()
{
   // Deleting the browser also stops its connector thread and drops its subscriptions.
   _browserView.reset();
   _discoveryView.setVisible(true);
   _discoveryView.grabKeyboardFocus();
}

void MainComponent :: paint(juce::Graphics & g)
{
   g.fillAll(zgb::theme::background);
}

void MainComponent :: resized()
{
   _discoveryView.setBounds(getLocalBounds());
   if (_browserView) _browserView->setBounds(getLocalBounds());
}
