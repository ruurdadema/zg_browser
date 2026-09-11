#include "MainComponent.h"

#include "Theme.h"

using namespace muscle;

MainComponent :: MainComponent()
   : _discoveryClient(&_callbackMechanism, "*")   // "*" == every kind of ZG server
   , _discoveryView(_discoveryClient)
{
   _discoveryView.onSystemChosen = [this](const String & signaturePattern, const String & systemName)
   {
      showBrowserFor(signaturePattern, systemName);
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

void MainComponent :: showBrowserFor(const String & signaturePattern, const String & systemName)
{
   _browserView.reset(new BrowserComponent(_callbackMechanism, signaturePattern, systemName));
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
