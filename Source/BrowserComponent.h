#pragma once

#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "MessagePanel.h"
#include "MuscleJuce.h"
#include "NodeSource.h"
#include "NodeTreeItem.h"
#include "SearchPanel.h"

#include "util/Hashtable.h"

/** Browses the node tree of one NodeSource -- a ZG system's database, or one
  * peer's whole MUSCLE server.
  *
  * The source does the discovery, TCP connection and automatic reconnection for
  * us; we just drive the tree on top of it.  A node of the tree is subscribed to
  * when it is opened and unsubscribed from when it is closed, so the client only
  * ever holds the part of the tree that is actually on screen -- and that part
  * is always live.
  */
class BrowserComponent final : public juce::Component,
                               private NodeSource::Listener
{
public:
   /** @param source where the tree comes from; we start it
     * @param title shown in the header (eg the system's name)
     * @param targetDescription what we're connecting to, for the "not connected" overlay (eg "system Venue")
     */
   BrowserComponent(std::unique_ptr<NodeSource> source,
                    const juce::String & title,
                    const juce::String & targetDescription);

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
   // NodeSource::Listener
   void nodeUpdated(const muscle::String & nodePath, const muscle::ConstMessageRef & optPayload) override;
   void searchResultsReturned(const muscle::String & tag, const std::vector<std::pair<muscle::String, muscle::ConstMessageRef> > & results) override;
   void pongReceived(const muscle::String & tag) override;
   void connectionStateChanged() override;
   void callbackBatchEnded() override;

   void startSearch(const juce::String & searchText);

   /** Opens the tree one level at a time until (_revealPath) is on screen.
     * Each level's children only arrive asynchronously, so after opening a level
     * this pings the server, and is re-driven from TreeLocalPeerPonged() once
     * that level's children are all in -- until it either lands or gives up.
     * @param childrenAreSettled true iff every item that is open has already
     *                           received all of its children (ie we're being
     *                           called in response to our ping's pong)
     */
   void advanceReveal(bool childrenAreSettled);

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

   const juce::String _targetDescription;

   std::unique_ptr<NodeSource> _source;

   // The part of the server's database we're currently holding, by session-relative path
   muscle::Hashtable<muscle::String, muscle::ConstMessageRef> _pathToMessage;

   // The paths of the nodes whose children we're currently subscribed to, one per open tree node
   muscle::Hashtable<muscle::String, muscle::Void> _subscriptions;

   muscle::String _selectedPath;
   bool _hasSelection = false;
   bool _messagePanelNeedsRefresh = false;
   bool _wasConnected = false;
   bool _hasEverConnected = false;

   // The tag of the search we're currently waiting for.  Replies carrying any
   // other tag are from a search the user has already superseded, and are dropped.
   muscle::String _pendingSearchTag;
   uint32 _nextSearchID = 0;

   // Set while we're walking the tree open towards a double-clicked search hit.
   muscle::String _revealPath;

   // The tag of the ping whose pong will tell us the level we just opened has
   // all of its children.  Pongs carrying any other tag are from an abandoned reveal.
   muscle::String _pendingRevealTag;
   uint32 _nextRevealID = 0;

   juce::Label _titleLabel;
   juce::Label _statusLabel;
   juce::TextButton _backButton {juce::String::fromUTF8("\u2190 Systems")};
   juce::TreeView _treeView;
   std::unique_ptr<NodeTreeItem> _rootItem;
   MessagePanel _messagePanel;
   SearchPanel _searchPanel;
   juce::StretchableLayoutManager _layout;
   std::unique_ptr<juce::StretchableLayoutResizerBar> _divider;
   std::unique_ptr<juce::StretchableLayoutResizerBar> _searchDivider;
   StatusOverlay _overlay;

   JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BrowserComponent)
};
