#include "MuscleNodeSource.h"

#include "reflector/StorageReflectConstants.h"               // for PR_COMMAND_*, PR_RESULT_*, PR_NAME_*
#include "regex/StringMatcher.h"                             // for EscapeRegexTokens()
#include "zg/discovery/common/DiscoveryUtilityFunctions.h"   // for ZG_DISCOVERY_NAME_*

using namespace muscle;

static const String kPingTagField = "zgb_tag";   // our pings' tag, echoed back in the pong

// ---------------------------------------------------------------------------

status_t MuscleNodeSource :: PeerConnector :: ParseTCPPortFromMessage(const Message & msg, uint16 & retPort) const
{
   // Called for each of the system's peers in turn:  refusing all but ours is
   // what keeps the ClientConnector from connecting (or failing over) elsewhere.
   zg::ZGPeerID peerID;
   if ((msg.FindFlat(ZG_DISCOVERY_NAME_PEERID, peerID).IsError())||(peerID != _peerID)) return B_DATA_NOT_FOUND;
   return zg::ClientConnector::ParseTCPPortFromMessage(msg, retPort);
}

void MuscleNodeSource :: PeerConnector :: ConnectionStatusUpdated(const MessageRef &)
{
   _owner.connectionStatusUpdated();
}

void MuscleNodeSource :: PeerConnector :: MessageReceivedFromNetwork(const MessageRef & msg)
{
   if (msg()) _owner.messageReceived(*msg());
}

// ---------------------------------------------------------------------------

MuscleNodeSource :: MuscleNodeSource(ICallbackMechanism & callbackMechanism,
                                     const String & signaturePattern,
                                     const String & systemName,
                                     const zg::ZGPeerID & peerID)
   : _signaturePattern(signaturePattern)
   , _systemName(systemName)
   , _connector(&callbackMechanism, *this, peerID)
{
   // empty
}

MuscleNodeSource :: ~MuscleNodeSource()
{
   _connector.Stop();
}

status_t MuscleNodeSource :: start()
{
   return _connector.Start(_signaturePattern, _systemName);
}

bool MuscleNodeSource :: isConnected() const
{
   return _connector.IsConnected();
}

juce::String MuscleNodeSource :: getConnectedHostDescription() const
{
   const MessageRef peerInfo = _connector.GetConnectedPeerInfo();
   if (peerInfo() == NULL) return juce::String();

   // "src" is where the peer's discovery reply came from; "port" is its TCP port.
   IPAddressAndPort iap(peerInfo()->GetString(ZG_DISCOVERY_NAME_SOURCE), 0, false);
   uint16 port = 0;
   if (peerInfo()->FindInt16("port", port).IsOK()) iap.SetPort(port);
   return zgb::toJuce(iap.ToString(port > 0));
}

String MuscleNodeSource :: childrenSubscriptionPath(const String & nodePath)
{
   // Node names are literal, but a subscription path's clauses are wildcard
   // patterns -- so escape each name, lest eg a "*" or "," in one widen the match.
   String ret;
   uint32 startAt = 0;
   while(startAt < nodePath.Length())
   {
      const int slash = nodePath.IndexOf('/', startAt);
      const uint32 endAt = (slash >= 0) ? (uint32) slash : nodePath.Length();
      ret += '/';
      ret += EscapeRegexTokens(nodePath.Substring(startAt, endAt));
      startAt = endAt+1;
   }
   return ret + "/*";
}

String MuscleNodeSource :: toTreePath(const String & absolutePath)
{
   return absolutePath.WithoutPrefix("/");
}

status_t MuscleNodeSource :: sendSubscribe(const Queue<String> & nodePaths, bool isNewConnection)
{
   MessageRef msg = GetMessageFromPool(PR_COMMAND_SETPARAMETERS);
   MRETURN_OOM_ON_NULL(msg());

   // By default a MUSCLE session's wildcards skip its own session node -- but
   // we're showing the whole tree, and our own session is part of it.
   if (isNewConnection) MRETURN_ON_ERROR(msg()->AddBool(PR_NAME_REFLECT_TO_SELF, true));

   for (uint32 i=0; i<nodePaths.GetNumItems(); i++) MRETURN_ON_ERROR(msg()->AddBool(childrenSubscriptionPath(nodePaths[i]).WithPrepend("SUBSCRIBE:"), true));
   return _connector.send(msg);
}

status_t MuscleNodeSource :: subscribeToChildrenOf(const String & nodePath)
{
   MRETURN_ON_ERROR(_subscribedPaths.PutWithDefault(nodePath));
   if (isConnected() == false) return B_NO_ERROR;   // connectionStatusUpdated() will send it once we are

   Queue<String> q;
   MRETURN_ON_ERROR(q.AddTail(nodePath));
   return sendSubscribe(q, false);
}

void MuscleNodeSource :: unsubscribeFromChildrenOf(const String & nodePath)
{
   if ((_subscribedPaths.Remove(nodePath).IsError())||(isConnected() == false)) return;

   // PR_NAME_KEYS are themselves patterns (matched against the parameter names),
   // so escape the subscription path to remove exactly this one and no other.
   MessageRef msg = GetMessageFromPool(PR_COMMAND_REMOVEPARAMETERS);
   if ((msg())&&(msg()->AddString(PR_NAME_KEYS, EscapeRegexTokens(childrenSubscriptionPath(nodePath)).WithPrepend("SUBSCRIBE:")).IsOK())) (void) _connector.send(msg);
}

status_t MuscleNodeSource :: requestSearch(const String & namePattern, const String & tag)
{
   // As with ZGNodeSource, one query string per depth -- two levels more here,
   // since our paths also include the host and session nodes.
   MessageRef msg = GetMessageFromPool(PR_COMMAND_GETDATATREES);
   MRETURN_OOM_ON_NULL(msg());

   String prefix("/");
   for (uint32 i=0; i<zgb::kMaxSearchDepth+2; i++)
   {
      MRETURN_ON_ERROR(msg()->AddString(PR_NAME_KEYS, prefix + namePattern));
      prefix += "*/";
   }

   // Depth 0:  just each matching node's own payload, not the subtree beneath it.
   MRETURN_ON_ERROR(msg()->AddInt32(PR_NAME_MAXDEPTH, 0));
   MRETURN_ON_ERROR(msg()->AddString(PR_NAME_TREE_REQUEST_ID, tag));
   return _connector.send(msg);
}

status_t MuscleNodeSource :: ping(const String & tag)
{
   MessageRef msg = GetMessageFromPool(PR_COMMAND_PING);
   MRETURN_OOM_ON_NULL(msg());
   MRETURN_ON_ERROR(msg()->AddString(kPingTagField, tag));
   return _connector.send(msg);
}

void MuscleNodeSource :: connectionStatusUpdated()
{
   if (isConnected())
   {
      // A fresh server session knows nothing of what we had open last time.
      Queue<String> paths;
      for (ConstHashtableIterator<String, Void> iter(_subscribedPaths); iter.HasData(); iter++) (void) paths.AddTail(iter.GetKey());

      status_t ret;
      if (sendSubscribe(paths, true).IsError(ret)) LogTime(MUSCLE_LOG_ERROR, "Couldn't set up our server session [%s]\n", ret());
   }

   if (_listener) _listener->connectionStateChanged();
}

void MuscleNodeSource :: messageReceived(const Message & msg)
{
   if (_listener == nullptr) return;

   switch(msg.what)
   {
      case PR_RESULT_DATAITEMS:
      {
         const String * path;
         for (uint32 i=0; msg.FindString(PR_NAME_REMOVED_DATAITEMS, i, &path).IsOK(); i++)
         {
            const String treePath = toTreePath(*path);
            if (treePath.HasChars()) _listener->nodeUpdated(treePath, ConstMessageRef());
         }

         // Each added/updated node is a Message field named by its path.  (A ZG
         // server also puts op-tag bookkeeping in here, but never as Messages.)
         MessageRef payload;
         for (MessageFieldNameIterator iter = msg.GetFieldNameIterator(B_MESSAGE_TYPE); iter.HasData(); iter++)
         {
            const String treePath = toTreePath(iter.GetFieldName());
            if (treePath.IsEmpty()) continue;
            for (uint32 i=0; msg.FindMessage(iter.GetFieldName(), i, payload).IsOK(); i++) _listener->nodeUpdated(treePath, ConstMessageRef(payload));
         }

         _listener->callbackBatchEnded();
      }
      break;

      case PR_RESULT_DATATREES:
      {
         // Same layout as ZG's:  one field per matched node, holding the
         // SaveNodeTreeToMessage() form (payload under PR_NAME_NODEDATA).
         std::vector<std::pair<String, ConstMessageRef> > results;

         MessageRef nodeRef;
         for (MessageFieldNameIterator iter = msg.GetFieldNameIterator(B_MESSAGE_TYPE); iter.HasData(); iter++)
         {
            if (msg.FindMessage(iter.GetFieldName(), 0, nodeRef).IsError()) continue;

            MessageRef payloadRef;
            (void) nodeRef()->FindMessage(PR_NAME_NODEDATA, payloadRef);

            results.push_back(std::make_pair(toTreePath(iter.GetFieldName()), ConstMessageRef(payloadRef)));
         }

         _listener->searchResultsReturned(msg.GetString(PR_NAME_TREE_REQUEST_ID), results);
      }
      break;

      case PR_RESULT_PONG:
      {
         const String * tag;
         if (msg.FindString(kPingTagField, &tag).IsOK()) _listener->pongReceived(*tag);
      }
      break;

      default:
         // eg PR_RESULT_ERRORUNIMPLEMENTED; nothing we asked for
      break;
   }
}
