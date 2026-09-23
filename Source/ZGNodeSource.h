#pragma once

#include "NodeSource.h"

#include "zg/messagetree/client/MessageTreeClientConnector.h"
#include "zg/messagetree/gateway/ITreeGatewaySubscriber.h"

/** A ZG system's database, seen through the MessageTree gateway.
  *
  * The MessageTreeClientConnector does the discovery, TCP connection and
  * automatic reconnection (to whichever of the system's peers is available);
  * paths are session-relative, so the tree's root is the database's root.
  *
  * Note that session-relative subscriptions match that level of *every*
  * session on the server, so nodes that clients keep in their own sessions
  * (eg a ClientDataMessageTreeDatabaseObject's local copies) show up at the
  * root alongside the database's own.  Browse a single peer (MuscleNodeSource)
  * to see where each node really lives.
  */
class ZGNodeSource final : public NodeSource,
                           private zg::ITreeGatewaySubscriber
{
public:
   ZGNodeSource(muscle::ICallbackMechanism & callbackMechanism,
                const muscle::String & signaturePattern,
                const muscle::String & systemNamePattern);
   ~ZGNodeSource() override;

   // NodeSource
   muscle::status_t start() override;
   bool isConnected() const override;
   juce::String getConnectedHostDescription() const override;
   muscle::status_t subscribeToChildrenOf(const muscle::String & nodePath) override;
   void unsubscribeFromChildrenOf(const muscle::String & nodePath) override;
   muscle::status_t requestSearch(const muscle::String & namePattern, const muscle::String & tag) override;
   muscle::status_t ping(const muscle::String & tag) override;

private:
   // ITreeGatewaySubscriber
   void TreeNodeUpdated(const muscle::String & nodePath, const muscle::ConstMessageRef & optPayloadMsg, const muscle::String & optOpTag) override;
   void SubtreesRequestResultReturned(const muscle::String & tag, const muscle::MessageRef & subtreeData) override;
   void TreeGatewayConnectionStateChanged() override;
   void TreeLocalPeerPonged(const muscle::String & tag) override;
   void CallbackBatchEnds() override;

   const muscle::String _signaturePattern;
   const muscle::String _systemNamePattern;
   zg::MessageTreeClientConnector _connector;

   JUCE_DECLARE_NON_COPYABLE(ZGNodeSource)
};
