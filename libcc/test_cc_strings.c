/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2025 Josh Simonot
 */

#ifndef NDEBUG
#error NDEBUG needs to be defined to test null cases which are normally caught via asserts
#endif

#define CC_STRINGS_IMPLEMENTATION
#include "cc_strings.h"

#include "cc_test.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <string.h>

int test_ccstrcpy_rawlen(void) {
    ccstr s = CCSTR_STATIC("test string");

    // strcpy into static string shall allocate space
    // and convert to heap allocated backing
    assert(s.flags & CCSTR_FLAG_STATIC);
    CHKEQ_PTR(ccstrcpy_raw(&s, "new string"), &s);
    CHKEQ_STR(s.cstr, "new string");
    CHKEQ_INT(s.len, 10);
    CHKEQ_INT(s.cap, 11);
    CHKEQ_INT(s.cstr[s.len], 0);
    CHKEQ_INT(s.flags & CCSTR_FLAG_STATIC, 0);
    
    // strcpy long string into ccstr shall realloc space
    CHKEQ_PTR(ccstrcpy_raw(&s, "new longer string"), &s);
    CHKEQ_STR(s.cstr, "new longer string");
    CHKEQ_INT(s.len, 17);
    CHKEQ_INT(s.cap, 18);
    CHKEQ_INT(s.cstr[s.len], 0);

    // strcpy into ccstr with enough capacity
    CHKEQ_PTR(ccstrcpy_raw(&s, "shorter"), &s);
    CHKEQ_STR(s.cstr, "shorter");
    CHKEQ_INT(s.len, 7);
    CHKEQ_INT(s.cap, 18);
    CHKEQ_INT(s.cstr[s.len], 0);

    // strcpy empty str into ccstr
    CHKEQ_PTR(ccstrcpy_raw(&s, ""), &s);
    CHKEQ_STR(s.cstr, "");
    CHKEQ_INT(s.len, 0);
    CHKEQ_INT(s.cap, 18);
    CHKEQ_INT(s.cstr[s.len], 0);

    // strcpy no null term str into ccstr
    char nonull[] = {'n','o','n','u','l','l'};
    CHKEQ_PTR(ccstrcpy_rawlen(&s, nonull, 6), &s);
    CHKEQ_STR(s.cstr, "nonull");
    CHKEQ_INT(s.len, 6);
    CHKEQ_INT(s.cstr[s.len], 0);

    // null ccstr
    CHKEQ_PTR(ccstrcpy_raw(NULL, "null"), NULL);

    return 0;
}

int test_ccstrchr(void) {
    ccstrview sv = ccsv_raw("test string");

    // test found char
    CHKEQ_INT(ccstrchr(sv, 'r'), 7);

    // test not found char
    CHKEQ_INT(ccstrchr(sv, 'z'), -1);

    return 0;
}

int test_ccstrstr(void) {
    ccstrview sv = ccsv_raw("one two three");

    // test found
    CHKEQ_INT(ccstrstr(sv, ccsv_raw("one")), 0);
    CHKEQ_INT(ccstrstr(sv, ccsv_raw("two")), 4);
    CHKEQ_INT(ccstrstr(sv, ccsv_raw("three")), 8);

    // test not found
    CHKEQ_INT(ccstrstr(sv, ccsv_raw("four")), -1);

    return 0;
}

int test_ccsv_offset(void) {
    ccstrview sv = ccsv_raw("one two three");
    ccstrview offsv;

    // offset 0
    offsv = ccsv_offset(sv, 0);
    CHKEQ_PTR(offsv.cstr, sv.cstr);
    CHKEQ_INT(offsv.len, sv.len);

    // offset mid
    offsv = ccsv_offset(sv, 4);
    CHKEQ_STR(offsv.cstr, "two three");
    CHKEQ_INT(offsv.len, sv.len-4);

    // offset max
    offsv = ccsv_offset(sv, 13);
    CHKEQ_STR(offsv.cstr, "");
    CHKEQ_INT(offsv.len, 0);

    // offset over max
    offsv = ccsv_offset(sv, 20);
    CHKEQ_STR(offsv.cstr, "");
    CHKEQ_INT(offsv.len, 0);

    return 0;
}

int test_ccsv_slice(void) {
    ccstrview sv = ccsv_raw("one two three");
    ccstrview offsv;

    // slice 0
    offsv = ccsv_slice(sv, 0, 0);
    CHKEQ_PTR(offsv.cstr, sv.cstr);
    CHKEQ_INT(offsv.len, 0);

    // slice start
    offsv = ccsv_slice(sv, 0, 3);
    CHKEQ_STRN(offsv.cstr, offsv.len, "one");
    CHKEQ_INT(offsv.len, 3);

    // slice mid
    offsv = ccsv_slice(sv, 4, 3);
    CHKEQ_STRN(offsv.cstr, offsv.len, "two");
    CHKEQ_INT(offsv.len, 3);

    // slice end
    offsv = ccsv_slice(sv, 8, 5);
    CHKEQ_STRN(offsv.cstr, offsv.len, "three");
    CHKEQ_INT(offsv.len, 5);

    // slice over end
    offsv = ccsv_slice(sv, 8, 10);
    CHKEQ_STRN(offsv.cstr, offsv.len, "three");
    CHKEQ_INT(offsv.len, 5);

    return 0;
}

int test_ccsv_strcount(void) {
    ccstrview sv = ccsv_raw("one two three two three three");

    CHKEQ_INT(ccsv_strcount(sv, ccsv_raw("zero")), 0);
    CHKEQ_INT(ccsv_strcount(sv, ccsv_raw("one")), 1);
    CHKEQ_INT(ccsv_strcount(sv, ccsv_raw("two")), 2);
    CHKEQ_INT(ccsv_strcount(sv, ccsv_raw("three")), 3);
    CHKEQ_INT(ccsv_strcount(sv, sv), 1);

    return 0;
}

int test_ccstr_replace(void) {
    ccstrview target = ccsv_raw("target");
    ccstr s = {0};

    // replace whole string
    ccstrcpy_raw(&s, "target");
    CHKEQ_PTR(ccstr_replace(&s, target, ccsv_raw("margot")), &s);
    CHKEQ_STR(s.cstr, "margot");
    CHKEQ_INT(s.len, strlen("margot"));

    // replace with smaller
    ccstrcpy_raw(&s, "target");
    CHKEQ_PTR(ccstr_replace(&s, target, ccsv_raw("got")), &s);
    CHKEQ_STR(s.cstr, "got");
    CHKEQ_INT(s.len, strlen("got"));

    // replace with longer
    ccstrcpy_raw(&s, "target");
    CHKEQ_PTR(ccstr_replace(&s, target, ccsv_raw("longer target")), &s);
    CHKEQ_STR(s.cstr, "longer target");
    CHKEQ_INT(s.len, strlen("longer target"));

    // replace partial string - shorter
    ccstrcpy_raw(&s, "target at start");
    CHKEQ_PTR(ccstr_replace(&s, target, ccsv_raw("ready")), &s);
    CHKEQ_STR(s.cstr, "ready at start");
    CHKEQ_INT(s.len, strlen("ready at start"));

    // replace partial string - longer
    ccstrcpy_raw(&s, "target at start");
    CHKEQ_PTR(ccstr_replace(&s, target, ccsv_raw("runners lined up")), &s);
    CHKEQ_STR(s.cstr, "runners lined up at start");
    CHKEQ_INT(s.len, strlen("runners lined up at start"));

    // replace partial string - shorter
    ccstrcpy_raw(&s, "middle target is here");
    CHKEQ_PTR(ccstr_replace(&s, target, ccsv_raw("word")), &s);
    CHKEQ_STR(s.cstr, "middle word is here");
    CHKEQ_INT(s.len, strlen("middle word is here"));

    // replace partial string - longer
    ccstrcpy_raw(&s, "middle target is here");
    CHKEQ_PTR(ccstr_replace(&s, target, ccsv_raw("looong word")), &s);
    CHKEQ_STR(s.cstr, "middle looong word is here");
    CHKEQ_INT(s.len, strlen("middle looong word is here"));

    // replace partial string - shorter
    ccstrcpy_raw(&s, "now the end target");
    CHKEQ_PTR(ccstr_replace(&s, target, ccsv_raw("...")), &s);
    CHKEQ_STR(s.cstr, "now the end ...");
    CHKEQ_INT(s.len, strlen("now the end ..."));

    // replace partial string - longer
    ccstrcpy_raw(&s, "now the end target");
    CHKEQ_PTR(ccstr_replace(&s, target, ccsv_raw("is here")), &s);
    CHKEQ_STR(s.cstr, "now the end is here");
    CHKEQ_INT(s.len, strlen("now the end is here"));

    // replace multiple
    ccstrcpy_raw(&s, "target target target");
    CHKEQ_PTR(ccstr_replace(&s, target, ccsv_raw("ho")), &s);
    CHKEQ_STR(s.cstr, "ho ho ho");
    CHKEQ_INT(s.len, strlen("ho ho ho"));

    return 0;
}

int test_ccstr_append_join(void) {
    ccstr s = CCSTR_STATIC("zero");
    ccstrview sep = ccsv_raw(" ");
    ccstrview svlist[] = {
        CCSTRVIEW_STATIC("one"),
        CCSTRVIEW_STATIC("two"),
        CCSTRVIEW_STATIC("three"),
    };

    // null dest
    CHKEQ_PTR(ccstr_append_join(NULL, sep, svlist, 1), NULL);

    // null list
    CHKEQ_PTR(ccstr_append_join(&s, sep, NULL, 0), &s);
    CHKEQ_STR(s.cstr, "zero");
    CHKEQ_INT(s.len, 4);

    // simple append
    CHKEQ_PTR(ccstr_append_join(&s, CCSTRVIEW_STATIC(""), svlist, 1), &s);
    CHKEQ_STR(s.cstr, "zeroone");
    CHKEQ_INT(s.len, 7);

    // append with separator
    ccstrcpy_raw(&s, "zero");
    CHKEQ_PTR(ccstr_append_join(&s, sep, svlist, 1), &s);
    CHKEQ_STR(s.cstr, "zero one");

    // append join 2
    ccstrcpy_raw(&s, "zero");
    CHKEQ_PTR(ccstr_append_join(&s, sep, svlist, 2), &s);
    CHKEQ_STR(s.cstr, "zero one two");

    // append join 3
    ccstrcpy_raw(&s, "zero");
    CHKEQ_PTR(ccstr_append_join(&s, sep, svlist, 3), &s);
    CHKEQ_STR(s.cstr, "zero one two three");
}

int main(void) {
    int err;
    
    err |= test_ccstrcpy_rawlen();
    err |= test_ccstrchr();
    err |= test_ccstrstr();
    err |= test_ccsv_offset();
    err |= test_ccsv_slice();
    err |= test_ccsv_strcount();
    err |= test_ccstr_replace();
    err |= test_ccstr_append_join();

    printf("[%s] test cc_strings\n", err? "FAILED": "PASSED");
    return 0;
}
