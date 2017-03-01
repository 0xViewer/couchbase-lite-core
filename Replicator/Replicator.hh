//
//  Replicator.hh
//  LiteCore
//
//  Created by Jens Alfke on 2/13/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#pragma once
#include "ReplActor.hh"
#include "BLIPConnection.hh"
#include "FleeceCpp.hh"

using namespace litecore::blip;

namespace litecore { namespace repl {

    class Pusher;
    class Puller;
    class DBActor;


    /** The top-level replicator object, which runs the BLIP connection.
        Pull and push operations are run by subidiary Puller and Pusher objects.
        The database will only be accessed by the DBAgent object. */
    class Replicator : public ReplActor, ConnectionDelegate {
    public:
        /** Constructor for a client connection; will open the Connection itself. */
        Replicator(C4Database*, websocket::Provider&, const websocket::Address&, Options);

        /** Constructor for an incoming (server) connection. */
        Replicator(C4Database*, websocket::WebSocket*, const websocket::Address&);

#if DEBUG
        websocket::WebSocket* webSocket() const {return connection()->webSocket();}
        alloc_slice checkpointID() const        {return _checkpointID;}
#endif

        void updatePushCheckpoint(C4SequenceNumber);
        void updatePullCheckpoint(std::string);
        
        /** Called by the Pusher and Puller when they finish their duties. */
        void taskComplete(bool isPush) {
            enqueue(&Replicator::_taskComplete, isPush);
        }

    protected:
        // BLIP ConnectionDelegate API:
        virtual void onConnect() override
                                    {enqueue(&Replicator::_onConnect);}
        virtual void onClose(bool normalClose, int status, fleece::slice reason) override
                                    {enqueue(&Replicator::_onClose,
                                             normalClose, status, alloc_slice(reason));}
        virtual void onRequestReceived(blip::MessageIn *msg) override
                                    {enqueue(&Replicator::_onRequestReceived,
                                             Retained<blip::MessageIn>(msg));}

        virtual void _updateCheckpoint() override;
        virtual void afterEvent() override;
    private:
        struct Checkpoint {
            C4SequenceNumber localSeq {0};
            std::string remoteSeq;
        };

        Replicator(C4Database*, const websocket::Address&, Options, Connection*);
        void _onConnect();
        void _onError(int errcode, fleece::alloc_slice reason);
        void _onClose(bool normalClose, int status, fleece::alloc_slice reason);
        void _onRequestReceived(Retained<blip::MessageIn> msg);

        void getCheckpoints();
        static Checkpoint decodeCheckpoint(slice json);
        static fleece::alloc_slice encodeCheckpoint(const Checkpoint&);
        void startReplicating();

        void _taskComplete(bool isPush);

        const websocket::Address _remoteAddress;
        Retained<DBActor> _dbActor;
        Retained<Pusher> _pusher;
        Retained<Puller> _puller;
        alloc_slice _checkpointID;
        Checkpoint _checkpoint;
        bool _checkpointChanged {false};
        bool _pushing, _pulling;
    };

} }
