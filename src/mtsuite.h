#ifndef __MTSUITE_H_
#define __MTSUITE_H_
#include<string.h>
#include<errno.h>
#include<stdio.h>

// ------- CORE PROTOTYPES ----
#define MTSUITE_FORK            (1 << 0)
#define MTSUITE_SKIP            (1 << 1)
#define MTSUITE_ENABLED         (1 << 2)
#define MTSUITE_OFF_BY_DEFAULT  (1 << 3)
#define MTSUITE_FIRST_USER_FLAG (1 << 4)

typedef void (*TCallbackFn_t)(void*);
struct Testcase_t;

struct TestcaseSetup_t {
    void* (*setup)(const struct Testcase_t *);
    int (*cleanup)(const struct Testcase_t *, void *);
};

struct Testcase_t{
    const char *name;
    TCallbackFn_t callback;
    unsigned long flags;
    const struct TestcaseSetup_t *config;
    void *info;
};

#define MTSUITE_END_OF_TESTCASES {NULL, NULL, 0, NULL, NULL}

struct Testgroup_t {
    const char *prefix;
    struct Testcase_t *cases;
};

#define MTSUITE_END_OF_GROUPS { NULL, NULL }

struct TestlistAlias_t {
    const char *name;
    const char **tests;
};

#define MTSUITE_END_OF_ALIASES { NULL, NULL }

//
int mtsuite_cur_test_has_failed(void);
void mtsuite_set_test_failed(void);
void mtsuite_set_test_skipped(void);
int mtsuite_get_verbosity(void);
int mtsuite_set_flag(
    struct Testgroup_t *, const char *, int set, unsigned long);
char* mtsuite_format_hex(const void*, unsigned long);

#define mtsuite_skip(groups, named) \
    mtsuite_set_flag(groups, names, 1, MTSUITE_SKIP) 

int mtsuite_run_one(const struct Testgroup_t*, const struct Testcase_t *);
void mtsuite_set_aliases(const struct TestlistAlias_t *aliases);
int mtsuite_main(int argc, char **argv, struct Testgroup_t *groups);

// ------- OTHER MACROS -------

#define MTSUITE_BEGIN_STMT do {
#define MTSUITE_END_STMT } while(0)

#ifndef MTSUITE_EXIT_TEST_FUNCTION
#define MTSUITE_EXIT_TEST_FUNCTION \
    MTSUITE_BEGIN_STMT goto end; MTSUITE_END_STMT
#endif

#ifndef MTSUITE_DECLARE
#define MTSUITE_DECLARE(prefix, args)                       \
    MTSUITE_BEGIN_STMT                                      \
    printf("\n  %s %s:%d: ", prefix, __FILE__, __LINE__);   \
    printf args;                                            \
    MTSUITE_END_STMT
#endif

#define MTSUITE_GRIPE(args)  MTSUITE_DECLARE("FAIL", args)

#define MTSUITE_BLATHER(args)                                       \
    MTSUITE_BEGIN_STMT                                              \
    if(mtsuite_get_verbosity() > 1) MTSUITE_DECLARE("  OK", args);  \
    MTSUITE_END_STMT

#define MTSUITE_DIE(args)               \
    MTSUITE_BEGIN_STMT                  \
    mtsuite_set_test_failed();          \
    MTSUITE_GRIPE(args);                \
    MTSUITE_EXIT_TEST_FUNCTION;         \
    MTSUITE_END_STMT

#define MTSUITE_FAIL(args)              \
    MTSUITE_BEGIN_STMT                  \
    mtsuite_set_test_failed();          \
    MTSUITE_GRIPE(args);                \
    MTSUITE_END_STMT

#define mttsuite_abort_printf(msg)  MTSUITE_DIE(msg)
#define mttsuite_abort_perror(op) \
    MTSUITE_DIE(("%s: %s [%d]", (op), strerror(errno), errno))

#define mttsuite_abort_msg(msg)  MTSUITE_DIE(("%s", msg))
#define mttsuite_abort() MTSUITE_DIE(("%s", "(Failed.)"))

#define mttsuite_failprintf(msg) MTSUITE_FAIL(msg)
#define mttsuite_failperror(op) \
    MTSUITE_FAIL(("%s: %s [%d]", (op), strerror(errno), errno))
#define mttsuite_failmsg(msg) MTSUITE_FAIL(("%s", msg))
#define mttsuite_fail() MTSUITE_FAIL(("%s", "(Failed.)"))

#define mttsuite_skip()              \
    MTSUITE_BEGIN_STMT              \
    mtsuite_set_test_skipped();     \
    MTSUITE_EXIT_TEST_FUNCTION;     \
    MTSUITE_END_STMT
#define _mttsuite_want(b, msg, fail)     \
    MTSUITE_BEGIN_STMT                  \
    if(!(b)){                           \
        mtsuite_set_test_failed();      \
        MTSUITE_GRIPE(("%s", msg));     \
        fail;                           \
    }else{                              \
        MTSUITE_BLATHER(("%s", msg));   \
    }                                   \
    MTSUITE_END_STMT

#define mttsuite_want_msg(b, msg)    _mttsuite_want(b, msg, )

#define mttsuite_assert_msg(b, msg)      \
    _mttsuite_want(b, msg, MTSUITE_EXIT_TEST_FUNCTION)

#define mttsuite_want(b) mttsuite_want_msg((b), "want("#b")")

#define mttsuite_assert(b) mttsuite_assert_msg((b), "assert("#b")")

#define mttsuite_assert_test_fmt_type(a, b, teststr, type, test, printftype, \
    printffmt, setupblock, cleanupblock, dieOnFail)                         \
    MTSUITE_BEGIN_STMT                                                      \
    type val1_ = (a);                                                       \
    type val2_ = (b);                                                       \
    int _mtsuite_status = (test);                                           \
    if(!_mtsuite_status || mtsuite_get_verbosity() > 1){                    \
        printftype print_;                                                   \
        printftype print1_;                                                 \
        printftype print2_;                                                 \
        type value_ = val1_;                                                \
        setupblock;                                                         \
        print1_ = print_;                                                   \
        value_ = val2_;                                                     \
        setupblock;                                                         \
        print2_ = print_;                                                   \
        MTSUITE_DECLARE(_mtsuite_status?"  OK":"FAIL",                      \
                ("assert(%s): "printffmt" vs "printffmt,                    \
                teststr, print1_, print2_));                                \
        print_ = print1_;                                                   \
        cleanupblock;                                                       \
        if(!_mtsuite_status){                                               \
            mtsuite_set_test_failed();                                      \
            dieOnFail;                                                      \
        }                                                                   \
    }                                                                       \
    MTSUITE_END_STMT

#define mttsuite_assert_test_type(a,b,teststr,type,test,fmt,dieOnFail)     \
    mttsuite_assert_test_fmt_type(a,b,teststr,type,test,type,fmt,     \
        {print_=value_;}, {}, dieOnFail)

#define mttsuite_assert_type_opt(a,b,teststr,type,test,fmt,dieOnFail) \
    mttsuite_assert_test_fmt_type(a,b,teststr,type,test,type,fmt,     \
        {print_=value_?value_:"<NULL>";}, {}, dieOnFail)

#define mttsuite_assert_op_type(a,op,b,type,fmt)                         \
    mttsuite_assert_test_type(a,n,#a" "#op" "#b,type, (val1_ op val2_),  \
        fmt, MTSUITE_EXIT_TEST_FUNCTION)

#define mttsuite_int_op(a,op,b)                                          \
    mttsuite_assert_test_type(a,b,#a" "#op" "#b, long, (val1_ op val2_), \
        "%ld", MTSUITE_EXIT_TEST_FUNCTION)

#define mttsuite_uint_op(a,op,b)                                         \
    mttsuite_assert_test_type(a,b,#a" "#op" "#b, unsigned long, \
        (val1_ op val2_), "%lu", MTSUITE_EXIT_TEST_FUNCTION)

#define mttsuite_ptr_op(a,op,b)                                              \
    mttsuite_assert_test_type(a,b,#a" "#op" "#b, void*, (val1_ op val2_),    \
        "%p", MTSUITE_EXIT_TEST_FUNCTION)

#define mttsuite_str_op(a,op,b)                                          \
    mttsuite_assert_test_type(a,b,#a" "#op" "#b, const char*,            \
        (val1_ && val2_ && strcmp(val1_, val2_) op 0),                  \
        "<%s>", MTSUITE_EXIT_TEST_FUNCTION)

#define mttsuite_mem_op(expr1,op,expr2, len)                             \
    mttsuite_assert_test_fmt_type(expr1,expr2,#expr1" "#op" "#expr2,     \
         const void*,                                                   \
         (val1_ && val2_ && memcmp(val1_, val2_, len) op 0),            \
         char *, "%s",                                                  \
         {print_ = mtsuite_format_hex(value_, (len)); },                \
         { if(print_) free(print_); },                                  \
         MTSUITE_EXIT_TEST_FUNCTION \
        );

#define mttsuite_want_int_op(a,op,b)                                     \
    mttsuite_assert_test_type(a,b,#a" "#op" "#b, long, (val1_ op val2_), \
        "%ld", (void)0)

#define mttsuite_want_uint_op(a,op,b)                                     \
    mtsuite_assert_test_type(a,b,#a" "#op" "#b, unsigned long,          \
         (val1_ op val2_), "%lu", (void)0)

#define mttsuite_want_ptr_op(a,op,b)                                     \
    mttsuite_assert_test_type(a,b,#a" "#op" "#b, const void*,            \
        (val1_ op val2_), "%p", (void)0)

#define mttsuite_want_str_op(a,op,b)                                     \
    mttsuite_assert_test_type(a,b,#a" "#op" "#b, const char*,            \
        (val1_ op val2_), "<%s>", (void)0)


#endif
