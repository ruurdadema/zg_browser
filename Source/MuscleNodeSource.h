#pragma once

#include "NodeSource.h"

#include "util/Hashtable.h"
#include "zg/ZGPeerID.h"
#include "zg/connector/ClientConnector.h"

/** One peer's MUSCLE server, seen as a plain muscled would show it.
  *
  * Every ZG peer runs a MUSCLE ReflectServer, and the sessions its clients get
  * are ordinary StorageReflectSessions -- so once connected we skip the ZG
  * MessageTree layer and talk raw MUSCLE:  PR_COMMAND_SETPARAMETERS to
  * subscribe, PR_COMMAND_GETDATATREES to search, PR_COMMAND_PING to ping.
  * Paths are absolute, so the tree's root is the server's global root:  host
  * nodes below it, session nodes below those (the ZG database lives in the
  * session whose host is "zg"), and each session's own nodes below that.
  *
  * zg::ClientConnector still handles the discovery, TCP connection and
  * automatic reconnection -- we only let it connect to the one peer we were
  * asked to browse, so a lost connection waits for that peer to come back
  * rather than failing over to another peer of the same system.
  */
class MuscleNodeSource final : public NodeSource
{
public:
   MuscleNodeSource(muscle::ICallbackMechanism & callbackMechanism,
                    const muscle::String & signaturePattern,
                    const muscle::String & systemName,
                    const zg::ZGPeerID & peerID);
   ~MuscleNodeSource() override;

   // NodeSource
   muscle::status_t start() override;
   bool isConnected() const override;
   juce::String getConnectedHostDescription() const override;
   muscle::status_t subscribeToChildrenOf(const muscle::String & nodePath) override;
   void unsubscribeFromChildrenOf(const muscle::String & nodePath) override;
   muscle::status_t requestSearch(const muscle::String & namePattern, const muscle::String & tag) override;
   muscle::status_t ping(const muscle::String & tag) override;

private:
   class PeerConnector final : public zg::ClientConnector
   {
   public:
      PeerConnector(muscle::ICallbackMechanism * mechanism, MuscleNodeSource & owner, const zg::ZGPeerID & peerID)
         : zg::ClientConnector(mechanism), _owner(owner), _peerID(peerID) {/* empty */}

      muscle::status_t send(const muscle::ConstMessageRef & msg) {return SendOutgoingMessageToNetwork(msg);}

   protected:
      muscle::status_t ParseTCPPortFromMessage(const muscle::Message & msg, uint16 & retPort) const override;
      void ConnectionStatusUpdated(const muscle::MessageRef & optServerInfo) override;
      void MessageReceivedFromNetwork(const muscle::MessageRef & msg) override;

   private:
      MuscleNodeSource & _owner;
      const zg::ZGPeerID _peerID;   // const, since ParseTCPPortFromMessage() reads it from the I/O thread
   };

   void connectionStatusUpdated();
   void messageReceived(const muscle::Message & msg);

   /** Returns the subscription path for the children of (nodePath), eg "/zg/<wildcard>" for "zg". */
   static muscle::String childrenSubscriptionPath(const muscle::String & nodePath);

   /** Returns (absolutePath) without its leading slash -- the form our listener deals in. */
   static muscle::String toTreePath(const muscle::String & absolutePath);

   /** Subscribes to the children of each of (nodePaths).  (isNewConnection) also sets up our fresh server session. */
   muscle::status_t sendSubscribe(const muscle::Queue<muscle::String> & nodePaths, bool isNewConnection);

   const muscle::String _signaturePattern;
   const muscle::String _systemName;
   PeerConnector _connector;

   // The node paths whose children we're subscribed to.  The server forgets them
   // when the connection drops, so we re-send them all when it comes back.
   muscle::Hashtable<muscle::String, muscle::Void> _subscribedPaths;

   JUCE_DECLARE_NON_COPYABLE(MuscleNodeSource)
};
