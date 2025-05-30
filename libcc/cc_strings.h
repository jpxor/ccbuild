/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2025 Josh Simonot
 */
#ifndef _CC_STRINGS_H
#define _CC_STRINGS_H

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// ccstr rules:
// - expands as needed via realloc
// - cannot abort on enomem
// - truncated flag set on enomem
// - allows static init for convenience
// - max len: 2^32
// - returns ccstr* to allow chaining

#define CCSTR_FLAG_HEAP    1
#define CCSTR_FLAG_LITERAL 2
#define CCSTR_FLAG_STACK   4
#define CCSTR_FLAG_TRUNCATED 8

// ccstr owns the raw cstr memory
// except when static flag is set,
// its cstr must be null terminated
typedef struct {
    char *cstr;
    size_t cap;
    uint32_t len;
    uint32_t flags;
} ccstr;

// ccstrview does not own its memory
// and cannot be resized, and might
// not be null terminated
typedef struct {
    char *cstr;
    uint32_t len;
    uint32_t flags;
} ccstrview;

#define CCSTR_LITERAL(s) (ccstr){  \
    .cstr = s,                    \
    .len = sizeof(s)-1,           \
    .cap = 0        ,             \
    .flags = CCSTR_FLAG_LITERAL,   \
}

#define CCSTR_STACK(array, size) (ccstr){  \
    .cstr = array,               \
    .len = 0,                    \
    .cap = size,                 \
    .flags = CCSTR_FLAG_STACK,   \
}

#define CCSTRVIEW_STATIC(s) (ccstrview){ \
    .cstr = s,                    \
    .len = sizeof(s)-1,           \
    .flags = CCSTR_FLAG_LITERAL,   \
}

static inline
ccstrview ccsv(const ccstr *s) {
    return (ccstrview) {
        .cstr = s->cstr,
        .len = s->len,
    };
}

static inline
ccstrview ccsv_raw(const char *raw) {
    return (ccstrview) {
        .cstr = (char*)raw,
        .len = strlen(raw),
        .flags = CCSTR_FLAG_LITERAL,
    };
}

static inline int ccstr_is_truncated(ccstr *s) {
    return s->flags & CCSTR_FLAG_TRUNCATED;
}

// constructors
ccstr ccstr_empty(int capacity);
ccstr ccstrdup(ccstr src);
ccstr ccstr_rawlen(const char *raw, int len);

ccstr* ccstrcpy_rawlen(ccstr *s, const char *raw, uint32_t len);
ccstr* ccstr_replace(ccstr *s, ccstrview search, ccstrview replace);
ccstr* ccstr_append_join(ccstr *dest, ccstrview sep, ccstrview *srcs, int count);

ccstrview ccsv_offset(ccstrview sv, uint32_t  offset);
ccstrview ccsv_slice(ccstrview sv, uint32_t  offset, uint32_t  len);
ccstrview ccsv_tokenize(ccstrview *sv, char delim);

int ccstrcasecmp(ccstrview a, ccstrview b);
int ccsv_strcount(ccstrview sv, ccstrview pattern);
int ccsv_charcount(ccstrview sv, char c);
int ccstrstr(ccstrview sv, ccstrview pattern);
int ccstrchr(ccstrview sv, char c);
int ccstrncmp(ccstrview str0, ccstrview str1, size_t n);

size_t ccstr_realloc(ccstr *s, size_t newcap);

static inline
ccstr * ccstr_replace_raw(ccstr *s, const char *search, const char *replace) {
    return ccstr_replace(s, ccsv_raw((char *)search), ccsv_raw((char *)replace));
}

static inline
ccstr * ccstrcpy_raw(ccstr *s, const char *raw) {
    return ccstrcpy_rawlen(s, raw, strlen(raw));
}

static inline
ccstr * ccstrcpy(ccstr *s, const ccstr src) {
    return ccstrcpy_rawlen(s, src.cstr, src.len);
}

static inline
ccstr * ccstr_append(ccstr *dest, ccstrview sv) {
    ccstrview sep = CCSTRVIEW_STATIC("");
    return ccstr_append_join(dest, sep, &sv, 1);
}

static inline
void ccstr_free(ccstr *s) {
    if (s->flags & CCSTR_FLAG_LITERAL) {
        return;
    }
    free(s->cstr);
    s->cstr = NULL;
    s->cap = s->len = 0;
}

#endif // _CC_STRINGS_H

// #define CC_STRINGS_IMPLEMENTATION
#ifdef CC_STRINGS_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

// constructors

ccstr ccstr_empty(int cap) {
    ccstr str = {
        .cstr = calloc(1, cap),
        .cap = cap,
        .len = 0,
    };
    if (str.cstr == NULL) {
        str.cap = 0;
    }
    return str;
}

ccstr ccstr_rawlen(const char *raw, int len) {
    return *ccstrcpy_rawlen(&(ccstr){0}, raw, len);
}

ccstr ccstrdup(ccstr src) {
    return ccstr_rawlen(src.cstr, src.len);
}

// READONLY functions

int ccstrchr(ccstrview sv, char c) {
    for (uint32_t  i = 0; i < sv.len; ++i) {
        if (sv.cstr[i] == c) {
            return i;
        }
    }
    return -1;
}

int ccstrstr(ccstrview sv, ccstrview pattern) {
    if (pattern.len == 0 || sv.len == 0 || pattern.len > sv.len) {
        return -1;
    }
    const char* found = strstr(sv.cstr, pattern.cstr);
    if (!found) {
        return -1;
    }
    return (int)(found - sv.cstr);
}

int ccstrcasecmp(ccstrview a, ccstrview b) {
    int minlen = (a.len < b.len)? a.len : b.len;
    for (int i = 0; i < minlen; ++i) {
        char c1 = tolower((unsigned char)a.cstr[i]);
        char c2 = tolower((unsigned char)b.cstr[i]);
        if (c1 != c2) {
            return c1 - c2;
        }
    }
    return a.len - b.len;
}

ccstrview ccsv_offset(ccstrview sv, uint32_t  offset) {
    offset = (sv.len > offset)? offset : sv.len;
    return (ccstrview) {
        .cstr = sv.cstr + offset,
        .len = sv.len - offset,
    };
}

ccstrview ccsv_slice(ccstrview sv, uint32_t  offset, uint32_t  len) {
    offset = (sv.len > offset)? offset : sv.len;
    return (ccstrview) {
        .cstr = sv.cstr + offset,
        .len = (len <= sv.len - offset)? len : sv.len - offset,
    };
}

int ccsv_strcount(ccstrview sv, ccstrview pattern) {
    if (pattern.len == 0 || sv.len == 0 || pattern.len > sv.len) {
        return 0;
    }
    int count = 0;
    for (;;) {
        int offset = ccstrstr(sv, pattern);
        if (offset == -1) {
            break;
        }
        ++count;
        sv = ccsv_offset(sv, offset + pattern.len);
    }
    return count;
}

int ccsv_charcount(ccstrview sv, char c) {
    int count = 0;
    for (size_t i = 0; i < sv.len; ++i) {
        if (sv.cstr[i] == c) {
            ++count;
        }
    }
    return count;
}

int ccstrncmp(ccstrview str0, ccstrview str1, size_t n) {
    size_t i;
    for (i = 0; i < n && i < str0.len && i < str1.len; ++i) {
        if (str0.cstr[i] != str1.cstr[i]) {
            return (int)str0.cstr[i] - (int)str1.cstr[i];
        }
    }
    if (i < n) {
        if (i < str0.len) return 1;
        if (i < str1.len) return -1;
    }
    return 0;
}

// READ/WRITE functions

size_t ccstr_realloc(ccstr *s, size_t newcap) {
    if (newcap <= s->len) {
        s->len = newcap - 1;

    } else if (s->flags & CCSTR_FLAG_STACK) {
        // can't realloc stack allocated ccstr
        s->flags = CCSTR_FLAG_TRUNCATED;
        return s->cap;
    }
    // literal innitialzed ccstr needs to be
    // replaced with heap allocation
    if (s->flags & CCSTR_FLAG_LITERAL) {
        s->flags = s->flags & ~CCSTR_FLAG_LITERAL;
        s->flags |= CCSTR_FLAG_HEAP;
        char *newptr = malloc(newcap);
        if (newptr == NULL) {
            *s = (ccstr) {0};
            s->flags = CCSTR_FLAG_TRUNCATED;
            return 0;
        }
        memcpy(newptr, s->cstr, s->len);
        s->cstr = newptr;
        s->cap = newcap;
        s->cstr[s->len] = 0;
        return s->cap;
    }
    void *newptr = realloc(s->cstr, newcap);
    if (newptr == NULL) {
        return s->cap;
    }
    s->cstr = newptr;
    s->cap = newcap;
    s->cstr[s->len] = 0;
    return newcap;
}

ccstr* ccstrcpy_rawlen(ccstr *s, const char *raw, uint32_t len) {
    assert(s != NULL);

    if (s == NULL) {
        return NULL;
    }
    size_t mincap = (size_t)len + 1;
    if (s->cap < mincap) {
        ccstr_realloc(s, mincap);
        if (s->cap < mincap) {
            s->flags |= CCSTR_FLAG_TRUNCATED;
            return s;
        }
    }
    uint32_t i;
    for (i = 0; i < s->cap && i < len; ++i) {
        s->cstr[i] = raw[i];
    }
    s->cstr[i] = 0;
    s->len = i;

    if (s->len < len) {
        s->flags |= CCSTR_FLAG_TRUNCATED;
    }
    return s;
}

ccstr * ccstr_replace(ccstr *s, ccstrview search, ccstrview replace) {
    assert(s != NULL);

    if (s == NULL) {
        return NULL;
    }
    // check for cases where there is nothing to do
    if (s->cstr == NULL || s->len == 0 || search.cstr == NULL || search.len == 0) {
        return s;
    }
    // count occurrences
    int count = ccsv_strcount(ccsv(s), search);
    if (count == 0) {
        return s;
    }
    // calc final size and expand if needed
    int shiftlen = replace.len - search.len;
    int final_len = s->len + (count * shiftlen);

    if (replace.len > search.len) {
        ccstr_realloc(s, final_len+1);
    }
    // replace, memmove to account for search-vs-replace len diff
    ccstrview itr = ccsv(s);
    for (int i = 0; i < count; ++i) {
        int offset = ccstrstr(itr, search);
        assert(offset >= 0);

        itr = ccsv_offset(itr, offset);
        if (shiftlen != 0) {
            char *dest = itr.cstr + replace.len;
            char *src = itr.cstr + search.len;
            memmove(dest, src, itr.len - search.len);
            itr.len += shiftlen;
        }
        memcpy(itr.cstr, replace.cstr, replace.len);
        itr = ccsv_offset(itr, replace.len);
    }
    s->len = final_len;
    s->cstr[final_len] = 0;
    return s;
}

ccstr * ccstr_append_join(ccstr *dest, ccstrview sep, ccstrview *srcs, int count) {
    assert(dest != NULL);
    assert(srcs != NULL);

    if (dest == NULL || srcs == NULL || count == 0) {
        return dest;
    }
    // cap check
    int additional_length = 0;
    for (int i = 0; i < count; ++i) {
        additional_length += srcs[i].len + sep.len;
    }
    size_t mincap = dest->len + additional_length + 1;
    if (dest->cap < mincap) {
        ccstr_realloc(dest, mincap);
        if (dest->cap < mincap) {
            dest->flags |= CCSTR_FLAG_TRUNCATED;
            return dest;
        }
    }
    // append
    for (int i = 0; i < count; ++i) {
        memcpy(dest->cstr + dest->len, sep.cstr, sep.len);
        dest->len += sep.len;

        memcpy(dest->cstr + dest->len, srcs[i].cstr, srcs[i].len);
        dest->len += srcs[i].len;

        dest->flags |= (srcs[i].flags & CCSTR_FLAG_TRUNCATED);
    }
    dest->cstr[dest->len] = 0;
    return dest;
}

ccstrview ccsv_tokenize(ccstrview *sv, char delim) {
    ccstrview token = {0};
    
    // Skip leading delimiters
    while (sv->len > 0 && sv->cstr[0] == delim) {
        sv->cstr++;
        sv->len--;
    }
    
    // Find end of token
    uint32_t pos = 0;
    while (pos < sv->len && sv->cstr[pos] != delim) {
        pos++;
    }
    
    // Create token view
    token.cstr = sv->cstr;
    token.len = pos;
    
    // Advance original view
    sv->cstr += pos;
    sv->len -= pos;

    // Skip leading delimiters (again)
    while (sv->len > 0 && sv->cstr[0] == delim) {
        sv->cstr++;
        sv->len--;
    }
    
    return token;
}

#endif // CC_STRINGS_IMPLEMENTATION
