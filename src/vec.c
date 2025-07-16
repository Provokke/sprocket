#include "sprocket.h"

#include <stdlib.h>
#include <string.h>

void sp_vec_init(sp_vec *v, size_t elem)
{
    v->data = NULL;
    v->len = 0;
    v->cap = 0;
    v->elem = elem;
}

void sp_vec_free(sp_vec *v)
{
    free(v->data);
    v->data = NULL;
    v->len = 0;
    v->cap = 0;
}

void sp_vec_clear(sp_vec *v)
{
    v->len = 0;
}

int sp_vec_reserve(sp_vec *v, size_t cap)
{
    if (cap <= v->cap) {
        return 0;
    }
    if (v->elem == 0 || cap > SIZE_MAX / v->elem) {
        return -1;
    }
    size_t next = v->cap ? v->cap : 4;
    while (next < cap) {
        if (next > SIZE_MAX / 2) {
            next = cap;
            break;
        }
        next *= 2;
    }
    if (next > SIZE_MAX / v->elem) {
        next = cap;
    }
    void *p = realloc(v->data, next * v->elem);
    if (!p) {
        return -1;
    }
    v->data = p;
    v->cap = next;
    return 0;
}

int sp_vec_shrink(sp_vec *v)
{
    if (v->len == v->cap) {
        return 0;
    }
    if (v->len == 0) {
        free(v->data);
        v->data = NULL;
        v->cap = 0;
        return 0;
    }
    void *p = realloc(v->data, v->len * v->elem);
    if (!p) {
        return -1;
    }
    v->data = p;
    v->cap = v->len;
    return 0;
}

void *sp_vec_at(const sp_vec *v, size_t i)
{
    if (i >= v->len) {
        return NULL;
    }
    return v->data + i * v->elem;
}

void *sp_vec_push(sp_vec *v, const void *item)
{
    if (v->len == SIZE_MAX || sp_vec_reserve(v, v->len + 1) != 0) {
        return NULL;
    }
    unsigned char *slot = v->data + v->len * v->elem;
    if (item) {
        memcpy(slot, item, v->elem);
    } else {
        memset(slot, 0, v->elem);
    }
    v->len++;
    return slot;
}

int sp_vec_extend(sp_vec *v, const void *items, size_t n)
{
    if (n == 0) {
        return 0;
    }
    if (n > SIZE_MAX - v->len || sp_vec_reserve(v, v->len + n) != 0) {
        return -1;
    }
    memcpy(v->data + v->len * v->elem, items, n * v->elem);
    v->len += n;
    return 0;
}

bool sp_vec_pop(sp_vec *v, void *out)
{
    if (v->len == 0) {
        return false;
    }
    v->len--;
    if (out) {
        memcpy(out, v->data + v->len * v->elem, v->elem);
    }
    return true;
}

void *sp_vec_insert(sp_vec *v, size_t i, const void *item)
{
    if (i > v->len || v->len == SIZE_MAX || sp_vec_reserve(v, v->len + 1) != 0) {
        return NULL;
    }
    unsigned char *slot = v->data + i * v->elem;
    memmove(slot + v->elem, slot, (v->len - i) * v->elem);
    memcpy(slot, item, v->elem);
    v->len++;
    return slot;
}

bool sp_vec_remove(sp_vec *v, size_t i)
{
    if (i >= v->len) {
        return false;
    }
    unsigned char *slot = v->data + i * v->elem;
    memmove(slot, slot + v->elem, (v->len - i - 1) * v->elem);
    v->len--;
    return true;
}

bool sp_vec_swap_remove(sp_vec *v, size_t i)
{
    if (i >= v->len) {
        return false;
    }
    if (i != v->len - 1) {
        memcpy(v->data + i * v->elem, v->data + (v->len - 1) * v->elem, v->elem);
    }
    v->len--;
    return true;
}

void sp_vec_sort(sp_vec *v, sp_cmp_fn cmp)
{
    if (v->len > 1) {
        qsort(v->data, v->len, v->elem, cmp);
    }
}

ptrdiff_t sp_vec_bsearch(const sp_vec *v, const void *key, sp_cmp_fn cmp)
{
    size_t lo = 0;
    size_t hi = v->len;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int c = cmp(v->data + mid * v->elem, key);
        if (c == 0) {
            return (ptrdiff_t)mid;
        }
        if (c < 0) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return -1;
}
