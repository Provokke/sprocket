#ifndef SPTEST_H
#define SPTEST_H

#include <stdio.h>
#include <string.h>

typedef void (*sptest_fn)(void);

void sptest_register(const char *name, sptest_fn fn);
void sptest_fail(const char *file, int line, const char *expr);

#define TEST(name)                                                      \
    static void name(void);                                             \
    __attribute__((constructor)) static void register_##name(void)      \
    {                                                                   \
        sptest_register(#name, name);                                   \
    }                                                                   \
    static void name(void)

#define CHECK(expr)                                  \
    do {                                             \
        if (!(expr)) {                               \
            sptest_fail(__FILE__, __LINE__, #expr);  \
            return;                                  \
        }                                            \
    } while (0)

#define CHECK_EQ(a, b) CHECK((a) == (b))
#define CHECK_STR(a, b) CHECK(strcmp((a), (b)) == 0)

#endif
