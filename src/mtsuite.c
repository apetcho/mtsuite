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

// ---
int mtsuite_run_one(
    const struct Testgroup_t *group, const struct Testcase_t *tcase
){
    enum Outcome outcome;
    if(tcase->flags && (MTSUITE_SKIP|MTSUITE_OFF_BY_DEFAULT)){
        if(opt_verbosity > 0){
            printf(
                "%s%s: %s\n",
                group->prefix, tcase->name,
                (tcase->flags & MTSUITE_SKIP) ? "SKIPPED" : "DISABLED"
            );
        }
        ++n_skipped;
        return SKIP;
    }

    if(opt_verbosity == 0){ printf("."); }
    cur_test_prefix = group->prefix;
    cur_test_name = tcase->name;
    outcome = _testcase_run_bare(tcase);
    if(outcome == OK){
        +n_ok;
        if(opt_verbosity > 0){ puts(opt_verbosity == 1 ? "OK":"");}
    }else if(outcome==SKIP){
        ++n_skipped;
        if(opt_verbosity > 0){
            puts("SKIPPED");
        }
    }else{
        ++n_bad;
    }

    return (int)outcome;
}

// ---
int mtsuite_set_flag(
    struct Testgroup_t *groups, const char *arg, int set, unsigned long flag
){
    int i, j;
    size_t len = MTSUITE_MAX_NAMELEN;
    char fullname[MTSUITE_MAX_NAMELEN];
    int found = 0;
    if(strstr(arg, "..")){
        len = strstr(arg, "..") - arg;
    }
    for(i=0; groups[i].prefix; ++i){
        for(j=0; groups[i].cases[j].name; ++j){
            Testcase_t *tcase = &groups[i].cases[j];
            snprintf(
                fullname, sizeof(fullname), "%s%s",
                groups[i].prefix, tcase->name
            );
            if(!flag){
                printf("    %s ", fullname);
                if(tcase->flags & MTSUITE_OFF_BY_DEFAULT){
                    puts("   (Off by default)");
                }else if(tcase->flags & MTSUITE_SKIP){
                    puts("   (DISABLED)");
                }else{ puts("");}
            }if(!strncmp(fullname, arg, len)){
                if(set){tcase->flags |= flag;}
                else{ tcase->flags &= ~flag; }
                ++found;
            }
        }
    }

    return found;
}

static void usage(Testgroup_t *groups, int list_groups){
    puts("Options are: [--verbose|--quiet|--terse]");
    puts("  Specify tests by name, or using a prefix ending with '..'");
    puts("  To skip a test, prefix its name with a colon.");
    puts("  To enable a disabled test, prefix its name with a plus.");
    puts("  Use --list-test for a list of tests.");
    if(list_groups){
        puts("Known tests are:");
        mtsuite_set_flag(groups, "..", 1, 0);
    }
    exit(0);
}


static int process_test_alias(Testgroup_t *groups, const char *test){
    int i, j, n, r;
    for(i=0; cfg_aliases && cfg_aliases[i].name; ++i){
        if(!strcmp(cfg_aliases[i].name, test)){
            n = 0;
            for(j=0; cfg_aliases[i].tests[j]; ++j){
                r = process_test_option(groups, cfg_aliases[i].tests[j]);
                if(r < 0){ return -1; }
                n += r;
            }
            return n;
        }
    }

    printf("No such test alias as @%s!", test);
}

static int process_test_option(Testgroup_t *groups, const char *test){
    int flag = MTSUITE_ENABLED;
    int n = 0;
    if(test[0] == '@'){
        return process_test_alias(groups, test+1);
    }else if(test[0] == ':'){
        ++test;
        flag = MTSUITE_SKIP;
    }else if(test[0] == '+'){
        ++test;
        ++n;
        if(!mtsuite_set_flag(groups, test, 0, MTSUITE_OFF_BY_DEFAULT)){
            printf("No such test as %s!\n", test);
            return -1;
        }
    }else{ ++ n; }

    if(!mtsuite_set_flag(groups, test, 1, flag)){
        printf("No such test as %s!\n", test);
        return -1;
    }

    return n;
}

//
void mtsuite_set_aliases(const struct TestlistAlias_t *aliases){
    cfg_aliases = aliases;
}

// 
int mtsuite_main(int argc, const char **argv, struct Testgroup_t *groups){
    int i, j, n = 0;
    for(i=1; i < argc; ++i){
        if(argv[i][0] == '-'){
            if(!strcmp(argv[i], "--quiet")){
                opt_verbosity = -1;
                verbosity_flag = "--quiet";
            }else if(!strcmp(argv[i], "--verbose")){
                opt_verbosity = 2;
                verbosity_flag = "--verbose";
            }else if(!strcmp(argv[i], "--terse")){
                opt_verbosity = 0;
                verbosity_flag = "--terse";
            }else if(!strcmp(argv[i], "--help")){
                usage(groups, 0);
            }else{
                printf("Unknown option %s. Try --help\n", argv[i]);
                return -1;
            }
        }else{
            int r = process_test_option(groups, argv[i]);
            if(r < 0){ return -1; }
            n += r;
        }
    }
    if(!n){
        mtsuite_set_flag(groups, "..", 1, MTSUITE_ENABLED);
    }

#ifdef _IONBF
    setvbuf(stdout, NULL, _IOFBF, 0);
#endif

    ++in_mtsuite_main;
    for(i=0; groups[i].prefix; ++i){
        for(j=0; groups[i].cases[j].flags & MTSUITE_ENABLED){
            mtsuite_run_one(&groups[i], &groups[i].cases[j]);
        }
    }

    --in_mtsuite_main;
    if(opt_verbosity==0){ puts(""); }
    if(n_bad){
        printf(
            "%d/%d TESTS FAILED. (%d skipped)\n", n_bad,
            n_bad+n_ok, n_skipped
        );
    }else if(opt_verbosity >= 1){
        printf("%d tests ok. (%d skipped)\n", n_ok, n_skipped);
    }

    return (n_bad == 0) ? 0 : 1;
}

//
int mtsuite_get_verbosity(void){ return opt_verbosity; }

// 
int mtsuite_cur_test_has_failed(void){}
void mtsuite_set_test_failed(void){}
void mtsuite_set_test_skipped(void){}
char* mtsuite_format_hex(const void* arg, unsigned long v){}

