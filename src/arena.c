#include "sprocket.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct sp_block {
    sp_block *next;
    size_t cap;
    size_t used;
};

#define HEADER_SIZE ((sizeof(sp_block) + 15u) & ~(size_t)15u)
#define DEFAULT_BLOCK 4096u

static unsigned char *block_data(sp_block *b)
{
    return (unsigned char *)b + HEADER_SIZE;
}

static sp_block *block_new(size_t cap)
{
    if (cap > SIZE_MAX - HEADER_SIZE) {
        return NULL;
    }
    sp_block *b = malloc(HEADER_SIZE + cap);
    if (!b) {
        return NULL;
    }
    b->next = NULL;
    b->cap = cap;
    b->used = 0;
    return b;
}

static bool is_pow2(size_t x)
{
    return x != 0 && (x & (x - 1)) == 0;
}

void sp_arena_init(sp_arena *a, size_t block_size)
{
    a->head = NULL;
    a->block_size = block_size ? block_size : DEFAULT_BLOCK;
    a->total = 0;
}

void sp_arena_free(sp_arena *a)
{
    sp_block *b = a->head;
    while (b) {
        sp_block *next = b->next;
        free(b);
        b = next;
    }
    a->head = NULL;
    a->total = 0;
}

void sp_arena_reset(sp_arena *a)
{
    sp_block *b = a->head;
    sp_block *oldest = NULL;
    while (b) {
        sp_block *next = b->next;
        if (next) {
            free(b);
        } else {
            oldest = b;
        }
        b = next;
    }
    a->head = oldest;
    a->total = 0;
    if (oldest) {
        oldest->used = 0;
    }
}

static void *try_block(sp_block *b, size_t size, size_t align)
{
    uintptr_t base = (uintptr_t)block_data(b) + b->used;
    uintptr_t aligned = (base + (align - 1)) & ~(uintptr_t)(align - 1);
    size_t pad = (size_t)(aligned - base);
    if (pad > b->cap - b->used || size > b->cap - b->used - pad) {
        return NULL;
    }
    b->used += pad + size;
    return (void *)aligned;
}

void *sp_arena_alloc(sp_arena *a, size_t size, size_t align)
{
    if (!is_pow2(align)) {
        return NULL;
    }
    if (a->head) {
        void *p = try_block(a->head, size, align);
        if (p) {
            a->total += size;
            return p;
        }
    }
    if (size > SIZE_MAX - align) {
        return NULL;
    }
    size_t need = size + align;
    size_t cap = need > a->block_size ? need : a->block_size;
    sp_block *b = block_new(cap);
    if (!b) {
        return NULL;
    }
    b->next = a->head;
    a->head = b;
    void *p = try_block(b, size, align);
    a->total += size;
    return p;
}

void *sp_arena_zalloc(sp_arena *a, size_t size, size_t align)
{
    void *p = sp_arena_alloc(a, size, align);
    if (p) {
        memset(p, 0, size);
    }
    return p;
}

void *sp_arena_memdup(sp_arena *a, const void *src, size_t size)
{
    void *p = sp_arena_alloc(a, size, 1);
    if (p && size) {
        memcpy(p, src, size);
    }
    return p;
}

char *sp_arena_strdup(sp_arena *a, const char *s)
{
    return sp_arena_memdup(a, s, strlen(s) + 1);
}

char *sp_arena_sprintf(sp_arena *a, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    va_list copy;
    va_copy(copy, ap);
    int n = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (n < 0) {
        va_end(ap);
        return NULL;
    }
    char *out = sp_arena_alloc(a, (size_t)n + 1, 1);
    if (out) {
        vsnprintf(out, (size_t)n + 1, fmt, ap);
    }
    va_end(ap);
    return out;
}

sp_mark sp_arena_mark(const sp_arena *a)
{
    sp_mark m;
    m.block = a->head;
    m.used = a->head ? a->head->used : 0;
    return m;
}

void sp_arena_rewind(sp_arena *a, sp_mark m)
{
    while (a->head && a->head != m.block) {
        sp_block *next = a->head->next;
        free(a->head);
        a->head = next;
    }
    if (a->head) {
        a->head->used = m.used;
    }
    size_t total = 0;
    for (sp_block *b = a->head; b; b = b->next) {
        total += b->used;
    }
    a->total = total;
}

size_t sp_arena_total(const sp_arena *a)
{
    return a->total;
}
