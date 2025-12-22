#include "sprocket.h"
#include "sptest.h"

#include <stdio.h>
#include <string.h>

TEST(map_put_and_get_strings)
{
    sp_map m;
    sp_map_init(&m, sizeof(int));
    int one = 1;
    int two = 2;
    sp_map_put_str(&m, "one", &one);
    sp_map_put_str(&m, "two", &two);
    CHECK_EQ(sp_map_count(&m), 2u);
    CHECK_EQ(*(int *)sp_map_get_str(&m, "one"), 1);
    CHECK_EQ(*(int *)sp_map_get_str(&m, "two"), 2);
    CHECK(sp_map_get_str(&m, "three") == NULL);
    sp_map_free(&m);
}

TEST(map_put_overwrites_existing)
{
    sp_map m;
    sp_map_init(&m, sizeof(int));
    int a = 1;
    int b = 99;
    sp_map_put_str(&m, "k", &a);
    sp_map_put_str(&m, "k", &b);
    CHECK_EQ(sp_map_count(&m), 1u);
    CHECK_EQ(*(int *)sp_map_get_str(&m, "k"), 99);
    sp_map_free(&m);
}

TEST(map_key_is_copied)
{
    sp_map m;
    sp_map_init(&m, sizeof(int));
    char key[] = "mutable";
    int v = 5;
    sp_map_put_str(&m, key, &v);
    key[0] = 'X';
    CHECK(sp_map_get_str(&m, "mutable") != NULL);
    CHECK(sp_map_get_str(&m, key) == NULL);
    sp_map_free(&m);
}

TEST(map_binary_keys_with_embedded_zeros)
{
    sp_map m;
    sp_map_init(&m, sizeof(int));
    unsigned char k1[] = {1, 0, 2};
    unsigned char k2[] = {1, 0, 3};
    int a = 10;
    int b = 20;
    sp_map_put(&m, k1, sizeof k1, &a);
    sp_map_put(&m, k2, sizeof k2, &b);
    CHECK_EQ(*(int *)sp_map_get(&m, k1, sizeof k1), 10);
    CHECK_EQ(*(int *)sp_map_get(&m, k2, sizeof k2), 20);
    CHECK(!sp_map_has(&m, k1, 2));
    sp_map_free(&m);
}

TEST(map_empty_key_is_valid)
{
    sp_map m;
    sp_map_init(&m, sizeof(int));
    int v = 4;
    CHECK(sp_map_put(&m, "", 0, &v) != NULL);
    CHECK(sp_map_has(&m, "", 0));
    CHECK_EQ(*(int *)sp_map_get_str(&m, ""), 4);
    CHECK(sp_map_remove_str(&m, ""));
    CHECK_EQ(sp_map_count(&m), 0u);
    sp_map_free(&m);
}

TEST(map_grows_through_many_inserts)
{
    sp_map m;
    sp_map_init(&m, sizeof(int));
    char key[32];
    for (int i = 0; i < 5000; i++) {
        snprintf(key, sizeof key, "key-%d", i);
        CHECK(sp_map_put_str(&m, key, &i) != NULL);
    }
    CHECK_EQ(sp_map_count(&m), 5000u);
    for (int i = 0; i < 5000; i++) {
        snprintf(key, sizeof key, "key-%d", i);
        int *got = sp_map_get_str(&m, key);
        CHECK(got != NULL && *got == i);
    }
    CHECK(m.count * 4 <= m.cap * 3);
    sp_map_free(&m);
}

TEST(map_remove_returns_status)
{
    sp_map m;
    sp_map_init(&m, sizeof(int));
    int v = 1;
    sp_map_put_str(&m, "a", &v);
    CHECK(sp_map_remove_str(&m, "a"));
    CHECK(!sp_map_remove_str(&m, "a"));
    CHECK(sp_map_get_str(&m, "a") == NULL);
    CHECK(!sp_map_remove_str(&m, "never"));
    sp_map_free(&m);
}

TEST(map_remove_keeps_probe_chains_intact)
{
    sp_map m;
    sp_map_init(&m, sizeof(int));
    char key[16];
    for (int i = 0; i < 200; i++) {
        snprintf(key, sizeof key, "n%d", i);
        sp_map_put_str(&m, key, &i);
    }
    for (int i = 0; i < 200; i += 2) {
        snprintf(key, sizeof key, "n%d", i);
        CHECK(sp_map_remove_str(&m, key));
    }
    for (int i = 0; i < 200; i++) {
        snprintf(key, sizeof key, "n%d", i);
        int *got = sp_map_get_str(&m, key);
        if (i % 2 == 0) {
            CHECK(got == NULL);
        } else {
            CHECK(got != NULL && *got == i);
        }
    }
    CHECK_EQ(sp_map_count(&m), 100u);
    sp_map_free(&m);
}

TEST(map_matches_reference_model_under_random_ops)
{
    enum { KEYS = 300, OPS = 20000 };
    int model[KEYS];
    int present[KEYS];
    memset(present, 0, sizeof present);
    sp_map m;
    sp_map_init(&m, sizeof(int));
    unsigned long long s = 88172645463325252ull;
    size_t live = 0;
    for (int op = 0; op < OPS; op++) {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        int k = (int)(s % KEYS);
        int action = (int)((s >> 20) % 3);
        char key[16];
        snprintf(key, sizeof key, "k%d", k);
        if (action == 0) {
            int v = (int)(s >> 33);
            sp_map_put_str(&m, key, &v);
            if (!present[k]) {
                live++;
            }
            present[k] = 1;
            model[k] = v;
        } else if (action == 1) {
            bool removed = sp_map_remove_str(&m, key);
            CHECK_EQ(removed, present[k] != 0);
            if (present[k]) {
                live--;
            }
            present[k] = 0;
        } else {
            int *got = sp_map_get_str(&m, key);
            CHECK_EQ(got != NULL, present[k] != 0);
            if (got) {
                CHECK_EQ(*got, model[k]);
            }
        }
        CHECK_EQ(sp_map_count(&m), live);
    }
    sp_map_free(&m);
}

TEST(map_iteration_visits_every_entry_once)
{
    sp_map m;
    sp_map_init(&m, sizeof(int));
    char key[16];
    for (int i = 0; i < 50; i++) {
        snprintf(key, sizeof key, "i%d", i);
        sp_map_put_str(&m, key, &i);
    }
    int seen[50] = {0};
    size_t it = 0;
    const unsigned char *k;
    size_t klen;
    void *val;
    size_t visited = 0;
    while (sp_map_next(&m, &it, &k, &klen, &val)) {
        int v = *(int *)val;
        snprintf(key, sizeof key, "i%d", v);
        CHECK_EQ(klen, strlen(key));
        CHECK(memcmp(k, key, klen) == 0);
        seen[v]++;
        visited++;
    }
    CHECK_EQ(visited, 50u);
    for (int i = 0; i < 50; i++) {
        CHECK_EQ(seen[i], 1);
    }
    sp_map_free(&m);
}

TEST(map_clear_empties_but_stays_usable)
{
    sp_map m;
    sp_map_init(&m, sizeof(int));
    int v = 3;
    sp_map_put_str(&m, "x", &v);
    sp_map_put_str(&m, "y", &v);
    sp_map_clear(&m);
    CHECK_EQ(sp_map_count(&m), 0u);
    CHECK(sp_map_get_str(&m, "x") == NULL);
    sp_map_put_str(&m, "z", &v);
    CHECK_EQ(sp_map_count(&m), 1u);
    sp_map_free(&m);
}

TEST(map_reserve_avoids_later_rehash)
{
    sp_map m;
    sp_map_init(&m, sizeof(int));
    CHECK_EQ(sp_map_reserve(&m, 1000), 0);
    size_t cap = m.cap;
    char key[16];
    for (int i = 0; i < 1000; i++) {
        snprintf(key, sizeof key, "r%d", i);
        sp_map_put_str(&m, key, &i);
    }
    CHECK_EQ(m.cap, cap);
    sp_map_free(&m);
}

TEST(map_zero_sized_values_work_as_a_set)
{
    sp_map set;
    sp_map_init(&set, 0);
    CHECK(sp_map_put_str(&set, "alpha", NULL) != NULL);
    sp_map_put_str(&set, "beta", NULL);
    CHECK(sp_map_has(&set, "alpha", 5));
    CHECK(sp_map_has(&set, "beta", 4));
    CHECK(!sp_map_has(&set, "gamma", 5));
    CHECK_EQ(sp_map_count(&set), 2u);
    sp_map_free(&set);
}

TEST(map_stores_struct_values_in_place)
{
    typedef struct {
        double x;
        double y;
    } point_t;
    sp_map m;
    sp_map_init(&m, sizeof(point_t));
    point_t p = {1.5, -2.5};
    sp_map_put_str(&m, "origin", &p);
    point_t *got = sp_map_get_str(&m, "origin");
    CHECK(got != NULL && got->x == 1.5 && got->y == -2.5);
    got->x = 8.0;
    CHECK(((point_t *)sp_map_get_str(&m, "origin"))->x == 8.0);
    sp_map_free(&m);
}

TEST(hash_is_stable_and_spreads)
{
    CHECK_EQ(sp_hash_bytes("abc", 3), sp_hash_bytes("abc", 3));
    CHECK(sp_hash_bytes("abc", 3) != sp_hash_bytes("abd", 3));
    CHECK(sp_hash_bytes("", 0) != sp_hash_bytes("a", 1));
}
