#include<stdlib.h>
#include<assert.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<unistd.h>
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

#define MTSUITE_MAGIC_EXIT_CODE 42


static enum Outcome _testcase_run_forked(
    const Testgroup_t* group, const Testcase_t *tcase
){
    int outpipe[2];
    pid_t pid;
    if(pipe(outpipe)){
        perror("opening pipe");
    }

    if(opt_verbosity > 0){
        printf("[forking] ");
    }
    pid = fork();
    if(!pid){
        /* child */
        int testr, writer;
        char b[1];
        close(outpipe[0]);
        testr = _testcase_run_bare(tcase);
        assert(0<=(int)testr && (int)testr<= 2);
        b[0] = "NYS"[testr];
        writer = (int)write(outpipe[1], b, 1);
        if(writer != 1){
            perror("write outcome to pipe");
            exit(1);
        }
        exit(0);
        return FAIL; /* unreachable */
    }else{
        /* parent */
        int status, r;
        char b[1];
        close(outpipe[1]);
        r = (int)read(outpipe[0], b, 1);
        if(r == 0){
            printf("[Lost connection!] ");
            return FAIL;
        }else if(r != 1){
            perror("read outcome from pipe");
        }
        r = waitpid(pid, &status, 0);
        close(outpipe[0]);
        if(r == -1){
            perror("waitpid");
            return FAIL;
        }
        if(!WIFEXITED(status) || WEXITSTATUS(status) != 0){
            printf("[did not exit cleanly.]");
            return FAIL;
        }

        return b[0] == 'Y' ? OK : (b[0] == 'S' ? SKIP : FAIL);
    }
}
