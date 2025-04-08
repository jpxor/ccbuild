
#ifndef _CCTEST_H
#define _CCTEST_H

#include <stdio.h>

typedef struct cctest_ctx {
    const char *test;
    const char *file;
    const int line;
} cctest_ctx_t;


#define TEST(test_func) \
do { if (test_func()) nerrs++; } while (0)


static int cctest_int_equals(struct cctest_ctx ctx, char *gotstr, int got, char *expstr, int exp) {
    if (got != exp) {
        printf("%s:%d\n", ctx.file, ctx.line);
        printf(" > %s\n", ctx.test);
        printf(" > GOT %s = %d\n", gotstr, got);
        printf(" > EXP %s = %d\n", expstr, exp);
        return -1;
    }
    return 0;
}
#define CHKEQ_INT(got, exp) \
if (cctest_int_equals((cctest_ctx_t){.file=__FILE__,.test=__func__,.line=__LINE__}, #got, got, #exp, exp) == -1) {return -1;}

static int cctest_int_not_equals(struct cctest_ctx ctx, char *gotstr, int got, char *expstr, int exp) {
    if (got == exp) {
        printf("%s:%d\n", ctx.file, ctx.line);
        printf(" > %s\n", ctx.test);
        printf(" > GOT %s = %d\n", gotstr, got);
        printf(" > EXP %s = %d\n", expstr, exp);
        return -1;
    }
    return 0;
}
#define CHKNOT_EQ_INT(got, exp) \
if (cctest_int_not_equals((cctest_ctx_t){.file=__FILE__,.test=__func__,.line=__LINE__}, #got, got, #exp, exp) == -1) {return -1;}


static int cctest_str_equals(struct cctest_ctx ctx, char *gotstr, char *got, char *expstr, char *exp) {
    if (strcmp(got, exp) != 0) {
        printf("%s:%d\n", ctx.file, ctx.line);
        printf(" > %s\n", ctx.test);
        printf(" > GOT %s = %s\n", gotstr, got);
        printf(" > EXP %s = %s\n", expstr, exp);
        return -1;
    }
    return 0;
}
#define CHKEQ_STR(got, exp) \
if (cctest_str_equals((cctest_ctx_t){.file=__FILE__,.test=__func__,.line=__LINE__}, #got, got, #exp, exp) == -1) {return -1;}

static int cctest_ptr_equals(struct cctest_ctx ctx, char *gotstr, void *got, char *expstr, void *exp) {
    if (got != exp) {
        printf("%s:%d\n", ctx.file, ctx.line);
        printf(" > %s\n", ctx.test);
        printf(" > GOT %s = 0x%p\n", gotstr, got);
        printf(" > EXP %s = 0x%p\n", expstr, exp);
        return -1;
    }
    return 0;
}
#define CHKEQ_PTR(got, exp) \
if (cctest_ptr_equals((cctest_ctx_t){.file=__FILE__,.test=__func__,.line=__LINE__}, #got, got, #exp, exp) == -1) {return -1;}


#endif // _CCTEST_H
