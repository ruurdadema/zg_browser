#include "BrowserComponent.h"

#include <algorithm>

#include "Theme.h"

using namespace muscle;

// ---------------------------------------------------------------------------

BrowserComponent :: StatusOverlay :: StatusOverlay()
{
   setInterceptsMouseClicks(true, false);   // swallow clicks aimed at the tree behind us
}

void BrowserComponent :: StatusOverlay :: setStatusText(const juce::String & text)
{
   _text = text;
   repaint();
}

void BrowserComponent :: StatusOverlay :: paint(juce::Graphics & g)
{
   g.fillAll(zgb::theme::overlay);

   g.setColour(zgb::theme::text);
   g.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold)));
   g.drawFittedText(_text, getLocalBounds().reduced(30), juce::Justification::centred, 3);
}

// ---------------------------------------------------------------------------

BrowserComponent :: BrowserComponent(std::unique_ptr<NodeSource> source,
                                     const juce::String & title,
                                     const juce::String & targetDescription)
   : _targetDescription(targetDescription)
   , _source(std::move(source))
{
   _source->setListener(this);

   _titleLabel.setText(title, juce::dontSendNotification);
   _titleLabel.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
   addAndMakeVisible(_titleLabel);

   _statusLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
   _statusLabel.setJustificationType(juce::Justification::centredRight);
   addAndMakeVisible(_statusLabel);

   _backButton.onClick = [this]
   {
      if (onBackButtonClicked) onBackButtonClicked();
   };
   addAndMakeVisible(_backButton);

   _treeView.setDefaultOpenness(false);
   _treeView.setMultiSelectEnabled(false);
   _treeView.setIndentSize(16);
   addAndMakeVisible(_treeView);

   _rootItem.reset(new NodeTreeItem(*this, GetEmptyString()));
   _treeView.setRootItem(_rootItem.get());
   _treeView.setRootItemVisible(true);

   addAndMakeVisible(_messagePanel);

   _searchPanel.onSearchRequested      = [this](const juce::String & text) {startSearch(text);};
   _searchPanel.onResultClicked        = [this](const String & path, const ConstMessageRef & payload)
   {
      // Show the hit straight from the result row:  the node it names may well
      // not be in the visible tree, so _pathToMessage can't be consulted here.
      _hasSelection = false;
      _selectedPath.Clear();
      _treeView.clearSelectedItems();
      _messagePanel.showNode(path, payload);
   };
   _searchPanel.onResultDoubleClicked  = [this](const String & path)
   {
      _revealPath = path;
      _pendingRevealTag.Clear();   // any pong still on its way belongs to an earlier reveal
      advanceReveal(false);
   };
   _searchPanel.onResultDeselected     = [this]
   {
      // A highlighted row always means the Message pane is showing that row
      // (selecting a tree node drops the highlight), so it's ours to clear.
      _messagePanel.clear();
   };
   _searchPanel.onSearchCleared        = [this]
   {
      _pendingSearchTag.Clear();   // so a reply that's still on its way doesn't bring the results back
   };
   addAndMakeVisible(_searchPanel);

   _layout.setItemLayout(0, 140.0,   -0.8, 280.0);   // tree
   _layout.setItemLayout(1,   7.0,    7.0,   7.0);   // divider
   _layout.setItemLayout(2, 200.0,   -0.9,  -0.5);   // message panel
   _layout.setItemLayout(3,   7.0,    7.0,   7.0);   // divider
   _layout.setItemLayout(4, 160.0,   -0.6, 300.0);   // search panel
   _divider.reset(new juce::StretchableLayoutResizerBar(&_layout, 1, true));
   addAndMakeVisible(*_divider);
   _searchDivider.reset(new juce::StretchableLayoutResizerBar(&_layout, 3, true));
   addAndMakeVisible(*_searchDivider);

   addAndMakeVisible(_overlay);

   // Opening the root subscribes us to the top level of the tree.  It is fine
   // to do this before we're connected:  the source remembers our
   // subscriptions and (re)sends them whenever a connection is established.
   _rootItem->setOpen(true);

   status_t ret;
   if (_source->start().IsError(ret))
   {
      LogTime(MUSCLE_LOG_ERROR, "Couldn't start connecting to %s [%s]\n", targetDescription.toRawUTF8(), ret());
   }

   updateConnectionStateUI();
}

BrowserComponent :: ~BrowserComponent()
{
   _source.reset();        // stops its connection, and with it the callbacks, before anything gets torn down

   _treeView.setRootItem(NULL);
   _rootItem.reset();
}

// ---------------------------------------------------------------------------
//  Subscription bookkeeping
// ---------------------------------------------------------------------------

void BrowserComponent :: subscribeToChildrenOf(const String & nodePath, NodeTreeItem & item)
{
   if (item.isSubscribed()) return;

   status_t ret;
   if (_source->subscribeToChildrenOf(nodePath).IsOK(ret))
   {
      (void) _subscriptions.PutWithDefault(nodePath);
      item.setSubscribed(true);
   }
   else LogTime(MUSCLE_LOG_ERROR, "Couldn't subscribe to the children of [/%s] [%s]\n", nodePath(), ret());
}

void BrowserComponent :: unsubscribeFromChildrenOf(const String & nodePath, NodeTreeItem & item)
{
   if (item.isSubscribed() == false) return;

   _source->unsubscribeFromChildrenOf(nodePath);
   (void) _subscriptions.Remove(nodePath);
   item.setSubscribed(false);
}

void BrowserComponent :: unsubscribeFromDescendantsOf(const String & nodePath, bool includeSelf)
{
   const String prefix = nodePath.IsEmpty() ? GetEmptyString() : (nodePath + "/");

   for (HashtableIterator<String, Void> iter(_subscriptions); iter.HasData(); iter++)
   {
      const String subscribedPath = iter.GetKey();   // deliberately a copy; we may remove this entry below
      const bool isSelf = (subscribedPath == nodePath);
      if ((isSelf ? includeSelf : subscribedPath.StartsWith(prefix)))
      {
         _source->unsubscribeFromChildrenOf(subscribedPath);
         (void) _subscriptions.Remove(subscribedPath);
      }
   }
}

void BrowserComponent :: forgetCachedDataUnder(const String & nodePath, bool includeSelf)
{
   const String prefix = nodePath.IsEmpty() ? GetEmptyString() : (nodePath + "/");

   for (HashtableIterator<String, ConstMessageRef> iter(_pathToMessage); iter.HasData(); iter++)
   {
      const String path = iter.GetKey();      // deliberately a copy; we may remove this entry below
      if (((includeSelf)&&(path == nodePath))||(path.StartsWith(prefix))) (void) _pathToMessage.Remove(path);
   }
}

// ---------------------------------------------------------------------------
//  Tree-item plumbing
// ---------------------------------------------------------------------------

NodeTreeItem * BrowserComponent :: findItemForPath(const String & nodePath) const
{
   NodeTreeItem * item = _rootItem.get();
   if (nodePath.IsEmpty()) return item;

   uint32 startAt = 0;
   while(item)
   {
      const int slash = nodePath.IndexOf('/', startAt);
      item = item->getChildByName((slash >= 0) ? nodePath.Substring(startAt, (uint32)slash) : nodePath.Substring(startAt));
      if (slash < 0) break;
      startAt = ((uint32)slash)+1;
   }

   return item;
}

NodeTreeItem * BrowserComponent :: createChildItem(NodeTreeItem & parentItem, const String & childName)
{
   NodeTreeItem * newItem = parentItem.addChildNode(childName);

   // If this node was open before we lost the connection, re-open it without
   // re-subscribing:  the source still holds (and has re-sent) that subscription.
   if (_subscriptions.ContainsKey(newItem->getNodePath()))
   {
      newItem->setSubscribed(true);
      newItem->setOpen(true);
   }

   return newItem;
}

void BrowserComponent :: nodeItemOpennessChanged(NodeTreeItem & item, bool isNowOpen)
{
   const String nodePath = item.getNodePath();

   if (isNowOpen) subscribeToChildrenOf(nodePath, item);
   else
   {
      unsubscribeFromChildrenOf(nodePath, item);
      unsubscribeFromDescendantsOf(nodePath, false);
      forgetCachedDataUnder(nodePath, false);
      item.clearSubItems();
   }
}

void BrowserComponent :: nodeItemSelected(NodeTreeItem & item)
{
   _selectedPath = item.getNodePath();
   _hasSelection = true;

   // The Message pane now belongs to the tree, so drop the search row's
   // highlight rather than leave it claiming to be what's on show.
   _searchPanel.deselectAll();

   refreshMessagePanel();
}

juce::String BrowserComponent :: getSummaryForPath(const String & nodePath) const
{
   const ConstMessageRef * msg = _pathToMessage.Get(nodePath);
   if (msg == NULL) return juce::String();

   const uint32 numFields = (*msg)()->GetNumNames();
   if (numFields == 0) return juce::String();
   return juce::String((int) numFields) + (numFields == 1 ? " field" : " fields");
}

// ---------------------------------------------------------------------------
//  NodeSource callbacks
// ---------------------------------------------------------------------------

void BrowserComponent :: nodeUpdated(const String & nodePath, const ConstMessageRef & optPayload)
{
   if (optPayload())
   {
      (void) _pathToMessage.Put(nodePath, optPayload);
      handleNodeAddedOrUpdated(nodePath);
   }
   else
   {
      (void) _pathToMessage.Remove(nodePath);
      handleNodeRemoved(nodePath);
   }

   if ((_hasSelection)&&(_selectedPath == nodePath)) _messagePanelNeedsRefresh = true;
}

void BrowserComponent :: handleNodeAddedOrUpdated(const String & nodePath)
{
   NodeTreeItem * parentItem = findItemForPath(zgb::parentPathOf(nodePath));
   if ((parentItem == NULL)||(parentItem->isOpen() == false)) return;   // we're not showing this part of the tree

   const String childName = zgb::leafNameOf(nodePath);
   NodeTreeItem * item = parentItem->getChildByName(childName);
   if (item == NULL) (void) createChildItem(*parentItem, childName);
              else item->repaintItem();
}

void BrowserComponent :: handleNodeRemoved(const String & nodePath)
{
   forgetCachedDataUnder(nodePath, false);
   unsubscribeFromDescendantsOf(nodePath, true);

   NodeTreeItem * parentItem = findItemForPath(zgb::parentPathOf(nodePath));
   if (parentItem) parentItem->removeChildNode(zgb::leafNameOf(nodePath));
}

void BrowserComponent :: callbackBatchEnded()
{
   if (_messagePanelNeedsRefresh)
   {
      _messagePanelNeedsRefresh = false;
      refreshMessagePanel();
   }
}

// ---------------------------------------------------------------------------
//  Search
// ---------------------------------------------------------------------------

void BrowserComponent :: startSearch(const juce::String & searchText)
{
   const String pattern = zgb::toMuscle(searchText).Contains("*")
                        ? zgb::toMuscle(searchText)
                        : zgb::toMuscle(searchText).WithPrepend("*").WithAppend("*");

   _pendingSearchTag = String("zgbsearch%1").Arg(++_nextSearchID);

   status_t ret;
   if (_source->requestSearch(pattern, _pendingSearchTag).IsError(ret))
   {
      LogTime(MUSCLE_LOG_ERROR, "Couldn't request search for [%s] [%s]\n", pattern(), ret());
      _pendingSearchTag.Clear();
      _searchPanel.setResults(std::vector<std::pair<String, ConstMessageRef> >());
   }
}

void BrowserComponent :: searchResultsReturned(const String & tag, const std::vector<std::pair<String, ConstMessageRef> > & unsortedResults)
{
   if ((tag != _pendingSearchTag)||(_pendingSearchTag.IsEmpty())) return;  // superseded by a newer search
   _pendingSearchTag.Clear();

   std::vector<std::pair<String, ConstMessageRef> > results(unsortedResults);
   std::sort(results.begin(), results.end(),
             [](const std::pair<String, ConstMessageRef> & a, const std::pair<String, ConstMessageRef> & b)
             {
                return zgb::toJuce(a.first).compareNatural(zgb::toJuce(b.first)) < 0;
             });

   _searchPanel.setResults(results);
}

void BrowserComponent :: pongReceived(const String & tag)
{
   if ((_pendingRevealTag.IsEmpty())||(tag != _pendingRevealTag)) return;  // not the pong our reveal is waiting for
   _pendingRevealTag.Clear();

   // The server answers a ping only after it has sent the initial results of
   // every subscription we made before it, so each item opened on the way down
   // now holds its complete set of children.
   advanceReveal(true);
}

void BrowserComponent :: advanceReveal(bool childrenAreSettled)
{
   if (_revealPath.IsEmpty()) return;

   // Walk as deep towards the target as the items we currently have allow.
   NodeTreeItem * item = _rootItem.get();
   uint32 startAt = 0;

   while(item)
   {
      const int slash = _revealPath.IndexOf('/', startAt);
      const String segment = (slash >= 0) ? _revealPath.Substring(startAt, (uint32)slash)
                                          : _revealPath.Substring(startAt);

      NodeTreeItem * child = item->getChildByName(segment);
      if (child == NULL)
      {
         // We've heard back about all of this item's children and the one we
         // want isn't among them:  it has gone away since the search was done.
         if ((item->isOpen())&&(childrenAreSettled)) {_revealPath.Clear(); return;}

         // Otherwise its children may still be on their way.  Note we can't just
         // wait for the next CallbackBatchEnds(), as that also fires for updates
         // to unrelated nodes -- instead ping, and let the pong (which trails the
         // results of the subscription that opening the item made) bring us back.
         if (item->isOpen() == false) item->setOpen(true);

         _pendingRevealTag = String("zgbreveal%1").Arg(++_nextRevealID);

         status_t ret;
         if (_source->ping(_pendingRevealTag).IsError(ret))
         {
            LogTime(MUSCLE_LOG_ERROR, "Couldn't ping while revealing [%s] [%s]\n", _revealPath(), ret());
            _pendingRevealTag.Clear();
            _revealPath.Clear();
         }
         return;
      }

      item = child;
      if (slash < 0) break;     // that was the last segment:  we've arrived
      startAt = ((uint32)slash)+1;
   }

   if (item)
   {
      // Deliberately *not* setOpen() here:  the user asked to be shown this
      // node, not to have its children expanded (and subscribed to) as well.
      _treeView.scrollToKeepItemVisible(item);
      item->setSelected(true, true);
   }

   _revealPath.Clear();
}

void BrowserComponent :: connectionStateChanged()
{
   const bool isConnected = _source->isConnected();
   if (isConnected == _wasConnected) return;
   _wasConnected = isConnected;
   if (isConnected) _hasEverConnected = true;

   if (isConnected == false)
   {
      // Our subscriptions are kept (the source re-sends them when the
      // connection comes back), but the data we cached for them is now stale,
      // so drop it and let the fresh subscription-results rebuild the tree.
      _pathToMessage.Clear();
      if (_rootItem) _rootItem->clearSubItems();
      _hasSelection = false;
      _selectedPath.Clear();
      _messagePanel.clear();

      // Results point at nodes we can no longer vouch for, and any descent we
      // were part-way through is now meaningless.
      _searchPanel.clear();
      _pendingSearchTag.Clear();
      _revealPath.Clear();
      _pendingRevealTag.Clear();
   }

   _searchPanel.setConnected(isConnected);
   updateConnectionStateUI();
}

// ---------------------------------------------------------------------------
//  UI
// ---------------------------------------------------------------------------

void BrowserComponent :: refreshMessagePanel()
{
   if (_hasSelection == false) {_messagePanel.clear(); return;}

   const ConstMessageRef * msg = _pathToMessage.Get(_selectedPath);
   _messagePanel.showNode(_selectedPath, msg ? *msg : ConstMessageRef());
}

void BrowserComponent :: updateConnectionStateUI()
{
   const bool isConnected = _source->isConnected();

   juce::String status;
   if (isConnected)
   {
      const juce::String host = _source->getConnectedHostDescription();
      status = host.isEmpty() ? juce::String("Connected") : ("Connected to " + host);
   }
   else status = "Not connected";

   _statusLabel.setColour(juce::Label::textColourId, isConnected ? zgb::theme::connected : zgb::theme::disconnected);
   _statusLabel.setText(status, juce::dontSendNotification);

   if (isConnected == false)
   {
      _overlay.setStatusText(_hasEverConnected
         ? ("Disconnected from " + _targetDescription + "\n\nReconnecting automatically as soon as it comes back...")
         : ("Looking for " + _targetDescription + " on the local network..."));
   }
   _overlay.setVisible(isConnected == false);
}

void BrowserComponent :: paint(juce::Graphics & g)
{
   g.fillAll(zgb::theme::background);

   auto header = getLocalBounds().removeFromTop(38);
   g.setColour(zgb::theme::header);
   g.fillRect(header);
   g.setColour(zgb::theme::border);
   g.drawHorizontalLine(header.getBottom()-1, 0.0f, (float) getWidth());
}

void BrowserComponent :: resized()
{
   auto r = getLocalBounds();

   auto header = r.removeFromTop(38).reduced(6, 5);
   _backButton.setBounds(header.removeFromLeft(104));
   header.removeFromLeft(10);
   _statusLabel.setBounds(header.removeFromRight(juce::jmin(360, header.getWidth()/2)));
   _titleLabel.setBounds(header);

   // Note the overlay covers the content only:  the header (and with it the
   // "back to the systems list" button) stays usable while we're disconnected.
   _overlay.setBounds(r);

   juce::Component * comps[] = {&_treeView, _divider.get(), &_messagePanel, _searchDivider.get(), &_searchPanel};
   _layout.layOutComponents(comps, 5, r.getX(), r.getY(), r.getWidth(), r.getHeight(), false, true);
}
