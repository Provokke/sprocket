#ifndef SPROCKET_H
#define SPROCKET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct sp_block sp_block;

typedef struct sp_arena {
    sp_block *head;
    size_t block_size;
    size_t total;
} sp_arena;

typedef struct sp_mark {
    sp_block *block;
    size_t used;
} sp_mark;

void sp_arena_init(sp_arena *a, size_t block_size);
void sp_arena_free(sp_arena *a);
void sp_arena_reset(sp_arena *a);
void *sp_arena_alloc(sp_arena *a, size_t size, size_t align);
void *sp_arena_zalloc(sp_arena *a, size_t size, size_t align);
void *sp_arena_memdup(sp_arena *a, const void *src, size_t size);
char *sp_arena_strdup(sp_arena *a, const char *s);
char *sp_arena_sprintf(sp_arena *a, const char *fmt, ...);
sp_mark sp_arena_mark(const sp_arena *a);
void sp_arena_rewind(sp_arena *a, sp_mark m);
size_t sp_arena_total(const sp_arena *a);

#define SP_NEW(a, T) ((T *)sp_arena_zalloc((a), sizeof(T), _Alignof(T)))
#define SP_NEW_ARRAY(a, T, n) ((T *)sp_arena_zalloc((a), sizeof(T) * (n), _Alignof(T)))

typedef struct sp_vec {
    unsigned char *data;
    size_t len;
    size_t cap;
    size_t elem;
} sp_vec;

typedef int (*sp_cmp_fn)(const void *a, const void *b);

void sp_vec_init(sp_vec *v, size_t elem);
void sp_vec_free(sp_vec *v);
void sp_vec_clear(sp_vec *v);
int sp_vec_reserve(sp_vec *v, size_t cap);
int sp_vec_shrink(sp_vec *v);
void *sp_vec_push(sp_vec *v, const void *item);
int sp_vec_extend(sp_vec *v, const void *items, size_t n);
bool sp_vec_pop(sp_vec *v, void *out);
void *sp_vec_at(const sp_vec *v, size_t i);
void *sp_vec_insert(sp_vec *v, size_t i, const void *item);
bool sp_vec_remove(sp_vec *v, size_t i);
bool sp_vec_swap_remove(sp_vec *v, size_t i);
void sp_vec_sort(sp_vec *v, sp_cmp_fn cmp);
ptrdiff_t sp_vec_bsearch(const sp_vec *v, const void *key, sp_cmp_fn cmp);

#define SP_VEC_GET(v, T, i) (*(T *)sp_vec_at((v), (i)))

typedef struct sp_slot {
    uint64_t hash;
    unsigned char *key;
    size_t key_len;
    bool used;
} sp_slot;

typedef struct sp_map {
    sp_slot *slots;
    unsigned char *vals;
    size_t cap;
    size_t count;
    size_t val_size;
} sp_map;

uint64_t sp_hash_bytes(const void *data, size_t len);
void sp_map_init(sp_map *m, size_t val_size);
void sp_map_free(sp_map *m);
void sp_map_clear(sp_map *m);
int sp_map_reserve(sp_map *m, size_t entries);
void *sp_map_put(sp_map *m, const void *key, size_t key_len, const void *val);
void *sp_map_get(const sp_map *m, const void *key, size_t key_len);
bool sp_map_has(const sp_map *m, const void *key, size_t key_len);
bool sp_map_remove(sp_map *m, const void *key, size_t key_len);
bool sp_map_next(const sp_map *m, size_t *it, const unsigned char **key, size_t *key_len, void **val);

void *sp_map_put_str(sp_map *m, const char *key, const void *val);
void *sp_map_get_str(const sp_map *m, const char *key);
bool sp_map_remove_str(sp_map *m, const char *key);
size_t sp_map_count(const sp_map *m);

#endif
