/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2025 Josh Simonot
 */
#ifndef _CC_STRINGS_H
#define _CC_STRINGS_H

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#define CCSTR_FLAG_LITERAL 0x01

/**
 * A string view into memory with a length and capacity.
 * The capacity represents the known length of modifiable
 * memory. A capacity of 0 means the string is read-only.
 * 
 * These string view are not guaranteed to be null-terminated.
 *
 * @param cptr Pointer to the character buffer containing the string data
 * @param cap Total allocated capacity of the string buffer
 * @param len Current length of the string (number of characters)
 * @param flags Optional flags for additional string metadata or behavior
 */
typedef struct {
    char *cptr;
    size_t cap;
    uint32_t len;
    uint32_t flags;
} ccstr;

/**
 * Creates a read-only view from a string literal.
 * 
 * @param lit A string literal from which to create a ccstr
 * @return A ccstr with the string literal's data, calculated length, and zero capacity
 */
#define CCSTR_LITERAL(lit) (ccstr){ \
    .cptr = (char *)lit,            \
    .len = sizeof(lit)-1,           \
    .cap = 0,                       \
    .flags = CCSTR_FLAG_LITERAL,    \
}

/**
 * Creates a read-only view of a character buffer with a specified length.
 * 
 * This function creates a ccstr with zero capacity, indicating a read-only view.
 * The view is not required to include a null terminator and assumes the buffer is
 * at least as long as the specified length.
 * 
 * @param buf Pointer to the character buffer containing the string data
 * @param len Length of the string data (number of characters)
 * @return A ccstr representing a read-only view of the string
 */
static inline ccstr CCSTR_VIEW(const char *buf, int len) {
    return (ccstr){
        .cptr = (char *)buf,
        .len = len,
        .cap = 0,
    };
}

static inline ccstr ccstr_view(const ccstr s) {
    return CCSTR_VIEW(s.cptr, s.len);
}

/**
 * Creates a mutable view of a character buffer with a specified length and capacity.
 * 
 * This function creates a ccstr with a mutable view of the provided buffer,
 * allowing modification up to the specified capacity.
 * 
 * @param buf Pointer to the character buffer containing the string data
 * @param len Current length of the string (number of characters)
 * @param cap Total allocated capacity of the string buffer
 * @return A ccstr representing a mutable view of the string
 */
static inline ccstr CCSTR(char *buf, int len, size_t cap) {
    return (ccstr){
        .cptr = buf,
        .len = len,
        .cap = cap,
    };
}

static inline ccstr CCSTR_EMPTY() {
    return (ccstr){0};
}

/**
 * Creates a new ccstr view with an offset into the original view.
 * 
 * This function creates a new view of the string, offset forward by the specified amount,
 * adjusting the pointer, length, and capacity accordingly. If the offset
 * exceeds the string's length, an empty string is returned.
 * 
 * @param sv The original ccstr from which to create an offset view
 * @param offset The number of characters to offset from the start of the string
 * @return A new ccstr view starting at the offset position
 * @note Offsets can only move forward and must stay within the known string length.
 *       Moving backwards would be unsafe because there is no way to know where the
 *       string starts. Moving beyond the known length is unsafe because we don't know
 *       if the memory is innitialized.
 */
static inline ccstr ccstr_offset(const ccstr sv, uint32_t offset) {
    // reduce offset to ensure it fits within the view,
    // will result in an empty string if offset is greater than
    // the length
    offset = (offset > sv.len)? sv.len : offset;
    return (ccstr) {
        .cptr = sv.cptr + offset,
        .len = sv.len - offset,
        .cap = sv.cap - offset,
    };
}

/**
 * Creates a new ccstr view with a specified offset and length.
 * 
 * This function creates a new view of the string,offset forward by the specified amount
 * and limiting the length. If the requested length exceeds the remaining string length,
 * the slice is truncated to the available length.
 * 
 * @param sv The original ccstr from which to create a slice
 * @param offset The number of characters to offset from the start of the string
 * @param len The maximum length of the slice
 * @return A new ccstr view starting at the offset position with the specified length
 * @note The slice is constrained by the original string's length and capacity
 */
static inline ccstr ccstr_slice(const ccstr sv, uint32_t offset, uint32_t  len) {
    ccstr ret = ccstr_offset(sv, offset);
    ret.len = (ret.len > len)? len : ret.len;
    return ret;
}

void ccstr_strip_whitespace(ccstr *s);

int ccstrcpy(ccstr *s, const ccstr src);

int ccstr_tostr(char *buf, size_t bufsize, const ccstr src);

ccstr ccstr_tokenize(ccstr *src, char delim);

int ccsv_strcount(ccstr sv, ccstr pattern);

int ccsv_charcount(ccstr sv, char c);

int ccstrstr(ccstr sv, ccstr pattern);

int ccstrchr(ccstr sv, char c);

int ccstrcasecmp(ccstr a, ccstr b);

int ccstrncmp(ccstr str0, ccstr str1, size_t n);

int ccstr_replace(ccstr *s, ccstr search, ccstr replace);

int ccstr_append_join(ccstr *dest, ccstr sep, ccstr *srcs, int count);

static int ccstr_append(ccstr *dest, ccstr sv) {
    ccstr sep = CCSTR_LITERAL("");
    return ccstr_append_join(dest, sep, &sv, 1);
}

size_t ccstr_realloc(ccstr *s, size_t newcap);

#endif // _CC_STRINGS_H

// #define CC_STRINGS_IMPLEMENTATION
#ifdef CC_STRINGS_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

// READONLY functions

/**
 * Finds the first occurrence of a character in a string view.
 * 
 * @param sv String view to search in
 * @param c Character to search for
 * @return Index of the first occurrence of the character in the string view,
 *         or -1 if the character is not found
 * 
 * @note Performs a linear search through the string view
 * @note Returns -1 if the character is not found or if the string view is empty
 */
int ccstrchr(ccstr sv, char c) {
    for (uint32_t  i = 0; i < sv.len; ++i) {
        if (sv.cptr[i] == c) {
            return i;
        }
    }
    return -1;
}

/**
 * Finds the first occurrence of a pattern within a string view.
 * 
 * @param sv String view to search in
 * @param pattern String view to search for
 * @return Index of the first occurrence of the pattern in the string view,
 *         or -1 if the pattern is not found or invalid
 * 
 * @note Returns -1 if pattern or string view is empty, or pattern is longer than string view
 * @note Uses standard C library strstr() for substring search
 */
int ccstrstr(ccstr sv, ccstr pattern) {
    if (pattern.len == 0 || sv.len == 0 || pattern.len > sv.len) {
        return -1;
    }
    const char* found = strstr(sv.cptr, pattern.cptr);
    if (!found) {
        return -1;
    }
    return (int)(found - sv.cptr);
}

/**
 * Compares two string views case-insensitively.
 * 
 * @param a First string view to compare
 * @param b Second string view to compare
 * @return Negative value if a is lexicographically less than b, 
 *         zero if a is equal to b, 
 *         positive value if a is lexicographically greater than b
 * 
 * @note Comparison is done character-by-character after converting to lowercase
 * @note If strings are identical up to the length of the shorter string, 
 *       the longer string is considered greater
 */
int ccstrcasecmp(ccstr a, ccstr b) {
    int minlen = (a.len < b.len)? a.len : b.len;
    for (int i = 0; i < minlen; ++i) {
        char c1 = tolower((unsigned char)a.cptr[i]);
        char c2 = tolower((unsigned char)b.cptr[i]);
        if (c1 != c2) {
            return c1 - c2;
        }
    }
    return a.len - b.len;
}

/**
 * Counts the number of occurrences of a specific string pattern in a string view.
 * 
 * @param sv String view to search
 * @param pattern String pattern to count
 * @return Number of times the pattern appears in the string view
 * 
 * @note Iterates through the string view and counts non-overlapping occurrences of the pattern
 * @note Returns 0 if the pattern is empty, the string view is empty, or the pattern is longer than the string view
 */
int ccsv_strcount(ccstr sv, ccstr pattern) {
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
        sv = ccstr_offset(sv, offset + pattern.len);
    }
    return count;
}

/**
 * Counts the number of occurrences of a specific character in a string view.
 * 
 * @param sv String view to search
 * @param c Character to count
 * @return Number of times the character appears in the string view
 * 
 * @note Iterates through the entire string view and counts exact character matches
 */
int ccsv_charcount(ccstr sv, char c) {
    int count = 0;
    for (size_t i = 0; i < sv.len; ++i) {
        if (sv.cptr[i] == c) {
            ++count;
        }
    }
    return count;
}

/**
 * Compares two string views up to a specified number of characters.
 * 
 * @param str0 First string view to compare
 * @param str1 Second string view to compare
 * @param n Maximum number of characters to compare
 * @return Difference between the first differing characters, or 0 if strings are equal up to n characters
 * 
 * @note Compares characters from both string views up to the minimum of n, str0.len, and str1.len
 * @note Returns positive value if str0 is lexicographically greater, negative if str1 is greater
 * @note If one string is shorter than n, comparison stops at the end of the shorter string
 */
int ccstrncmp(ccstr str0, ccstr str1, size_t n) {
    size_t i;
    for (i = 0; i < n && i < str0.len && i < str1.len; ++i) {
        if (str0.cptr[i] != str1.cptr[i]) {
            return (int)str0.cptr[i] - (int)str1.cptr[i];
        }
    }
    if (i < n) {
        if (i < str0.len) return 1;
        if (i < str1.len) return -1;
    }
    return 0;
}

// READ/WRITE functions

/**
 * Converts a ccstr to a null-terminated C string in a provided buffer.
 * 
 * @param buf Destination buffer to store the null-terminated string
 * @param bufsize Size of the destination buffer
 * @param src Source ccstr to convert
 * @return Total length of the source string
 * 
 * @note If buffer is NULL or has zero size, returns source string length
 * @note Ensures the destination buffer is null-terminated
 * @note Copies up to buffer size - 1 characters to leave room for null terminator
 */
int ccstr_tostr(char *buf, size_t bufsize, const ccstr src) {
    ccstr dst = CCSTR(buf, 0, bufsize);
    int len = ccstrcpy(&dst, src);
    if (buf && bufsize > 0) {
        int end = (len < bufsize) ? len : bufsize-1;
        buf[end] = 0;
    }
    return len;
}

/**
 * Removes leading and trailing whitespace from a string view in-place.
 * 
 * @param s Pointer to the string to be stripped of whitespace
 * 
 * @note Modifies the string view by adjusting its pointer, length, and capacity. The underlaying
 * memory is not modified.
 * @note Removes whitespace characters from both the beginning and end of the string
 */
void ccstr_strip_whitespace(ccstr *s) {
    int offset = 0;
    while (s->len > 0 && isspace(s->cptr[0])) {
        s->cptr++;
        offset++;
        s->len--;
    }
    while (s->len > 0 && isspace(s->cptr[s->len-1])) {
        s->len--;
    }
    if (s->cap > 0) {
        s->cap -= offset;
    }
}

size_t ccstr_realloc(ccstr *s, size_t newcap) {
    if (s->flags & CCSTR_FLAG_LITERAL) {
        char *newptr = malloc(newcap);
        if (newptr == NULL) {
            return s->cap;
        }
        newptr[s->len] = 0;
        s->cptr = newptr;
        s->cap = newcap;
        s->flags = 0;
        return newcap;
    }
    void *newptr = realloc(s->cptr, newcap);
    if (newptr == NULL) {
        return s->cap;
    }
    s->len = (s->len > newcap)? newcap : s->len;
    s->cptr = newptr;
    s->cap = newcap;
    return newcap;
}

/**
 * Copies a source string to a destination string, respecting the destination's capacity.
 * 
 * @param dst Pointer to the destination string to copy into
 * @param src Source string to copy from
 * @return Total length of the source string, even if not fully copied
 * 
 * @note If destination is NULL or has zero capacity, returns source string length
 * @note Copies up to the minimum of source length and destination capacity
 * @note Updates destination string length to reflect actual copied characters
 */
int ccstrcpy(ccstr *dst, const ccstr src) {
    if (dst == NULL || dst->cap == 0) {
        return src.len;
    }
    size_t count = (src.len > dst->cap)? dst->cap : src.len;
    for (size_t i = 0; i < count; i++) {
        dst->cptr[i] = src.cptr[i];
    }
    dst->len = count;
    return src.len;
}

/**
 * Replaces all occurrences of a search string with a replacement string.
 * 
 * This function searches for all instances of 'search' within the string 's'
 * and replaces them with the 'replace' string. It handles in-place replacement
 * by shifting characters to accommodate different length search and replace strings.
 * 
 * @param s Pointer to the string to be modified
 * @param search String view to search for within the original string
 * @param replace String view to replace the search string with
 * @return Total length of the modified string after replacements
 * 
 * @note If the destination buffer is too small, returns the required final length
 * @note Modifies the original string in-place
 * @note Null-terminates the string if space allows
 */
int ccstr_replace(ccstr *s, ccstr search, ccstr replace) {
    assert(s != NULL);

    // count occurrences
    int count = ccsv_strcount(*s, search);
    if (count == 0) {
        return s->len;
    }
    // calc final size and expand if needed
    int shiftlen = replace.len - search.len;
    int final_len = s->len + (count * shiftlen);

    if (s->cap < final_len) {
        return final_len;
    }

    // replace, memmove to account for search-vs-replace len diff
    ccstr itr = ccstr_view(*s);
    for (int i = 0; i < count; ++i) {
        int offset = ccstrstr(itr, search);
        assert(offset >= 0);

        itr = ccstr_offset(itr, offset);
        if (shiftlen != 0) {
            char *dest = itr.cptr + replace.len;
            char *src = itr.cptr + search.len;
            memmove(dest, src, itr.len - search.len);
            itr.len += shiftlen;
        }
        memcpy(itr.cptr, replace.cptr, replace.len);
        itr = ccstr_offset(itr, replace.len);
    }
    s->len = final_len;
    if (s->len < s->cap) {
        s->cptr[s->len] = 0;
    }
    return final_len;
}

/**
 * Appends and joins multiple string views with a separator.
 * 
 * This function calculates the total length needed to join the given string views,
 * optionally prepending an existing destination string. It handles:
 * - Calculating the final length of the joined string
 * - Appending a separator between each source string
 * - Optionally starting with an existing destination string
 * 
 * @param dest Destination string to append to (can be NULL or empty)
 * @param sep Separator string to insert between source strings
 * @param srcs Array of source string views to join
 * @param count Number of source strings in the array
 * @return Total length of the resulting joined string
 * 
 * @note If dest is NULL or has zero capacity, only the length is calculated
 * @note The destination string is null-terminated if space allows
 */
int ccstr_append_join(ccstr *dest, ccstr sep, ccstr *srcs, int count) {
    // calculate final length
    int final_len = 0;
    for (int i = 0; i < count; i++) {
        final_len += srcs[i].len + sep.len;
    }
    if (dest == NULL || dest->len == 0) {
        final_len -= sep.len;
    } else {
        final_len += dest->len;
    }
    if (dest == NULL || dest->cap == 0) {
        return final_len;
    }
    // appends
    for (int i = 0; i < count; ++i) {
        memcpy(dest->cptr + dest->len, sep.cptr, sep.len);
        dest->len += sep.len;

        memcpy(dest->cptr + dest->len, srcs[i].cptr, srcs[i].len);
        dest->len += srcs[i].len;
    }
    if (dest->len < dest->cap) {
        dest->cptr[dest->len] = 0;
    }
    assert(dest->len == final_len);
    return final_len;
}

/**
 * Tokenizes a string view by splitting on a delimiter character.
 * Returns the next token and advances the view past it.
 * 
 * This function:
 * 1. Skips any leading delimiter characters
 * 2. Extracts the next token up to the next delimiter
 * 3. Advances the view past the token and trailing delimiters
 * 4. Returns the token as a new string view
 *
 * The original view is modified to point to the remaining string.
 * Multiple calls can be used to iterate through all tokens.
 *
 * @param view Pointer to the string view to tokenize. Will be modified.
 * @param delim The delimiter character to split on
 * @return A string view containing the next token (empty if no more tokens)
 *
 * Example usage:
 *   ccstr str = CCSTR_LITERAL("a,b,c");
 *   ccstr view = str;
 *   ccstr token;
 *   while (token = ccstr_tokenize(&view, ','), token.len > 0) {
 *      // process token
 *   }
 */
ccstr ccstr_tokenize(ccstr *view, char delim) {
    ccstr token = {0};
    
    // skip leading delimiters
    while (view->len > 0 && view->cptr[0] == delim) {
        view->cptr++;
        view->len--;
    }
    
    // find end of token
    uint32_t pos = 0;
    while (pos < view->len && view->cptr[pos] != delim) {
        pos++;
    }
    
    // create token view
    token.cptr = view->cptr;
    token.len = pos;
    
    // advance original view
    view->cptr += pos;
    view->len -= pos;

    // skip leading delimiters (again)
    while (view->len > 0 && view->cptr[0] == delim) {
        view->cptr++;
        view->len--;
    }
    
    return token;
}

#endif // CC_STRINGS_IMPLEMENTATION
