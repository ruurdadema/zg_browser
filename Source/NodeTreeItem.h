#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "MuscleJuce.h"

class BrowserComponent;

/** One node of the server's database tree.
  *
  * Every item claims that it might contain sub-items, because the only way to
  * find out whether a node has children is to subscribe to them -- which is
  * exactly what opening the item does.
  */
class NodeTreeItem final : public juce::TreeViewItem
{
public:
   NodeTreeItem(BrowserComponent & owner, const muscle::String & name);

   /** This node's name within its parent ("" for the root node). */
   const muscle::String & getNodeName() const {return _name;}

   /** This node's session-relative path ("" for the root node, "srv/foo" for a grandchild). */
   muscle::String getNodePath() const;

   NodeTreeItem * getChildByName(const muscle::String & name) const;

   /** Creates a child item, inserted so that children stay in name order. */
   NodeTreeItem * addChildNode(const muscle::String & name);

   /** Deletes the named child item (and its descendants), if present. */
   void removeChildNode(const muscle::String & name);

   /** True iff we currently hold a subscription to this node's children. */
   bool isSubscribed() const {return _subscribed;}
   void setSubscribed(bool subscribed) {_subscribed = subscribed;}

   // TreeViewItem
   bool mightContainSubItems() override {return true;}
   juce::String getUniqueName() const override;
   int getItemHeight() const override {return 22;}
   void paintItem(juce::Graphics & g, int width, int height) override;
   void itemOpennessChanged(bool isNowOpen) override;
   void itemSelectionChanged(bool isNowSelected) override;

private:
   BrowserComponent & _owner;
   const muscle::String _name;
   bool _subscribed = false;

   JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeTreeItem)
};
