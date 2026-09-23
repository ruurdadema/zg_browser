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

   _hintLabel.setText("Double-click a system to browse its database, or one of its peers to browse that peer's whole MUSCLE node tree.", juce::dontSendNotification);
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
   const int previouslySelectedRow = _listBox.getSelectedRow();
   const juce::String previouslySelected = ((previouslySelectedRow >= 0)&&(previouslySelectedRow < (int) _rows.size())) ? _rows[(size_t) previouslySelectedRow].getKey() : juce::String();

   std::vector<Row> systemRows;
   std::vector<std::vector<Row> > peerRowsPerSystem;

   for (ConstHashtableIterator<String, MessageRef> iter(_systems); iter.HasData(); iter++)
   {
      Row row;
      row._systemName = iter.GetKey();
      row._heading    = zgb::toJuce(row._systemName);

      std::vector<Row> peerRows;

      ConstMessageRef peerInfo;
      for (uint32 i=0; iter.GetValue()()->FindMessage(ZG_DISCOVERY_NAME_PEERINFO, i, peerInfo).IsOK(); i++)
      {
         if (row._signature.IsEmpty()) row._signature = peerInfo()->GetString(ZG_DISCOVERY_NAME_SIGNATURE);

         Row peerRow;
         peerRow._isPeer     = true;
         peerRow._systemName = row._systemName;
         peerRow._signature  = peerInfo()->GetString(ZG_DISCOVERY_NAME_SIGNATURE);
         (void) peerInfo()->FindFlat(ZG_DISCOVERY_NAME_PEERID, peerRow._peerID);

         // "src" is where the peer's discovery reply came from; "port" is the TCP
         // port its MUSCLE server accepts on (a peer attribute, so it may be absent).
         const String source = peerInfo()->GetString(ZG_DISCOVERY_NAME_SOURCE);
         IPAddressAndPort iap(source, 0, false);
         uint16 port = 0;
         if ((iap.GetIPAddress().IsValid())&&(peerInfo()->FindInt16("port", port).IsOK())&&(port > 0))
         {
            iap.SetPort(port);
            peerRow._peerAddress = zgb::toJuce(iap.ToString());
         }

         peerRow._heading = peerRow._peerAddress.isNotEmpty() ? peerRow._peerAddress
                                                              : (zgb::toJuce(source.HasChars() ? source : String("(unknown address)")) + "  (no MUSCLE port advertised)");
         peerRow._detail  = "peer " + zgb::toJuce(peerRow._peerID.ToString());

         peerRows.push_back(peerRow);
      }

      std::sort(peerRows.begin(), peerRows.end(), [](const Row & a, const Row & b)
      {
         return a._heading.compareNatural(b._heading) < 0;
      });

      const size_t numPeers = peerRows.size();
      row._detail = zgb::toJuce(row._signature.IsEmpty() ? String("(unknown signature)") : row._signature)
                  + juce::String("  |  ") + juce::String((int) numPeers) + (numPeers == 1 ? " peer" : " peers");

      systemRows.push_back(row);
      peerRowsPerSystem.push_back(peerRows);
   }

   // Sort the systems, keeping each one's peers with it
   std::vector<size_t> order(systemRows.size());
   for (size_t i=0; i<order.size(); i++) order[i] = i;
   std::sort(order.begin(), order.end(), [&systemRows](size_t a, size_t b)
   {
      return systemRows[a]._heading.compareNatural(systemRows[b]._heading) < 0;
   });

   _rows.clear();
   int rowToSelect = -1;
   for (size_t idx : order)
   {
      _rows.push_back(systemRows[idx]);
      for (const Row & peerRow : peerRowsPerSystem[idx]) _rows.push_back(peerRow);
   }
   for (size_t i=0; i<_rows.size(); i++) if ((previouslySelected.isNotEmpty())&&(_rows[i].getKey() == previouslySelected)) rowToSelect = (int) i;

   _listBox.updateContent();
   if (rowToSelect >= 0) _listBox.selectRow(rowToSelect, true, true);
                    else _listBox.deselectAllRows();
   repaint();
}

int DiscoveryComponent :: getNumRows()
{
   return (int) _rows.size();
}

void DiscoveryComponent :: paintListBoxItem(int rowNumber, juce::Graphics & g, int width, int height, bool rowIsSelected)
{
   if ((rowNumber < 0)||(rowNumber >= (int) _rows.size())) return;
   const Row & row = _rows[(size_t) rowNumber];

   if (rowIsSelected) g.fillAll(zgb::theme::accent);

   const int indent = row._isPeer ? 36 : 12;
   const bool connectable = (row._isPeer == false)||(row._peerAddress.isNotEmpty());

   g.setColour(connectable ? zgb::theme::text : zgb::theme::textDim);
   g.setFont(juce::Font(juce::FontOptions(row._isPeer ? 14.0f : 15.0f, row._isPeer ? juce::Font::plain : juce::Font::bold)));
   g.drawText(row._heading, indent, 5, width-indent-12, 20, juce::Justification::centredLeft, true);

   g.setColour(rowIsSelected ? juce::Colours::white.withAlpha(0.8f) : zgb::theme::textDim);
   g.setFont(juce::Font(juce::FontOptions(12.5f)));
   g.drawText(row._detail, indent, 24, width-indent-12, 17, juce::Justification::centredLeft, true);

   g.setColour(zgb::theme::header);
   g.drawHorizontalLine(height-1, 0.0f, (float) width);
}

void DiscoveryComponent :: listBoxItemDoubleClicked(int rowNumber, const juce::MouseEvent &)
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

   const Row & row = _rows[(size_t) rowNumber];
   const String signature = row._signature.IsEmpty() ? String("*") : row._signature;

   if (row._isPeer == false)
   {
      if (onSystemChosen) onSystemChosen(signature, row._systemName);
   }
   else if ((row._peerAddress.isNotEmpty())&&(onPeerChosen)) onPeerChosen(signature, row._systemName, row._peerID, row._peerAddress);
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
