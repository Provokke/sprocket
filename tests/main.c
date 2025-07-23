#include "sptest.h"

#include <stdlib.h>

#define MAX_TESTS 256

static struct {
    const char *name;
    sptest_fn fn;
} registry[MAX_TESTS];

static size_t registered;
static int current_failed;

void sptest_register(const char *name, sptest_fn fn)
{
    if (registered < MAX_TESTS) {
        registry[registered].name = name;
        registry[registered].fn = fn;
        registered++;
    }
}

void sptest_fail(const char *file, int line, const char *expr)
{
    fprintf(stderr, "    %s:%d: check failed: %s\n", file, line, expr);
    current_failed = 1;
}

int main(int argc, char **argv)
{
    const char *filter = argc > 1 ? argv[1] : NULL;
    size_t ran = 0;
    size_t failed = 0;
    for (size_t i = 0; i < registered; i++) {
        if (filter && !strstr(registry[i].name, filter)) {
            continue;
        }
        current_failed = 0;
        registry[i].fn();
        ran++;
        if (current_failed) {
            failed++;
            printf("FAIL %s\n", registry[i].name);
        } else {
            printf("ok   %s\n", registry[i].name);
        }
    }
    printf("%zu tests, %zu passed, %zu failed\n", ran, ran - failed, failed);
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
