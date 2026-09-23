#pragma once

#include <utility>
#include <vector>

#include "MuscleJuce.h"
#include "message/Message.h"

/** Where the BrowserComponent's tree comes from.
  *
  * Paths exchanged through this interface are relative to the root of the tree
  * the source exposes, and never have a leading slash:  "" is the root itself,
  * "foo/bar" is a grandchild of it.  What that root *is* depends on the source --
  * the session-root of a ZG system's database (ZGNodeSource), or the root of one
  * peer's whole MUSCLE node-tree (MuscleNodeSource).
  */
class NodeSource
{
public:
   /** Receives a NodeSource's news.  Every call arrives on the message thread. */
   class Listener
   {
   public:
      virtual ~Listener() = default;

      /** A node we're subscribed to was created or changed (optPayload non-NULL), or went away (optPayload NULL). */
      virtual void nodeUpdated(const muscle::String & nodePath, const muscle::ConstMessageRef & optPayload) = 0;

      /** The reply to an earlier requestSearch() call:  the matching nodes and their payloads. */
      virtual void searchResultsReturned(const muscle::String & tag, const std::vector<std::pair<muscle::String, muscle::ConstMessageRef> > & results) = 0;

      /** The reply to an earlier ping() call. */
      virtual void pongReceived(const muscle::String & tag) = 0;

      /** isConnected() has (or may have) changed its answer. */
      virtual void connectionStateChanged() = 0;

      /** Called after a burst of nodeUpdated() calls, so the listener can do its expensive work once. */
      virtual void callbackBatchEnded() = 0;
   };

   virtual ~NodeSource() = default;

   void setListener(Listener * listener) {_listener = listener;}

   /** Starts connecting.  Call this only once the listener is ready for callbacks. */
   virtual muscle::status_t start() = 0;

   virtual bool isConnected() const = 0;

   /** Where we're connected to (eg an IP address), or "" if we can't tell. */
   virtual juce::String getConnectedHostDescription() const = 0;

   /** Subscribes to the children of (nodePath).  Subscriptions outlive a lost
     * connection:  the source re-establishes them when it reconnects.
     */
   virtual muscle::status_t subscribeToChildrenOf(const muscle::String & nodePath) = 0;
   virtual void unsubscribeFromChildrenOf(const muscle::String & nodePath) = 0;

   /** Asks for every node whose name matches (namePattern), at any depth. */
   virtual muscle::status_t requestSearch(const muscle::String & namePattern, const muscle::String & tag) = 0;

   /** The pong for this ping arrives only after the initial results of every subscription made before it. */
   virtual muscle::status_t ping(const muscle::String & tag) = 0;

protected:
   Listener * _listener = nullptr;
};
