#include "ZGNodeSource.h"

#include "reflector/StorageReflectConstants.h"               // for PR_NAME_NODEDATA
#include "zg/discovery/common/DiscoveryUtilityFunctions.h"   // for ZG_DISCOVERY_NAME_*

using namespace muscle;

ZGNodeSource :: ZGNodeSource(ICallbackMechanism & callbackMechanism,
                             const String & signaturePattern,
                             const String & systemNamePattern)
   : zg::ITreeGatewaySubscriber(NULL)   // our gateway isn't constructed yet; we register below
   , _signaturePattern(signaturePattern)
   , _systemNamePattern(systemNamePattern)
   , _connector(&callbackMechanism)
{
   SetGateway(&_connector);
}

ZGNodeSource :: ~ZGNodeSource()
{
   SetGateway(NULL);       // stop receiving callbacks before anything gets torn down
   _connector.Stop();
}

status_t ZGNodeSource :: start()
{
   return _connector.Start(_signaturePattern, _systemNamePattern);
}

bool ZGNodeSource :: isConnected() const
{
   return IsTreeGatewayConnected();
}

juce::String ZGNodeSource :: getConnectedHostDescription() const
{
   const MessageRef peerInfo = _connector.GetConnectedPeerInfo();
   const String source = peerInfo() ? peerInfo()->GetString(ZG_DISCOVERY_NAME_SOURCE) : GetEmptyString();
   const IPAddressAndPort sourceIAP(source, 0, false);
   return zgb::toJuce(sourceIAP.GetIPAddress().IsValid() ? sourceIAP.ToString(false) : source);
}

status_t ZGNodeSource :: subscribeToChildrenOf(const String & nodePath)
{
   // It is fine to do this before we're connected:  the gateway remembers our
   // subscriptions and (re)sends them whenever a connection is established.
   return AddTreeSubscription(zgb::childrenSubscriptionString(nodePath));
}

void ZGNodeSource :: unsubscribeFromChildrenOf(const String & nodePath)
{
   (void) RemoveTreeSubscription(zgb::childrenSubscriptionString(nodePath));
}

status_t ZGNodeSource :: requestSearch(const String & namePattern, const String & tag)
{
   // A path clause never spans a '/', so a pattern only ever matches nodes at
   // its own depth -- to search the whole database we ask for every depth at
   // once.  RequestTreeNodeSubtrees takes the whole list in a single request.
   Queue<String> queryStrings;
   String prefix;
   for (uint32 i=0; i<zgb::kMaxSearchDepth; i++)
   {
      (void) queryStrings.AddTail(prefix + namePattern);
      prefix += "*/";
   }

   return RequestTreeNodeSubtrees(queryStrings, Queue<ConstQueryFilterRef>(), tag, zgb::kMaxSearchDepth);
}

status_t ZGNodeSource :: ping(const String & tag)
{
   return PingTreeLocalPeer(tag);
}

void ZGNodeSource :: TreeNodeUpdated(const String & nodePath, const ConstMessageRef & optPayloadMsg, const String & /*optOpTag*/)
{
   if (nodePath.IsEmpty()) return;   // the session-root itself is never shown as a child

   if (_listener) _listener->nodeUpdated(nodePath, optPayloadMsg);
   if ((_listener)&&(IsInCallbackBatch() == false)) _listener->callbackBatchEnded();
}

void ZGNodeSource :: SubtreesRequestResultReturned(const String & tag, const MessageRef & subtreeData)
{
   std::vector<std::pair<String, ConstMessageRef> > results;

   if (subtreeData())
   {
      // Each matched node is one top-level field, keyed by its node path, whose
      // value is the SaveNodeTreeToMessage() form (payload under PR_NAME_NODEDATA).
      // The gateway has already made those paths session-relative for us.
      MessageRef nodeRef;
      for (MessageFieldNameIterator iter = subtreeData()->GetFieldNameIterator(B_MESSAGE_TYPE); iter.HasData(); iter++)
      {
         const String & fieldName = iter.GetFieldName();
         if (subtreeData()->FindMessage(fieldName, 0, nodeRef).IsError()) continue;

         MessageRef payloadRef;
         (void) nodeRef()->FindMessage(PR_NAME_NODEDATA, payloadRef);

         results.push_back(std::make_pair(fieldName, ConstMessageRef(payloadRef)));
      }
   }

   if (_listener) _listener->searchResultsReturned(tag, results);
}

void ZGNodeSource :: TreeGatewayConnectionStateChanged()
{
   if (_listener) _listener->connectionStateChanged();
}

void ZGNodeSource :: TreeLocalPeerPonged(const String & tag)
{
   if (_listener) _listener->pongReceived(tag);
}

void ZGNodeSource :: CallbackBatchEnds()
{
   if (_listener) _listener->callbackBatchEnded();
}
