#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "MainComponent.h"
#include "ZGLookAndFeel.h"

#include "system/SetupSystem.h"

/** A browser for ZG (zg_choir) systems, in the spirit of muscle's qt_muscled_browser. */
class ZGBrowserApplication final : public juce::JUCEApplication
{
public:
   ZGBrowserApplication() = default;

   const juce::String getApplicationName() override    {return "ZG Browser";}
   const juce::String getApplicationVersion() override {return "0.1.0";}
   bool moreThanOneInstanceAllowed() override          {return true;}

   void initialise(const juce::String &) override
   {
      // MUSCLE requires this object to exist for as long as any muscle/zg code runs.
      _setupSystem.reset(new muscle::CompleteSetupSystem());

      juce::LookAndFeel::setDefaultLookAndFeel(&_lookAndFeel);

      _mainWindow.reset(new MainWindow(getApplicationName()));
   }

   void shutdown() override
   {
      _mainWindow.reset();
      juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
      _setupSystem.reset();
   }

   void systemRequestedQuit() override {quit();}

   class MainWindow final : public juce::DocumentWindow
   {
   public:
      explicit MainWindow(const juce::String & name)
         : juce::DocumentWindow(name,
                                juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId),
                                juce::DocumentWindow::allButtons)
      {
         setUsingNativeTitleBar(true);
         setContentOwned(new MainComponent(), true);
         setResizable(true, false);
         setResizeLimits(600, 400, 10000, 10000);
         centreWithSize(getWidth(), getHeight());
         setVisible(true);
      }

      void closeButtonPressed() override {juce::JUCEApplication::getInstance()->systemRequestedQuit();}

   private:
      JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
   };

private:
   std::unique_ptr<muscle::CompleteSetupSystem> _setupSystem;
   ZGLookAndFeel _lookAndFeel;
   std::unique_ptr<MainWindow> _mainWindow;
};

START_JUCE_APPLICATION(ZGBrowserApplication)
