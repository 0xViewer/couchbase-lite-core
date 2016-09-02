//
//  c4BlobStoreTest.cc
//  LiteCore
//
//  Created by Jens Alfke on 9/1/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#include "c4Test.hh"
#include "c4BlobStore.h"
#include "C4Private.h"

using namespace std;


class BlobStoreTest {
public:
    
    BlobStoreTest() {
        C4Error error;
        store = c4blob_openStore(c4str(kTestDir "cbl_blob_test/"), kC4DB_Create, nullptr, &error);
        CHECK(store != nullptr);

        memset(bogusKey.bytes, 0x55, sizeof(bogusKey.bytes));
    }

    ~BlobStoreTest() {
        C4Error error;
        CHECK(c4blob_deleteStore(store, &error));
    }

    C4BlobStore *store {nullptr};

    C4BlobKey bogusKey;
};


TEST_CASE("parse blob keys", "[blob][C]") {
    C4BlobKey key1;
    memset(key1.bytes, 0x55, sizeof(key1.bytes));
    C4SliceResult str = c4blob_keyToString(key1);
    CHECK(string((char*)str.buf, str.size) == "sha1-VVVVVVVVVVVVVVVVVVVVVVVVVVU=");

    C4BlobKey key2;
    CHECK(c4blob_keyFromString(C4Slice{str.buf, str.size}, &key2));
    CHECK(memcmp(key1.bytes, key2.bytes, sizeof(key1.bytes)) == 0);
}


TEST_CASE("parse invalid blob keys", "[blob][C][!throws]") {
    c4log_warnOnErrors(false);
    C4BlobKey key2;
    CHECK_FALSE(c4blob_keyFromString(C4STR(""), &key2));
    CHECK_FALSE(c4blob_keyFromString(C4STR("rot13-xxxx"), &key2));
    CHECK_FALSE(c4blob_keyFromString(C4STR("sha1-"), &key2));
    CHECK_FALSE(c4blob_keyFromString(C4STR("sha1-VVVVVVVVVVVVVVVVVVVVVV"), &key2));
    CHECK_FALSE(c4blob_keyFromString(C4STR("sha1-VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVU"), &key2));
    c4log_warnOnErrors(true);
}


TEST_CASE_METHOD(BlobStoreTest, "missing blobs", "[blob][C]") {
    CHECK(c4blob_getSize(store, bogusKey) == -1);

    C4Error error;
    C4SliceResult data = c4blob_getContents(store, bogusKey, &error);
    CHECK(data.buf == nullptr);
    CHECK(data.size == 0);
    CHECK(error.code == kC4ErrorNotFound);
}


TEST_CASE_METHOD(BlobStoreTest, "create blobs", "[blob][C]") {
    C4Slice blobToStore = C4STR("This is a blob to store in the store!");

    // Add blob to the store:
    C4BlobKey key;
    C4Error error;
    CHECK(c4blob_create(store, blobToStore, &key, &error));

    auto str = c4blob_keyToString(key);
    CHECK(string((char*)str.buf, str.size) == "sha1-QneWo5IYIQ0ZrbCG0hXPGC6jy7E=");

    // Read it back and compare
    CHECK(c4blob_getSize(store, key) == blobToStore.size);
    
    auto gotBlob = c4blob_getContents(store, key, &error);
    CHECK(gotBlob.buf != nullptr);
    CHECK(gotBlob.size == blobToStore.size);
    CHECK(memcmp(gotBlob.buf, blobToStore.buf, gotBlob.size) == 0);

    // Try storing it again
    C4BlobKey key2;
    CHECK(c4blob_create(store, blobToStore, &key2, &error));
    CHECK(memcmp(&key2, &key, sizeof(key2)) == 0);
}


TEST_CASE_METHOD(BlobStoreTest, "read blob with stream", "[blob][C]") {
    string blob = "This is a blob to store in the store!";

    // Add blob to the store:
    C4BlobKey key;
    C4Error error;
    CHECK(c4blob_create(store, {blob.data(), blob.size()}, &key, &error));

    CHECK( c4blob_openStream(store, bogusKey, &error) == nullptr);

    auto stream = c4blob_openStream(store, key, &error);
    CHECK(stream);

    // Read it back, 6 bytes at a time:
    string readBack;
    char buf[6];
    size_t bytesRead;
    do {
        bytesRead = c4stream_read(stream, buf, sizeof(buf), &error);
        readBack.append(buf, bytesRead);
    } while (bytesRead == sizeof(buf));
    CHECK(error.code == 0);
    CHECK(readBack == blob);

    // Try seeking:
    CHECK(c4stream_seek(stream, 10, &error));
    CHECK(c4stream_read(stream, buf, 4, &error) == 4);
    CHECK(memcmp(buf, "blob", 4) == 0);

    c4stream_close(stream);
    c4stream_close(nullptr); // this should be a no-op, not a crash
}
