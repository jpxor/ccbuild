/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2025 Josh Simonot
 */

// NDEBUG needs to be defined to test null cases which are normally
// caught via asserts during development
#ifndef NDEBUG
#define NDEBUG
#endif

#define CC_STRINGS_IMPLEMENTATION
#include "cc_strings.h"

#include "cc_test.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <string.h>

int test_ccstrchr(void) {
    ccstr sv = CCSTR_LITERAL("test string");

    // test found char
    CHKEQ_INT(ccstrchr(sv, 'r'), 7);

    // test not found char
    CHKEQ_INT(ccstrchr(sv, 'z'), -1);

    return 0;
}

int test_ccstrstr(void) {
    ccstr sv = CCSTR_LITERAL("one two three");

    // test found
    CHKEQ_INT(ccstrstr(sv, CCSTR_LITERAL("one")), 0);
    CHKEQ_INT(ccstrstr(sv, CCSTR_LITERAL("two")), 4);
    CHKEQ_INT(ccstrstr(sv, CCSTR_LITERAL("three")), 8);

    // test not found
    CHKEQ_INT(ccstrstr(sv, CCSTR_LITERAL("four")), -1);

    return 0;
}

int test_ccstr_offset(void) {
    ccstr sv = CCSTR_LITERAL("one two three");
    ccstr offsv;

    // offset 0
    offsv = ccstr_offset(sv, 0);
    CHKEQ_PTR(offsv.cptr, sv.cptr);
    CHKEQ_INT(offsv.len, sv.len);

    // offset mid
    offsv = ccstr_offset(sv, 4);
    CHKEQ_STR(offsv.cptr, "two three");
    CHKEQ_INT(offsv.len, sv.len-4);

    // offset max
    offsv = ccstr_offset(sv, 13);
    CHKEQ_STR(offsv.cptr, "");
    CHKEQ_INT(offsv.len, 0);

    // offset over max
    offsv = ccstr_offset(sv, 20);
    CHKEQ_STR(offsv.cptr, "");
    CHKEQ_INT(offsv.len, 0);

    return 0;
}

int test_ccstr_slice(void) {
    ccstr sv = CCSTR_LITERAL("one two three");
    ccstr offsv;

    // slice 0
    offsv = ccstr_slice(sv, 0, 0);
    CHKEQ_PTR(offsv.cptr, sv.cptr);
    CHKEQ_INT(offsv.len, 0);

    // slice start
    offsv = ccstr_slice(sv, 0, 3);
    CHKEQ_STRN(offsv.cptr, offsv.len, "one");
    CHKEQ_INT(offsv.len, 3);

    // slice mid
    offsv = ccstr_slice(sv, 4, 3);
    CHKEQ_STRN(offsv.cptr, offsv.len, "two");
    CHKEQ_INT(offsv.len, 3);

    // slice end
    offsv = ccstr_slice(sv, 8, 5);
    CHKEQ_STRN(offsv.cptr, offsv.len, "three");
    CHKEQ_INT(offsv.len, 5);

    // slice over end
    offsv = ccstr_slice(sv, 8, 10);
    CHKEQ_STRN(offsv.cptr, offsv.len, "three");
    CHKEQ_INT(offsv.len, 5);

    return 0;
}

int test_ccsv_strcount(void) {
    ccstr sv = CCSTR_LITERAL("one two three two three three");

    CHKEQ_INT(ccsv_strcount(sv, CCSTR_LITERAL("zero")), 0);
    CHKEQ_INT(ccsv_strcount(sv, CCSTR_LITERAL("one")), 1);
    CHKEQ_INT(ccsv_strcount(sv, CCSTR_LITERAL("two")), 2);
    CHKEQ_INT(ccsv_strcount(sv, CCSTR_LITERAL("three")), 3);
    CHKEQ_INT(ccsv_strcount(sv, sv), 1);

    return 0;
}

int test_ccstr_replace(void) {
    ccstr target = CCSTR_LITERAL("target");
    
    char buf[100] = {0};
    ccstr s = CCSTR(buf, 0, 100);

    // replace whole string
    ccstrcpy(&s, target);
    CHKEQ_INT(ccstr_replace(&s, target, CCSTR_LITERAL("margot")), strlen("margot"));
    CHKEQ_STR(s.cptr, "margot");
    CHKEQ_INT(s.len, strlen("margot"));

    // replace with smaller
    ccstrcpy(&s, target);
    CHKEQ_INT(ccstr_replace(&s, target, CCSTR_LITERAL("got")), strlen("got"));
    CHKEQ_STR(s.cptr, "got");
    CHKEQ_INT(s.len, strlen("got"));

    // replace with longer
    ccstrcpy(&s, target);
    CHKEQ_INT(ccstr_replace(&s, target, CCSTR_LITERAL("longer target")), strlen("longer target"));
    CHKEQ_STR(s.cptr, "longer target");
    CHKEQ_INT(s.len, strlen("longer target"));

    // replace partial string - shorter
    ccstrcpy(&s, CCSTR_LITERAL("target at start"));
    CHKEQ_INT(ccstr_replace(&s, target, CCSTR_LITERAL("ready")), strlen("ready at start"));
    CHKEQ_STR(s.cptr, "ready at start");
    CHKEQ_INT(s.len, strlen("ready at start"));

    // replace partial string - longer
    ccstrcpy(&s, CCSTR_LITERAL("target at start"));
    CHKEQ_INT(ccstr_replace(&s, target, CCSTR_LITERAL("runners lined up")), strlen("runners lined up at start"));
    CHKEQ_STR(s.cptr, "runners lined up at start");
    CHKEQ_INT(s.len, strlen("runners lined up at start"));

    // replace partial string - shorter
    ccstrcpy(&s, CCSTR_LITERAL("middle target is here"));
    CHKEQ_INT(ccstr_replace(&s, target, CCSTR_LITERAL("word")), strlen("middle word is here")); 
    CHKEQ_STR(s.cptr, "middle word is here");
    CHKEQ_INT(s.len, strlen("middle word is here"));

    // replace partial string - longer
    ccstrcpy(&s, CCSTR_LITERAL("middle target is here"));
    CHKEQ_INT(ccstr_replace(&s, target, CCSTR_LITERAL("looong word")), strlen("middle looong word is here"));
    CHKEQ_STR(s.cptr, "middle looong word is here");
    CHKEQ_INT(s.len, strlen("middle looong word is here"));

    // replace partial string - shorter
    ccstrcpy(&s, CCSTR_LITERAL("now the end target"));
    CHKEQ_INT(ccstr_replace(&s, target, CCSTR_LITERAL("...")), strlen("now the end ..."));
    CHKEQ_STR(s.cptr, "now the end ...");
    CHKEQ_INT(s.len, strlen("now the end ..."));

    // replace partial string - longer
    ccstrcpy(&s, CCSTR_LITERAL("now the end target"));
    CHKEQ_INT(ccstr_replace(&s, target, CCSTR_LITERAL("is here")), strlen("now the end is here"));
    CHKEQ_STR(s.cptr, "now the end is here");
    CHKEQ_INT(s.len, strlen("now the end is here"));

    // replace multiple
    ccstrcpy(&s, CCSTR_LITERAL("target target target"));
    CHKEQ_INT(ccstr_replace(&s, target, CCSTR_LITERAL("ho")), strlen("ho ho ho"));
    CHKEQ_STR(s.cptr, "ho ho ho");
    CHKEQ_INT(s.len, strlen("ho ho ho"));

    return 0;
}

int test_ccstr_append_join(void) {
    char buf[100] = "zero";
    ccstr s = CCSTR(buf, strlen(buf), 100);

    ccstr sep = CCSTR_LITERAL(" ");
    ccstr svlist[] = {
        CCSTR_LITERAL("one"),
        CCSTR_LITERAL("two"),
        CCSTR_LITERAL("three"),
    };

    // null dest
    CHKEQ_INT(ccstr_append_join(NULL, sep, svlist, 1), strlen("one"));

    // null list
    CHKEQ_INT(ccstr_append_join(&s, sep, NULL, 0), strlen("zero"));
    CHKEQ_STR(s.cptr, "zero");
    CHKEQ_INT(s.len, strlen("zero"));

    // simple append
    CHKEQ_INT(ccstr_append_join(&s, CCSTR_LITERAL(""), svlist, 1), strlen("zeroone"));
    CHKEQ_STR(s.cptr, "zeroone");
    CHKEQ_INT(s.len, 7);

    // append with separator
    ccstrcpy(&s, CCSTR_LITERAL("zero"));
    CHKEQ_INT(ccstr_append_join(&s, sep, svlist, 1), strlen("zero one"));
    CHKEQ_STR(s.cptr, "zero one");

    // append join 2
    ccstrcpy(&s, CCSTR_LITERAL("zero"));
    CHKEQ_INT(ccstr_append_join(&s, sep, svlist, 2), strlen("zero one two"));
    CHKEQ_STR(s.cptr, "zero one two");

    // append join 3
    ccstrcpy(&s, CCSTR_LITERAL("zero"));
    CHKEQ_INT(ccstr_append_join(&s, sep, svlist, 3), strlen("zero one two three"));
    CHKEQ_STR(s.cptr, "zero one two three");

    return 0;
}

int test_ccstrcasecmp(void) {
    ccstr a1 = CCSTR_LITERAL("one");
    ccstr b1 = CCSTR_LITERAL("ONE");
    CHKEQ_INT(ccstrcasecmp(a1, b1), 0);

    ccstr a2 = CCSTR_LITERAL("tWo");
    ccstr b2 = CCSTR_LITERAL("TwO");
    CHKEQ_INT(ccstrcasecmp(a2, b2), 0);

    ccstr a3 = CCSTR_LITERAL("ThReE");
    ccstr b3 = CCSTR_LITERAL("tHrEeE");
    CHKEQ_INT(ccstrcasecmp(a3, b3), -1);

    ccstr a4 = CCSTR_LITERAL("fourr");
    ccstr b4 = CCSTR_LITERAL("four");
    CHKEQ_INT(ccstrcasecmp(a4, b4),  1);

    ccstr a5 = CCSTR_LITERAL("five");
    ccstr b5 = CCSTR_LITERAL("5");
    CHKNOT_EQ_INT(ccstrcasecmp(a5, b5),  0);

    return 0;
}

int test_ccstr_rawlen(void) {
    ccstr s = CCSTR_VIEW("test string", 11);
    CHKEQ_STR(s.cptr, "test string");
    CHKEQ_INT(s.len, 11);
    return 0;
}

int test_ccsv_charcount(void) {
    ccstr sv = CCSTR_LITERAL("0011001100");
    CHKEQ_INT(ccsv_charcount(sv, '0'), 6);
    return 0;
}

int test_ccstrncmp(void) {
    ccstr a = CCSTR_LITERAL("123456789");
    ccstr b = CCSTR_LITERAL("123456543");
    CHKEQ_INT(ccstrncmp(a, b, 0), 0);
    CHKEQ_INT(ccstrncmp(a, b, 1), 0);
    CHKEQ_INT(ccstrncmp(a, b, 6), 0);
    CHKEQ_INT(ccstrncmp(a, b, 7), '7' - '5');
    CHKEQ_INT(ccstrncmp(a, b, 9), '7' - '5');
    CHKEQ_INT(ccstrncmp(a, b, 20),'7' - '5');
    CHKEQ_INT(ccstrncmp(b, a, 7), '5' - '7');
    return 0;
}

int test_ccsv_tokenize(void) {
    ccstr sv = CCSTR_LITERAL("one two three");
    ccstr token;

    token = ccstr_tokenize(&sv, ' ');
    CHKEQ_STRN(token.cptr, token.len, "one");
    CHKEQ_STRN(sv.cptr, sv.len, "two three");

    token = ccstr_tokenize(&sv, ' ');
    CHKEQ_STRN(token.cptr, token.len, "two");
    CHKEQ_STRN(sv.cptr, sv.len, "three");

    token = ccstr_tokenize(&sv, ' ');
    CHKEQ_STRN(token.cptr, token.len, "three");
    CHKEQ_STRN(sv.cptr, sv.len, "");
    CHKEQ_INT(sv.len, 0);

    token = ccstr_tokenize(&sv, ' ');
    CHKEQ_STRN(token.cptr, token.len, "");
    CHKEQ_STRN(sv.cptr, sv.len, "");
    CHKEQ_INT(token.len, 0);
    CHKEQ_INT(sv.len, 0);

    return 0;
}

int test_ccstrcpy(void) {
    char tmpbuf[20];
    ccstr dst = CCSTR(tmpbuf, 0, sizeof tmpbuf);

    { // from literal into empty
        memset(tmpbuf, 0, sizeof tmpbuf);
        ccstrcpy(&dst, CCSTR_LITERAL("test"));
        CHKEQ_STRN(dst.cptr, 4, "test");
        CHKEQ_INT(dst.len, 4);
    }{// from view (not null terminated)
        memset(tmpbuf, 0, sizeof tmpbuf);
        ccstrcpy(&dst, CCSTR_VIEW("testy", 4));
        CHKEQ_STR(dst.cptr, "test");
        CHKEQ_INT(dst.len, 4);
    }{// into stack (use existing mem, null terminate)
        ccstr dst = CCSTR(tmpbuf, 0, sizeof tmpbuf);
        ccstrcpy(&dst, CCSTR_LITERAL("test"));
        CHKEQ_STR(dst.cptr, "test");
        CHKEQ_INT(dst.len, 4);
    }
    return 0;
}

int test_ccstr_tostr(void) {
    char tmpbuf[6];
    memset(tmpbuf, 'X', sizeof tmpbuf);

    int reqsize = ccstr_tostr(NULL, 0, CCSTR_LITERAL("test"));
    CHKEQ_INT(reqsize, 4);

    ccstr_tostr(tmpbuf, sizeof tmpbuf, CCSTR_LITERAL("test"));
    CHKEQ_STR(tmpbuf, "test");
    CHKEQ_INT(strlen(tmpbuf), 4);
    CHKEQ_INT(tmpbuf[4], 0); // must be null terminated

    memset(tmpbuf, 'X', sizeof tmpbuf);
    reqsize = ccstr_tostr(tmpbuf, sizeof tmpbuf, CCSTR_LITERAL("test-test"));
    CHKEQ_INT(reqsize, 9);
    CHKEQ_STR(tmpbuf, "test-");
    CHKEQ_INT(strlen(tmpbuf), 5); // sizeof tmpbuf - 1

    return 0;
}

int test_ccstr_strip_whitespace(void) {
    char tmpbuf[] = "  \t  test test  \t\t ";
    ccstr src = CCSTR(tmpbuf, strlen(tmpbuf), sizeof tmpbuf);
    ccstr_strip_whitespace(&src);

    CHKEQ_STRN(src.cptr, src.len, "test test");
    CHKEQ_INT(src.len, 9);
    CHKEQ_PTR(src.cptr, src.cptr);

    ccstr literal = CCSTR_LITERAL("  \t  test test  \t\t ");
    src = CCSTR(tmpbuf, 0, sizeof tmpbuf);

    ccstrcpy(&src, literal);
    ccstr view = ccstr_view(src);
    ccstr_strip_whitespace(&view);

    // ensure the original ccstr is not modified
    CHKEQ_STR(literal.cptr, src.cptr);

    CHKEQ_STRN(view.cptr, view.len, "test test");
    CHKEQ_INT(view.len, 9);

    return 0;
}

int main(void) {
    int err = 0;
    
    err |= test_ccstrchr();
    err |= test_ccstrstr();
    err |= test_ccstr_offset();
    err |= test_ccstr_slice();
    err |= test_ccsv_strcount();
    err |= test_ccstr_replace();
    err |= test_ccstr_append_join();
    err |= test_ccstrcasecmp();
    err |= test_ccstr_rawlen();
    err |= test_ccsv_charcount();
    err |= test_ccstrncmp();
    err |= test_ccsv_tokenize();
    err |= test_ccstrcpy();
    err |= test_ccstr_tostr();
    err |= test_ccstr_strip_whitespace();

    printf("[%s] test cc_strings\n", err? "FAILED": "PASSED");
    return 0;
}
