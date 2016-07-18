//
//  RevisionStore.hh
//  CBForest
//
//  Created by Jens Alfke on 7/8/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#ifndef RevisionStore_hh
#define RevisionStore_hh
#include "Database.hh"
#include "Revision.hh"
#include "VersionVector.hh"

namespace cbforest {

    class Revision;


    /** Manages storage of version-vectored document revisions in a Database. */
    class RevisionStore {
    public:

        RevisionStore(Database *db);
        virtual ~RevisionStore() { }

        //////// GETTING REVISIONS:

        Revision::Ref get(slice docID,
                      KeyStore::contentOptions = KeyStore::kDefaultContent) const;
        Revision::Ref get(slice docID, slice revID,
                      KeyStore::contentOptions = KeyStore::kDefaultContent) const;

        /** Make sure a Revision has a body (if it was originally loaded as meta-only) */
        void readBody(Revision&);

        /** How does this revision compare to what's in the database?
            @return  kNewer if it should be added, kSame if it's present, kOlder if it's obsolete. */
        versionOrder checkRevision(slice docID, slice revID);

        //////// ADDING REVISIONS:

        /** Creates a new revision.
            @param docID  The document ID
            @param parentVersion  The version vector of the revision being modified
            @param body  The body and related flags
            @param t  Transaction to write the revision to
            @return  New Revision, or null if there's a conflict */
        Revision::Ref create(slice docID,
                             const VersionVector &parentVersion,
                             Revision::BodyParams body,
                             Transaction &t);

        /** Inserts a revision, probably from a peer. */
        versionOrder insert(Revision&, Transaction&);

        virtual Revision::Ref resolveConflict(std::vector<Revision*> conflicting,
                                              Revision::BodyParams body,
                                              Transaction &t);

        //////// DOCUMENT KEYS:

        /** The document key to use for a non-current Revision. */
        static alloc_slice keyForNonCurrentRevision(slice docID, struct version vers);

        /** The start of the key range for non-current Revisions with the given docID
            (and author, if non-null.) */
        static alloc_slice startKeyFor(slice docID, peerID author =slice::null);
        
        /** The non-inclusive end of the key range for non-current Revisions with the given docID
            (and author, if non-null.) */
        static alloc_slice endKeyFor(slice docID, peerID author =slice::null);

        static slice docIDFromKey(slice key);

    protected:
        virtual void willReplaceCurrentRevision(Revision &curRev, const Revision &incomingRev, Transaction &t);
        virtual bool shouldKeepAncestor(const Revision &rev, const Revision &child);

        Revision::Ref resolveConflict(std::vector<Revision*> conflicting,
                                      slice keepingRevID,
                                      Revision::BodyParams body,
                                      Transaction &t);
        void replaceCurrent(Revision &newRev, Revision *current, Transaction &t);
        bool deleteNonCurrent(slice docID, slice revID, Transaction &t);
        Revision::Ref getNonCurrent(slice docID, slice revID, KeyStore::contentOptions) const;
        void deleteAncestors(Revision&, Transaction&);
        DocEnumerator enumerateRevisions(slice docID, slice author = slice::null);

    protected:
        Database * const _db;
        KeyStore &_store;
        KeyStore &_nonCurrentStore;
    };

}

#endif /* RevisionStore_hh */