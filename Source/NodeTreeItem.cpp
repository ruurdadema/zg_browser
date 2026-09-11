#include "NodeTreeItem.h"

#include "BrowserComponent.h"
#include "Theme.h"

using namespace muscle;

NodeTreeItem :: NodeTreeItem(BrowserComponent & owner, const String & name)
   : _owner(owner)
   , _name(name)
{
   // empty
}

String NodeTreeItem :: getNodePath() const
{
   const NodeTreeItem * parent = static_cast<const NodeTreeItem *>(getParentItem());
   if (parent == NULL) return GetEmptyString();  // we're the root node

   const String parentPath = parent->getNodePath();
   return parentPath.IsEmpty() ? _name : (parentPath + "/" + _name);
}

NodeTreeItem * NodeTreeItem :: getChildByName(const String & name) const
{
   for (int i=0; i<getNumSubItems(); i++)
   {
      NodeTreeItem * child = static_cast<NodeTreeItem *>(getSubItem(i));
      if (child->getNodeName() == name) return child;
   }
   return NULL;
}

NodeTreeItem * NodeTreeItem :: addChildNode(const String & name)
{
   const juce::String newName = zgb::toJuce(name);

   int insertAt = getNumSubItems();
   for (int i=0; i<getNumSubItems(); i++)
   {
      const NodeTreeItem * child = static_cast<const NodeTreeItem *>(getSubItem(i));
      if (newName.compareNatural(zgb::toJuce(child->getNodeName())) < 0) {insertAt = i; break;}
   }

   NodeTreeItem * newItem = new NodeTreeItem(_owner, name);
   addSubItem(newItem, insertAt);
   return newItem;
}

void NodeTreeItem :: removeChildNode(const String & name)
{
   for (int i=0; i<getNumSubItems(); i++)
   {
      const NodeTreeItem * child = static_cast<const NodeTreeItem *>(getSubItem(i));
      if (child->getNodeName() == name) {removeSubItem(i, true); return;}
   }
}

juce::String NodeTreeItem :: getUniqueName() const
{
   const String path = getNodePath();
   return path.IsEmpty() ? juce::String("/") : zgb::toJuce(path);
}

void NodeTreeItem :: paintItem(juce::Graphics & g, int width, int height)
{
   if (isSelected()) g.fillAll(zgb::theme::accent);

   const juce::String name = _name.IsEmpty() ? juce::String("/") : zgb::toJuce(_name);
   const juce::String summary = _owner.getSummaryForPath(getNodePath());

   g.setFont(juce::Font(juce::FontOptions((float) height * 0.65f)));

   const int summaryWidth = summary.isEmpty()
                          ? 0
                          : juce::jmin(width/2, juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(), summary) + 12);

   g.setColour(zgb::theme::text);
   g.drawText(name, 2, 0, width - summaryWidth - 4, height, juce::Justification::centredLeft, true);

   if (summaryWidth > 0)
   {
      g.setColour(zgb::theme::textDim);
      g.drawText(summary, width - summaryWidth, 0, summaryWidth - 4, height, juce::Justification::centredRight, true);
   }
}

void NodeTreeItem :: itemOpennessChanged(bool isNowOpen)
{
   _owner.nodeItemOpennessChanged(*this, isNowOpen);
}

void NodeTreeItem :: itemSelectionChanged(bool isNowSelected)
{
   if (isNowSelected) _owner.nodeItemSelected(*this);
}
