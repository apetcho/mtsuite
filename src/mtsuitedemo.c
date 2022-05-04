#include "mtsuite.h"
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<time.h>
#include<unistd.h>
#include<printf.h>

#define OP_LT <
#define OP_EQ ==
#define OP_NE !=
#define OP_GE >=
#define OP_LE <=


void test_strcmp(void *data){
    (void)data;
    if(strcmp("", "")){
        mttsuite_abort_msg("The empty string was not equal to itself");
    }
    mttsuite_assert(strcmp("testcase", "testcase") == 0);
    mttsuite_want(strcmp("mtsuites", "mtsuite") > 0);
    mttsuite_int_op(strcmp("abc", "abc"), OP_EQ, 0);
    mttsuite_int_op(strcmp("abc", "abcd"), OP_LT, 0);
    mttsuite_str_op("abc", OP_LT, "abcd");

end:
    ;
}

typedef struct DataBuffer{
    char buf1[512];
    char buf2[512];
} DataBuffer;

void* new_db(const struct Testcase_t *tcase){
    DataBuffer *db = malloc(sizeof(DataBuffer));
    return db;
}

int delete_db(const struct Testcase_t *tcase, void *ptr){
    DataBuffer *db = ptr;
    if(db){
        free(db);
        return 1;
    }
    return 0;
}

struct TestcaseSetup_t dbsetup = {
    .setup=new_db,
    .cleanup=delete_db
};

void test_memcpy(void *ptr){
    DataBuffer *db = ptr;
    char *mem = NULL;
    strcpy(db->buf1, "String 0");
    memcpy(db->buf2, db->buf1, sizeof(db->buf1));
    mttsuite_str_op(db->buf1, OP_EQ, db->buf2);
    db->buf2[100] = 3;
    mttsuite_mem_op(db->buf1, OP_LT, db->buf2, sizeof(db->buf1));
    mem = strdup("Hello world.");
    mttsuite_assert(mem);
    mttsuite_str_op(db->buf1, OP_NE, mem);

end:
    if(mem){free(mem); }
}

void test_timeout(void *ptr){
    time_t t1, t2;
    (void)ptr;
    t1 = time(NULL);
    sleep(5);
    t2 = time(NULL);
    mttsuite_int_op(t2-t1, OP_GE, 4);
    mttsuite_int_op(t2-t1, OP_LE, 6);

end:
    ;
}

struct Testcase_t demoTests[] = {
    {.name="strcmp", .callback=test_strcmp, },
    {.name="memcpy", .callback=test_memcpy, .config=&dbsetup, },
    {.name="timeout", .callback=test_timeout, },
    MTSUITE_END_OF_TESTCASES
};

struct Testgroup_t groups[] = {
    {.prefix="demo/", .cases=demoTests},
    MTSUITE_END_OF_GROUPS
};


const char *alltests[] = {"+..", NULL};
const char *slowtests[] = {"+demo/timeout", NULL};
struct TestlistAlias_t aliases[] = {
    {"ALL", alltests},
    {"SLOW", slowtests},
    MTSUITE_END_OF_ALIASES
};


int main(int argc, char **argv){
    mtsuite_set_aliases(aliases);
    return mtsuite_main(argc, argv, groups);   
}
