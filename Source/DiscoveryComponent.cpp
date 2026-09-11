#include "DiscoveryComponent.h"

#include <algorithm>

#include "Theme.h"

#include "zg/discovery/common/DiscoveryUtilityFunctions.h"

using namespace muscle;

DiscoveryComponent :: DiscoveryComponent(zg::SystemDiscoveryClient & discoveryClient)
   : zg::IDiscoveryNotificationTarget(&discoveryClient)
{
   _titleLabel.setText("ZG systems on this network", juce::dontSendNotification);
   _titleLabel.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold)));
   addAndMakeVisible(_titleLabel);

   _hintLabel.setText("Click a system to browse its database.", juce::dontSendNotification);
   _hintLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
   _hintLabel.setColour(juce::Label::textColourId, zgb::theme::textDim);
   addAndMakeVisible(_hintLabel);

   _listBox.setRowHeight(46);
   addAndMakeVisible(_listBox);

   _manualLabel.setText("Or connect to a system by name:", juce::dontSendNotification);
   _manualLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
   _manualLabel.setColour(juce::Label::textColourId, zgb::theme::textDim);
   addAndMakeVisible(_manualLabel);

   _manualName.setFont(juce::Font(juce::FontOptions(13.5f)));
   _manualName.setIndents(8, 5);
   _manualName.setTextToShowWhenEmpty("system name (wildcards allowed, eg *)", zgb::theme::textDim.darker(0.3f));
   _manualName.onReturnKey = [this] {connectToTypedSystemName();};
   addAndMakeVisible(_manualName);

   _manualConnect.setColour(juce::TextButton::buttonColourId, zgb::theme::accent);
   _manualConnect.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
   _manualConnect.onClick = [this] {connectToTypedSystemName();};
   addAndMakeVisible(_manualConnect);
}

DiscoveryComponent :: ~DiscoveryComponent()
{
   SetDiscoveryClient(NULL);   // no more callbacks, please
}

void DiscoveryComponent :: DiscoveryUpdate(const String & systemName, const MessageRef & optSystemInfo)
{
   if (optSystemInfo()) (void) _systems.Put(systemName, optSystemInfo);
                   else (void) _systems.Remove(systemName);
   rebuildRows();
}

void DiscoveryComponent :: rebuildRows()
{
   const int previouslySelected = _listBox.getSelectedRow();

   _rows.clear();
   _rows.reserve(_systems.GetNumItems());

   for (ConstHashtableIterator<String, MessageRef> iter(_systems); iter.HasData(); iter++)
   {
      SystemRow row;
      row._systemName = iter.GetKey();

      juce::StringArray addresses;
      uint32 numPeers = 0;

      ConstMessageRef peerInfo;
      for (uint32 i=0; iter.GetValue()()->FindMessage(ZG_DISCOVERY_NAME_PEERINFO, i, peerInfo).IsOK(); i++)
      {
         numPeers++;
         if (row._signature.IsEmpty()) row._signature = peerInfo()->GetString(ZG_DISCOVERY_NAME_SIGNATURE);

         const String source = peerInfo()->GetString(ZG_DISCOVERY_NAME_SOURCE);
         if (source.HasChars()) addresses.addIfNotAlreadyThere(zgb::toJuce(source));
      }

      row._detail = zgb::toJuce(row._signature.IsEmpty() ? String("(unknown signature)") : row._signature)
                  + juce::String("  |  ") + juce::String((int) numPeers) + (numPeers == 1 ? " peer" : " peers")
                  + (addresses.isEmpty() ? juce::String() : ("  |  " + addresses.joinIntoString(", ")));

      _rows.push_back(row);
   }

   std::sort(_rows.begin(), _rows.end(), [](const SystemRow & a, const SystemRow & b)
   {
      return zgb::toJuce(a._systemName).compareNatural(zgb::toJuce(b._systemName)) < 0;
   });

   _listBox.updateContent();
   if ((previouslySelected >= 0)&&(previouslySelected < (int) _rows.size())) _listBox.selectRow(previouslySelected, true, false);
   repaint();
}

int DiscoveryComponent :: getNumRows()
{
   return (int) _rows.size();
}

void DiscoveryComponent :: paintListBoxItem(int rowNumber, juce::Graphics & g, int width, int height, bool rowIsSelected)
{
   if ((rowNumber < 0)||(rowNumber >= (int) _rows.size())) return;
   const SystemRow & row = _rows[(size_t) rowNumber];

   if (rowIsSelected) g.fillAll(zgb::theme::accent);

   g.setColour(zgb::theme::text);
   g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
   g.drawText(zgb::toJuce(row._systemName), 12, 5, width-24, 20, juce::Justification::centredLeft, true);

   g.setColour(rowIsSelected ? juce::Colours::white.withAlpha(0.8f) : zgb::theme::textDim);
   g.setFont(juce::Font(juce::FontOptions(12.5f)));
   g.drawText(row._detail, 12, 24, width-24, 17, juce::Justification::centredLeft, true);

   g.setColour(zgb::theme::header);
   g.drawHorizontalLine(height-1, 0.0f, (float) width);
}

void DiscoveryComponent :: listBoxItemClicked(int rowNumber, const juce::MouseEvent &)
{
   chooseRow(rowNumber);
}

void DiscoveryComponent :: returnKeyPressed(int lastRowSelected)
{
   chooseRow(lastRowSelected);
}

void DiscoveryComponent :: chooseRow(int rowNumber)
{
   if ((rowNumber < 0)||(rowNumber >= (int) _rows.size())) return;

   const SystemRow & row = _rows[(size_t) rowNumber];
   if (onSystemChosen) onSystemChosen(row._signature.IsEmpty() ? String("*") : row._signature, row._systemName);
}

void DiscoveryComponent :: connectToTypedSystemName()
{
   const juce::String typed = _manualName.getText().trim();
   if ((typed.isNotEmpty())&&(onSystemChosen)) onSystemChosen(String("*"), zgb::toMuscle(typed));
}

void DiscoveryComponent :: paint(juce::Graphics & g)
{
   g.fillAll(zgb::theme::background);

   if (_rows.empty())
   {
      g.setColour(zgb::theme::textDim);
      g.setFont(juce::Font(juce::FontOptions(15.0f)));
      g.drawFittedText("Listening for ZG systems...", _listBox.getBounds(), juce::Justification::centred, 2);
   }
}

void DiscoveryComponent :: resized()
{
   auto r = getLocalBounds().reduced(16);

   _titleLabel.setBounds(r.removeFromTop(28));
   _hintLabel.setBounds(r.removeFromTop(20));
   r.removeFromTop(8);

   auto manualRow = r.removeFromBottom(30);
   _manualConnect.setBounds(manualRow.removeFromRight(96));
   manualRow.removeFromRight(8);
   _manualLabel.setBounds(manualRow.removeFromLeft(juce::jmin(210, manualRow.getWidth()/2)));
   manualRow.removeFromLeft(8);
   _manualName.setBounds(manualRow);
   r.removeFromBottom(10);

   _listBox.setBounds(r);
}
