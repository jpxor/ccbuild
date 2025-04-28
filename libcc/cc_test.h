
#ifndef CCTEST_H
#define CCTEST_H

#include <stdio.h>
#include <string.h>

struct cctest_ctx {
    const char *test;
    const char *file;
    const int line;
};

#define CCTEST_MAKE_CTX() (struct cctest_ctx){.file=__FILE__, .test=__func__, .line=__LINE__}

static inline void cctest_print_context(struct cctest_ctx ctx) {
    printf("%s:%d\n", ctx.file, ctx.line);
    printf(" > %s\n", ctx.test);
}

#define TEST(test_func) \
do { if (test_func()) nerrs++; } while (0)

static inline int cctest_int_equals(struct cctest_ctx ctx, char *gotstr, int got, char *expstr, int exp) {
    if (got != exp) {
        cctest_print_context(ctx);
        printf(" > GOT %s = %d\n", gotstr, got);
        printf(" > EXP %s = %d\n", expstr, exp);
        return -1;
    }
    return 0;
}

#define CHKEQ_INT(got, exp) \
if (cctest_int_equals(CCTEST_MAKE_CTX(), #got, got, #exp, exp) == -1) {return -1;}

static inline int cctest_int_not_equals(struct cctest_ctx ctx, char *gotstr, int got, char *expstr, int exp) {
    if (got == exp) {
        cctest_print_context(ctx);
        printf(" > GOT %s = %d\n", gotstr, got);
        printf(" > EXP %s != %d\n", expstr, exp);
        return -1;
    }
    return 0;
}

#define CHKNOT_EQ_INT(got, exp) \
if (cctest_int_not_equals(CCTEST_MAKE_CTX(), #got, got, #exp, exp) == -1) {return -1;}


static inline int cctest_str_equals(struct cctest_ctx ctx, char *gotstr, char *got, char *expstr, char *exp) {
    if (strcmp(got, exp) != 0) {
        cctest_print_context(ctx);
        printf(" > GOT %s = %s\n", gotstr, got);
        printf(" > EXP %s = %s\n", expstr, exp);
        return -1;
    }
    return 0;
}

#define CHKEQ_STR(got, exp) \
if (cctest_str_equals(CCTEST_MAKE_CTX(), #got, got, #exp, exp) == -1) {return -1;}

static inline int cctest_strn_equals(struct cctest_ctx ctx, char *gotstr, char *got, char *expstr, char *exp, int n) {
    if (strncmp(got, exp, n) != 0) {
        cctest_print_context(ctx);
        printf(" > GOT %s = %s\n", gotstr, got);
        printf(" > EXP %s = %s\n", expstr, exp);
        return -1;
    }
    return 0;
}

#define CHKEQ_STRN(got, N, exp) \
if (cctest_strn_equals(CCTEST_MAKE_CTX(), #got, got, #exp, exp, N) == -1) {return -1;}


static inline int cctest_ptr_equals(struct cctest_ctx ctx, char *gotstr, void *got, char *expstr, void *exp) {
    if (got != exp) {
        cctest_print_context(ctx);
        printf(" > GOT %s = 0x%p\n", gotstr, got);
        printf(" > EXP %s = 0x%p\n", expstr, exp);
        return -1;
    }
    return 0;
}

#define CHKEQ_PTR(got, exp) \
if (cctest_ptr_equals(CCTEST_MAKE_CTX(), #got, got, #exp, exp) == -1) {return -1;}

#endif // CCTEST_H
