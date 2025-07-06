#include "sprocket.h"

#include <stdlib.h>
#include <string.h>

#define MIN_CAP 8u

uint64_t sp_hash_bytes(const void *data, size_t len)
{
    const unsigned char *p = data;
    uint64_t h = 14695981039346656037ull;
    for (size_t i = 0; i < len; i++) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    h ^= h >> 32;
    h *= 0x9e3779b97f4a7c15ull;
    h ^= h >> 29;
    return h;
}

void sp_map_init(sp_map *m, size_t val_size)
{
    m->slots = NULL;
    m->vals = NULL;
    m->cap = 0;
    m->count = 0;
    m->val_size = val_size;
}

static void release_keys(sp_map *m)
{
    for (size_t i = 0; i < m->cap; i++) {
        if (m->slots[i].used) {
            free(m->slots[i].key);
        }
    }
}

void sp_map_free(sp_map *m)
{
    release_keys(m);
    free(m->slots);
    free(m->vals);
    m->slots = NULL;
    m->vals = NULL;
    m->cap = 0;
    m->count = 0;
}

void sp_map_clear(sp_map *m)
{
    release_keys(m);
    if (m->slots) {
        memset(m->slots, 0, m->cap * sizeof(sp_slot));
    }
    m->count = 0;
}

static unsigned char *val_at(const sp_map *m, size_t i)
{
    if (m->val_size == 0) {
        return m->slots[i].key;
    }
    return m->vals + i * m->val_size;
}

static bool key_equal(const sp_slot *s, uint64_t hash, const void *key, size_t len)
{
    return s->used && s->hash == hash && s->key_len == len && (len == 0 || memcmp(s->key, key, len) == 0);
}

static ptrdiff_t find_index(const sp_map *m, uint64_t hash, const void *key, size_t len)
{
    if (m->cap == 0) {
        return -1;
    }
    size_t mask = m->cap - 1;
    size_t i = (size_t)hash & mask;
    while (m->slots[i].used) {
        if (key_equal(&m->slots[i], hash, key, len)) {
            return (ptrdiff_t)i;
        }
        i = (i + 1) & mask;
    }
    return -1;
}

static size_t cap_for(size_t entries)
{
    size_t cap = MIN_CAP;
    while (cap - cap / 4 < entries) {
        cap *= 2;
    }
    return cap;
}

static int rehash(sp_map *m, size_t new_cap)
{
    if (new_cap > SIZE_MAX / sizeof(sp_slot)) {
        return -1;
    }
    if (m->val_size && new_cap > SIZE_MAX / m->val_size) {
        return -1;
    }
    sp_slot *slots = calloc(new_cap, sizeof(sp_slot));
    unsigned char *vals = m->val_size ? malloc(new_cap * m->val_size) : NULL;
    if (!slots || (m->val_size && !vals)) {
        free(slots);
        free(vals);
        return -1;
    }
    size_t mask = new_cap - 1;
    for (size_t i = 0; i < m->cap; i++) {
        if (!m->slots[i].used) {
            continue;
        }
        size_t j = (size_t)m->slots[i].hash & mask;
        while (slots[j].used) {
            j = (j + 1) & mask;
        }
        slots[j] = m->slots[i];
        if (m->val_size) {
            memcpy(vals + j * m->val_size, val_at(m, i), m->val_size);
        }
    }
    free(m->slots);
    free(m->vals);
    m->slots = slots;
    m->vals = vals;
    m->cap = new_cap;
    return 0;
}

int sp_map_reserve(sp_map *m, size_t entries)
{
    if (entries > SIZE_MAX / 2) {
        return -1;
    }
    size_t want = cap_for(entries);
    if (want <= m->cap) {
        return 0;
    }
    return rehash(m, want);
}

void *sp_map_put(sp_map *m, const void *key, size_t key_len, const void *val)
{
    uint64_t hash = sp_hash_bytes(key, key_len);
    ptrdiff_t found = find_index(m, hash, key, key_len);
    if (found >= 0) {
        unsigned char *dst = val_at(m, (size_t)found);
        if (val && m->val_size) {
            memcpy(dst, val, m->val_size);
        }
        return dst;
    }
    if (m->cap == 0 || m->count + 1 > m->cap - m->cap / 4) {
        size_t next = m->cap == 0 ? MIN_CAP : m->cap * 2;
        if (rehash(m, next) != 0) {
            return NULL;
        }
    }
    unsigned char *copy = malloc(key_len ? key_len : 1);
    if (!copy) {
        return NULL;
    }
    if (key_len) {
        memcpy(copy, key, key_len);
    }
    size_t mask = m->cap - 1;
    size_t i = (size_t)hash & mask;
    while (m->slots[i].used) {
        i = (i + 1) & mask;
    }
    m->slots[i].hash = hash;
    m->slots[i].key = copy;
    m->slots[i].key_len = key_len;
    m->slots[i].used = true;
    unsigned char *dst = val_at(m, i);
    if (m->val_size) {
        if (val) {
            memcpy(dst, val, m->val_size);
        } else {
            memset(dst, 0, m->val_size);
        }
    }
    m->count++;
    return dst;
}

void *sp_map_get(const sp_map *m, const void *key, size_t key_len)
{
    ptrdiff_t i = find_index(m, sp_hash_bytes(key, key_len), key, key_len);
    return i < 0 ? NULL : val_at(m, (size_t)i);
}

bool sp_map_has(const sp_map *m, const void *key, size_t key_len)
{
    return find_index(m, sp_hash_bytes(key, key_len), key, key_len) >= 0;
}

bool sp_map_remove(sp_map *m, const void *key, size_t key_len)
{
    ptrdiff_t found = find_index(m, sp_hash_bytes(key, key_len), key, key_len);
    if (found < 0) {
        return false;
    }
    size_t mask = m->cap - 1;
    size_t hole = (size_t)found;
    free(m->slots[hole].key);
    size_t j = hole;
    for (;;) {
        j = (j + 1) & mask;
        if (!m->slots[j].used) {
            break;
        }
        size_t ideal = (size_t)m->slots[j].hash & mask;
        if (((j - ideal) & mask) >= ((j - hole) & mask)) {
            m->slots[hole] = m->slots[j];
            if (m->val_size) {
                memcpy(val_at(m, hole), val_at(m, j), m->val_size);
            }
            hole = j;
        }
    }
    memset(&m->slots[hole], 0, sizeof(sp_slot));
    m->count--;
    return true;
}

bool sp_map_next(const sp_map *m, size_t *it, const unsigned char **key, size_t *key_len, void **val)
{
    while (*it < m->cap) {
        size_t i = (*it)++;
        if (m->slots[i].used) {
            if (key) {
                *key = m->slots[i].key;
            }
            if (key_len) {
                *key_len = m->slots[i].key_len;
            }
            if (val) {
                *val = val_at(m, i);
            }
            return true;
        }
    }
    return false;
}

void *sp_map_put_str(sp_map *m, const char *key, const void *val)
{
    return sp_map_put(m, key, strlen(key), val);
}

void *sp_map_get_str(const sp_map *m, const char *key)
{
    return sp_map_get(m, key, strlen(key));
}

bool sp_map_remove_str(sp_map *m, const char *key)
{
    return sp_map_remove(m, key, strlen(key));
}

size_t sp_map_count(const sp_map *m)
{
    return m->count;
}
