#include "sprocket.h"
#include "sptest.h"

#include <stdint.h>
#include <string.h>

TEST(arena_alloc_returns_distinct_writable_memory)
{
    sp_arena a;
    sp_arena_init(&a, 256);
    char *x = sp_arena_alloc(&a, 16, 1);
    char *y = sp_arena_alloc(&a, 16, 1);
    CHECK(x != NULL && y != NULL && x != y);
    memset(x, 'x', 16);
    memset(y, 'y', 16);
    CHECK(x[15] == 'x' && y[0] == 'y');
    sp_arena_free(&a);
}

TEST(arena_respects_alignment)
{
    sp_arena a;
    sp_arena_init(&a, 128);
    size_t aligns[] = {1, 2, 4, 8, 16, 32, 64};
    for (size_t i = 0; i < sizeof aligns / sizeof aligns[0]; i++) {
        sp_arena_alloc(&a, 3, 1);
        void *p = sp_arena_alloc(&a, 5, aligns[i]);
        CHECK(p != NULL);
        CHECK(((uintptr_t)p & (aligns[i] - 1)) == 0);
    }
    sp_arena_free(&a);
}

TEST(arena_rejects_non_power_of_two_alignment)
{
    sp_arena a;
    sp_arena_init(&a, 128);
    CHECK(sp_arena_alloc(&a, 8, 3) == NULL);
    CHECK(sp_arena_alloc(&a, 8, 0) == NULL);
    sp_arena_free(&a);
}

TEST(arena_grows_with_new_blocks)
{
    sp_arena a;
    sp_arena_init(&a, 64);
    for (int i = 0; i < 100; i++) {
        unsigned char *p = sp_arena_alloc(&a, 40, 8);
        CHECK(p != NULL);
        memset(p, i, 40);
    }
    CHECK_EQ(sp_arena_total(&a), 4000u);
    sp_arena_free(&a);
}

TEST(arena_serves_oversized_requests)
{
    sp_arena a;
    sp_arena_init(&a, 64);
    unsigned char *big = sp_arena_alloc(&a, 10000, 16);
    CHECK(big != NULL);
    memset(big, 0xab, 10000);
    CHECK_EQ(big[9999], 0xab);
    sp_arena_free(&a);
}

TEST(arena_zalloc_zeroes_memory)
{
    sp_arena a;
    sp_arena_init(&a, 0);
    unsigned char *p = sp_arena_zalloc(&a, 512, 8);
    CHECK(p != NULL);
    for (size_t i = 0; i < 512; i++) {
        CHECK_EQ(p[i], 0);
    }
    sp_arena_free(&a);
}

TEST(arena_strdup_copies_string)
{
    sp_arena a;
    sp_arena_init(&a, 0);
    char src[] = "hello";
    char *copy = sp_arena_strdup(&a, src);
    src[0] = 'j';
    CHECK_STR(copy, "hello");
    sp_arena_free(&a);
}

TEST(arena_memdup_copies_bytes)
{
    sp_arena a;
    sp_arena_init(&a, 0);
    int values[] = {1, 2, 3, 4};
    int *copy = sp_arena_memdup(&a, values, sizeof values);
    CHECK(copy != NULL && copy != values);
    CHECK_EQ(copy[3], 4);
    sp_arena_free(&a);
}

TEST(arena_sprintf_formats_into_arena)
{
    sp_arena a;
    sp_arena_init(&a, 16);
    char *s = sp_arena_sprintf(&a, "%s-%d-%05.1f", "id", 42, 3.14159);
    CHECK_STR(s, "id-42-003.1");
    char *long_s = sp_arena_sprintf(&a, "%0200d", 7);
    CHECK_EQ(strlen(long_s), 200u);
    sp_arena_free(&a);
}

TEST(arena_new_macros_zero_and_align)
{
    typedef struct {
        char c;
        double d;
    } pair_t;
    sp_arena a;
    sp_arena_init(&a, 0);
    pair_t *p = SP_NEW(&a, pair_t);
    CHECK(p != NULL);
    CHECK(p->c == 0 && p->d == 0.0);
    CHECK(((uintptr_t)p % _Alignof(pair_t)) == 0);
    int *arr = SP_NEW_ARRAY(&a, int, 100);
    CHECK_EQ(arr[99], 0);
    sp_arena_free(&a);
}

TEST(arena_mark_and_rewind_reuses_space)
{
    sp_arena a;
    sp_arena_init(&a, 128);
    sp_arena_alloc(&a, 10, 1);
    sp_mark m = sp_arena_mark(&a);
    size_t before = sp_arena_total(&a);
    for (int i = 0; i < 50; i++) {
        sp_arena_alloc(&a, 64, 8);
    }
    CHECK(sp_arena_total(&a) > before);
    sp_arena_rewind(&a, m);
    CHECK_EQ(sp_arena_total(&a), before);
    char *p = sp_arena_alloc(&a, 8, 1);
    CHECK(p != NULL);
    sp_arena_free(&a);
}

TEST(arena_reset_keeps_working)
{
    sp_arena a;
    sp_arena_init(&a, 64);
    for (int i = 0; i < 20; i++) {
        sp_arena_alloc(&a, 50, 8);
    }
    sp_arena_reset(&a);
    CHECK_EQ(sp_arena_total(&a), 0u);
    char *p = sp_arena_alloc(&a, 32, 8);
    CHECK(p != NULL);
    memset(p, 1, 32);
    sp_arena_free(&a);
}

TEST(arena_zero_size_allocation_is_valid)
{
    sp_arena a;
    sp_arena_init(&a, 0);
    CHECK(sp_arena_alloc(&a, 0, 1) != NULL);
    char *e = sp_arena_strdup(&a, "");
    CHECK_STR(e, "");
    sp_arena_free(&a);
}

TEST(arena_huge_request_fails_cleanly)
{
    sp_arena a;
    sp_arena_init(&a, 0);
    CHECK(sp_arena_alloc(&a, SIZE_MAX, 8) == NULL);
    CHECK(sp_arena_alloc(&a, SIZE_MAX - 4, 1) == NULL);
    sp_arena_free(&a);
}
