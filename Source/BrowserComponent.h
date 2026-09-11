#pragma once

#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "MessagePanel.h"
#include "MuscleJuce.h"
#include "NodeTreeItem.h"

#include "util/Hashtable.h"
#include "zg/messagetree/client/MessageTreeClientConnector.h"
#include "zg/messagetree/gateway/ITreeGatewaySubscriber.h"

/** Browses the database of one ZG system.
  *
  * The MessageTreeClientConnector does the discovery, TCP connection and
  * automatic reconnection for us; we just drive the tree on top of it.  A node
  * of the tree is subscribed to when it is opened and unsubscribed from when it
  * is closed, so the client only ever holds the part of the database that is
  * actually on screen -- and that part is always live.
  */
class BrowserComponent final : public juce::Component,
                               private zg::ITreeGatewaySubscriber
{
public:
   /** @param callbackMechanism marshals the network thread's callbacks onto the JUCE message thread
     * @param signaturePattern the kind of ZG server to connect to (may be wildcarded, eg "*")
     * @param systemNamePattern the ZG system to connect to (may be wildcarded)
     */
   BrowserComponent(muscle::ICallbackMechanism & callbackMechanism,
                    const muscle::String & signaturePattern,
                    const muscle::String & systemNamePattern);

   ~BrowserComponent() override;

   /** Called when the user wants to go back to the discovery list. */
   std::function<void()> onBackButtonClicked;

   void resized() override;
   void paint(juce::Graphics & g) override;

   // Called by our NodeTreeItems
   void nodeItemOpennessChanged(NodeTreeItem & item, bool isNowOpen);
   void nodeItemSelected(NodeTreeItem & item);

   /** Returns a short right-aligned annotation for a node in the tree (eg "3 fields"). */
   juce::String getSummaryForPath(const muscle::String & nodePath) const;

private:
   // ITreeGatewaySubscriber
   void TreeNodeUpdated(const muscle::String & nodePath, const muscle::ConstMessageRef & optPayloadMsg, const muscle::String & optOpTag) override;
   void TreeGatewayConnectionStateChanged() override;
   void CallbackBatchEnds() override;

   void subscribeToChildrenOf(const muscle::String & nodePath, NodeTreeItem & item);
   void unsubscribeFromChildrenOf(const muscle::String & nodePath, NodeTreeItem & item);
   void unsubscribeFromDescendantsOf(const muscle::String & nodePath, bool includeSelf);
   void forgetCachedDataUnder(const muscle::String & nodePath, bool includeSelf);

   NodeTreeItem * findItemForPath(const muscle::String & nodePath) const;
   NodeTreeItem * createChildItem(NodeTreeItem & parentItem, const muscle::String & childName);

   void handleNodeAddedOrUpdated(const muscle::String & nodePath);
   void handleNodeRemoved(const muscle::String & nodePath);
   void refreshMessagePanel();
   void updateConnectionStateUI();

   /** Semi-transparent "we're not connected right now" cover. */
   class StatusOverlay final : public juce::Component
   {
   public:
      StatusOverlay();
      void setStatusText(const juce::String & text);
      void paint(juce::Graphics & g) override;
   private:
      juce::String _text;
   };

   const muscle::String _systemName;

   zg::MessageTreeClientConnector _connector;

   // The part of the server's database we're currently holding, by session-relative path
   muscle::Hashtable<muscle::String, muscle::ConstMessageRef> _pathToMessage;

   // The subscription-strings (eg "srv/*") we currently hold, one per open tree node
   muscle::Hashtable<muscle::String, muscle::Void> _subscriptions;

   muscle::String _selectedPath;
   bool _hasSelection = false;
   bool _messagePanelNeedsRefresh = false;
   bool _wasConnected = false;
   bool _hasEverConnected = false;

   juce::Label _titleLabel;
   juce::Label _statusLabel;
   juce::TextButton _backButton {juce::String::fromUTF8("\u2190 Systems")};
   juce::TreeView _treeView;
   std::unique_ptr<NodeTreeItem> _rootItem;
   MessagePanel _messagePanel;
   juce::StretchableLayoutManager _layout;
   std::unique_ptr<juce::StretchableLayoutResizerBar> _divider;
   StatusOverlay _overlay;

   JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BrowserComponent)
};
