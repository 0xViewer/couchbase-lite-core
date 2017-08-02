//
//  UnicodeCollator_Apple.cc
//  LiteCore
//
//  Created by Jens Alfke on 7/27/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#include "UnicodeCollator.hh"
#include "Error.hh"
#include "Logging.hh"
#include "StringUtil.hh"
#include "PlatformCompat.hh"
#include "SQLiteCpp/SQLiteCpp.h"
#include <sqlite3.h>

#if __APPLE__ // For Apple platforms; see UnicodeCollator_std.cc for cross-platform implementation

#include <CoreFoundation/CFString.h>


namespace litecore {

    using namespace std;
    using namespace fleece;


    // Stores CF collation parameters for fast lookup; callback context points to this
    class CFCollationContext : public CollationContext {
    public:
        CFLocaleRef localeRef {nullptr};
        CFStringCompareFlags flags;

        CFCollationContext(const Collation &coll)
        :CollationContext(coll)
        ,flags(kCFCompareNonliteral | kCFCompareWidthInsensitive)
        {
            Assert(coll.unicodeAware);
            if (!coll.caseSensitive)
                flags |= kCFCompareCaseInsensitive;
            
            if (!coll.diacriticSensitive)
                flags |= kCFCompareDiacriticInsensitive;

            slice localeName = coll.localeName ?: "en_US"_sl;
            flags |= kCFCompareLocalized;
            auto localeStr = CFStringCreateWithBytesNoCopy(nullptr,
                                       (const UInt8*)localeName.buf, localeName.size,
                                       kCFStringEncodingASCII, false, kCFAllocatorNull);
            if (localeStr) {
                localeRef = CFLocaleCreate(NULL, localeStr);
                CFRelease(localeStr);
            }
            if (!localeRef)
                Warn("Unknown locale name '%.*s'", SPLAT(coll.localeName));
        }

        ~CFCollationContext() {
            if (localeRef)
                CFRelease(localeRef);
        }
    };


    /** Full Unicode-savvy string comparison. */
    static inline int compareStringsUnicode(int len1, const void * chars1,
                                            int len2, const void * chars2,
                                            const CFCollationContext &ctx)
    {
        // OPT: Consider using UCCompareText(), from <CarbonCore/UnicodeUtilities.h>, instead?

        auto cfstr1 = CFStringCreateWithBytesNoCopy(nullptr, (const UInt8*)chars1, len1,
                                                    kCFStringEncodingUTF8, false, kCFAllocatorNull);
        if (_usuallyFalse(!cfstr1))
            return -1;
        auto cfstr2 = CFStringCreateWithBytesNoCopy(nullptr, (const UInt8*)chars2, len2,
                                                    kCFStringEncodingUTF8, false, kCFAllocatorNull);
        if (_usuallyFalse(!cfstr2)) {
            CFRelease(cfstr1);
            return 1;
        }


        int result = CFStringCompareWithOptionsAndLocale(cfstr1, cfstr2,
                                                         CFRange{0, CFStringGetLength(cfstr1)},
                                                         ctx.flags, ctx.localeRef);
        CFRelease(cfstr1);
        CFRelease(cfstr2);
        return result;
    }


    static int collateUnicodeCallback(void *context,
                                      int len1, const void * chars1,
                                      int len2, const void * chars2)
    {
        auto &coll = *(CFCollationContext*)context;
        if (coll.canCompareASCII) {
            int result = CompareASCII(len1, (const uint8_t*)chars1, len2, (const uint8_t*)chars2,
                                      coll.caseSensitive);
            if (result != kCompareASCIIGaveUp)
                return result;
        }
        return compareStringsUnicode(len1, chars1, len2, chars2, coll);
    }


    int CompareUTF8(slice str1, slice str2, const Collation &coll) {
        CFCollationContext ctx(coll);
        return collateUnicodeCallback(&ctx, (int)str1.size, str1.buf,
                                            (int)str2.size, str2.buf);
    }


    unique_ptr<CollationContext> RegisterSQLiteUnicodeCollation(sqlite3* dbHandle,
                                                                const Collation &coll) {
        unique_ptr<CollationContext> context(new CFCollationContext(coll));
        int rc = sqlite3_create_collation(dbHandle,
                                          coll.sqliteName().c_str(),
                                          SQLITE_UTF8,
                                          (void*)context.get(),
                                          collateUnicodeCallback);
        if (rc != SQLITE_OK)
            context.reset();
        return context;
    }
}

#endif // __APPLE__
