#include "sprocket.h"
#include "sptest.h"

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    return (x > y) - (x < y);
}

TEST(vec_push_and_index)
{
    sp_vec v;
    sp_vec_init(&v, sizeof(int));
    for (int i = 0; i < 1000; i++) {
        CHECK(sp_vec_push(&v, &i) != NULL);
    }
    CHECK_EQ(v.len, 1000u);
    CHECK_EQ(SP_VEC_GET(&v, int, 0), 0);
    CHECK_EQ(SP_VEC_GET(&v, int, 999), 999);
    CHECK(sp_vec_at(&v, 1000) == NULL);
    sp_vec_free(&v);
}

TEST(vec_push_null_zero_fills)
{
    sp_vec v;
    sp_vec_init(&v, sizeof(long));
    long *slot = sp_vec_push(&v, NULL);
    CHECK(slot != NULL);
    CHECK_EQ(*slot, 0);
    sp_vec_free(&v);
}

TEST(vec_pop_returns_last)
{
    sp_vec v;
    sp_vec_init(&v, sizeof(int));
    int a = 7;
    int b = 9;
    sp_vec_push(&v, &a);
    sp_vec_push(&v, &b);
    int out = 0;
    CHECK(sp_vec_pop(&v, &out));
    CHECK_EQ(out, 9);
    CHECK(sp_vec_pop(&v, NULL));
    CHECK(!sp_vec_pop(&v, &out));
    sp_vec_free(&v);
}

TEST(vec_extend_appends_block)
{
    sp_vec v;
    sp_vec_init(&v, sizeof(int));
    int first[] = {1, 2, 3};
    int second[] = {4, 5};
    CHECK_EQ(sp_vec_extend(&v, first, 3), 0);
    CHECK_EQ(sp_vec_extend(&v, second, 2), 0);
    CHECK_EQ(sp_vec_extend(&v, second, 0), 0);
    CHECK_EQ(v.len, 5u);
    CHECK_EQ(SP_VEC_GET(&v, int, 4), 5);
    sp_vec_free(&v);
}

TEST(vec_insert_shifts_tail)
{
    sp_vec v;
    sp_vec_init(&v, sizeof(int));
    int vals[] = {1, 2, 4, 5};
    sp_vec_extend(&v, vals, 4);
    int three = 3;
    CHECK(sp_vec_insert(&v, 2, &three) != NULL);
    for (int i = 0; i < 5; i++) {
        CHECK_EQ(SP_VEC_GET(&v, int, (size_t)i), i + 1);
    }
    int zero = 0;
    CHECK(sp_vec_insert(&v, 0, &zero) != NULL);
    CHECK(sp_vec_insert(&v, v.len, &zero) != NULL);
    CHECK(sp_vec_insert(&v, v.len + 1, &zero) == NULL);
    sp_vec_free(&v);
}

TEST(vec_remove_preserves_order)
{
    sp_vec v;
    sp_vec_init(&v, sizeof(int));
    int vals[] = {10, 20, 30, 40};
    sp_vec_extend(&v, vals, 4);
    CHECK(sp_vec_remove(&v, 1));
    CHECK_EQ(v.len, 3u);
    CHECK_EQ(SP_VEC_GET(&v, int, 1), 30);
    CHECK_EQ(SP_VEC_GET(&v, int, 2), 40);
    CHECK(!sp_vec_remove(&v, 3));
    sp_vec_free(&v);
}

TEST(vec_swap_remove_moves_last)
{
    sp_vec v;
    sp_vec_init(&v, sizeof(int));
    int vals[] = {10, 20, 30, 40};
    sp_vec_extend(&v, vals, 4);
    CHECK(sp_vec_swap_remove(&v, 0));
    CHECK_EQ(SP_VEC_GET(&v, int, 0), 40);
    CHECK(sp_vec_swap_remove(&v, 2));
    CHECK_EQ(v.len, 2u);
    CHECK(!sp_vec_swap_remove(&v, 5));
    sp_vec_free(&v);
}

TEST(vec_sort_and_bsearch)
{
    sp_vec v;
    sp_vec_init(&v, sizeof(int));
    unsigned state = 12345;
    for (int i = 0; i < 500; i++) {
        state = state * 1103515245u + 12345u;
        int x = (int)((state >> 8) % 100000);
        sp_vec_push(&v, &x);
    }
    sp_vec_sort(&v, cmp_int);
    for (size_t i = 1; i < v.len; i++) {
        CHECK(SP_VEC_GET(&v, int, i - 1) <= SP_VEC_GET(&v, int, i));
    }
    int target = SP_VEC_GET(&v, int, 250);
    ptrdiff_t at = sp_vec_bsearch(&v, &target, cmp_int);
    CHECK(at >= 0);
    CHECK_EQ(SP_VEC_GET(&v, int, (size_t)at), target);
    int missing = -5;
    CHECK_EQ(sp_vec_bsearch(&v, &missing, cmp_int), -1);
    sp_vec_free(&v);
}

TEST(vec_reserve_and_shrink)
{
    sp_vec v;
    sp_vec_init(&v, sizeof(int));
    CHECK_EQ(sp_vec_reserve(&v, 100), 0);
    CHECK(v.cap >= 100);
    int x = 1;
    sp_vec_push(&v, &x);
    sp_vec_push(&v, &x);
    CHECK_EQ(sp_vec_shrink(&v), 0);
    CHECK_EQ(v.cap, 2u);
    sp_vec_clear(&v);
    CHECK_EQ(sp_vec_shrink(&v), 0);
    CHECK_EQ(v.cap, 0u);
    sp_vec_free(&v);
}

TEST(vec_reserve_overflow_fails)
{
    sp_vec v;
    sp_vec_init(&v, sizeof(long));
    CHECK(sp_vec_reserve(&v, SIZE_MAX / 2) != 0);
    CHECK(v.data == NULL);
    sp_vec_free(&v);
}

TEST(vec_holds_structs)
{
    typedef struct {
        int id;
        double weight;
        char tag[8];
    } item_t;
    sp_vec v;
    sp_vec_init(&v, sizeof(item_t));
    for (int i = 0; i < 64; i++) {
        item_t it = {i, i * 0.5, "tag"};
        sp_vec_push(&v, &it);
    }
    item_t *last = sp_vec_at(&v, 63);
    CHECK_EQ(last->id, 63);
    CHECK(last->weight == 31.5);
    CHECK_STR(last->tag, "tag");
    sp_vec_free(&v);
}

TEST(vec_free_is_idempotent)
{
    sp_vec v;
    sp_vec_init(&v, sizeof(int));
    int x = 3;
    sp_vec_push(&v, &x);
    sp_vec_free(&v);
    sp_vec_free(&v);
    CHECK_EQ(v.len, 0u);
}
