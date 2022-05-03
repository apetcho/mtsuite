#ifndef __MTSUITE_H_
#define __MTSUITE_H_

// ------- CORE PROTOTYPES ----
#define MTSUITE_FORK            (1 << 0)
#define MTSUITE_SKIP            (1 << 1)
#define MTSUITE_ENABLED         (1 << 2)
#define MTSUITE_OFF_BY_DEFAULT  (1 << 3)
#define MTSUITE_FIRST_USER_FLAG (1 << 4)

typedef void (*TCallbackFn_t)(void*);
struct Testcase_t;

struct TestcaseSetup_t {
    void* (setup)(const struct Testcase_t *);
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
int mtsuite_main(int argc, const char **argv, struct Testgroup_t *groups);

// ------- OTHER MACROS -------
#endif
