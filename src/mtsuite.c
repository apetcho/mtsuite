#include<stdlib.h>
#include<assert.h>
// NO_FORKING not considered
#include "mtsuite.h"

#define MTSUITE_MAX_NAMELEN     16384

typedef struct Testcase_t Testcase_t;
typedef struct Testgroup_t Testgroup_t;
typedef struct TestcaseAlias_t TestcaseAlias_t;
typedef struct TestcaseSetup_t TestcaseSetup_t;

static int in_mtsuite_main = 0;
static int n_ok = 0;
static int n_bad = 0;
static int n_skipped = 0;

static int opt_verbosity = 1;  /* quiet=<0, terse=1, normal=1, verbose=2 */
static const char *verbosity_flag = "";

static const TestcaseAlias_t *cfg_aliases = NULL;

enum Outcome {SKIP=2, OK=1, FAIL=0 };
static enum Outcome cur_test_outcome = 0;

static const char *cur_test_prefix = NULL;
static const char *cur_test_name = NULL;

static void usage(Testgroup_t *groups, int list_groups);
static int process_test_option(Testgroup_t *groups, const char *test);

static enum Outcome _testcase_run_bare(const Testcase_t *tcase){
    void *env = NULL;
    int outcome;
    if(tcase->config){
        env = tcase->config->setup(tcase);
        if(!env){
            return FAIL;
        }else if(env == (void*)MTSUITE_SKIP){
            return SKIP;
        }
    }

    cur_test_outcome = OK;
    tcase->callback(env);
    outcome = cur_test_outcome;

    if(tcase->config){
        if(tcase->config->cleanup(tcase, env) == 0){
            outcome = FAIL;
        }
    }

    return outcome;
}
