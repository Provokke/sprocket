<div align="center">

# sprocket

A small C11 toolkit: a block arena allocator, an open-addressing hash map and a growable array, with no dependencies beyond libc. I wrote it because I kept re-typing the same three pieces in every C project.

![License](https://img.shields.io/badge/LICENSE-MIT-blue?style=for-the-badge)  ![Language](https://img.shields.io/badge/C11-555555?style=for-the-badge&logo=c&logoColor=white)  ![Tests](https://img.shields.io/badge/TESTS-41-brightgreen?style=for-the-badge)

Refer to [LICENSE](LICENSE) for the legal bits.

</div>

## What's in it

- **Arena** (`sp_arena`): a chain of malloc'd blocks you bump-allocate from.
  - Any power-of-two alignment, zeroing allocation, `memdup`, `strdup` and a `printf`-style helper that formats straight into the arena.
  - Requests bigger than a block get a block of their own.
  - `sp_arena_mark` / `sp_arena_rewind` give you cheap scratch space, and `sp_arena_reset` throws everything away but keeps one block for reuse.
  - Overflowing sizes and bad alignments return `NULL` instead of wrapping.
- **Hash map** (`sp_map`): open addressing with linear probing and a 75% load limit.
  - Keys are arbitrary byte strings (embedded zeros are fine) and are copied in. Values are fixed-size and stored inline, so a value size of 0 gives you a set.
  - Deletion uses backward shifting rather than tombstones, so lookups never slow down after heavy churn.
  - Iteration with `sp_map_next`, `reserve`, `clear`, plus `_str` convenience wrappers.
- **Vector** (`sp_vec`): a type-erased growable array with push, pop, extend, insert, ordered remove, swap-remove, sort, binary search, reserve and shrink. Every size calculation is checked for overflow.
- **Test runner** (`tests/`): a tiny self-registering runner in about 60 lines. Tests register themselves with a constructor attribute, so adding one is a single `TEST(name) { ... }` block.

```c
#include "sprocket.h"

sp_arena arena;
sp_arena_init(&arena, 0);

sp_map counts;
sp_map_init(&counts, sizeof(int));

const char *words[] = {"to", "be", "or", "not", "to", "be"};
for (size_t i = 0; i < 6; i++) {
    int *n = sp_map_get_str(&counts, words[i]);
    int one = 1;
    if (n) {
        (*n)++;
    } else {
        sp_map_put_str(&counts, words[i], &one);
    }
}

char *label = sp_arena_sprintf(&arena, "%zu distinct words", sp_map_count(&counts));

sp_map_free(&counts);
sp_arena_free(&arena);
```

## 🏗️ Architecture

```
 include/sprocket.h      one public header
 src/
  ├─ arena.c   sp_arena ── head ─▶ block ─▶ block ─▶ block   (newest first)
  │                          └─ bump pointer per block, mark/rewind frees newer blocks
  ├─ vec.c     sp_vec   ── data ─▶ [ e0 | e1 | e2 | ... spare capacity ]
  └─ map.c     sp_map   ── slots ─▶ [ hash | key* | key_len | used ] x cap
                          vals  ─▶ [ v0 | v1 | v2 | ... ]            x cap
 tests/
  ├─ sptest.h  TEST / CHECK macros, constructor registration
  ├─ main.c    runner, optional name filter
  └─ test_*.c  arena, vec, map
```

- Arena blocks carry a 16 byte aligned header, and the bump pointer rounds up to the requested alignment per allocation.
- The map keeps slot metadata and values in two parallel arrays, which keeps probing cache friendly when values are big.
- Hashing is FNV-1a with an extra mix step so that low bits are usable as a table index.
- Removal walks the probe chain after the hole and pulls back any entry whose ideal position allows it.

## 🚀 Quick start

```
make test       # plain build, runs the suite
make sanitize   # same suite under AddressSanitizer and UBSan
make clean
```

The runner takes an optional substring to filter by name:

```
./build/sprocket_tests map_remove
```

I ran everything in Docker with `gcc:14`:

```
docker run --rm -v "$PWD":/src -w /src gcc:14 make sanitize
```

Output ends with:

```
41 tests, 41 passed, 0 failed
```

## 🧪 Testing

41 tests, all passing, and clean under `-fsanitize=address,undefined` with `-Werror`:

- Arena (14): alignment for every power of two up to 64, rejected alignments, growth over many blocks, oversized requests, zeroing, `strdup`/`memdup`/`sprintf`, the `SP_NEW` macros, mark and rewind, reset, zero-sized and absurdly huge requests.
- Vector (12): push and index, zero-filled push, pop, extend, insert, ordered and swap removal, sort plus binary search on 500 pseudo-random values, reserve and shrink, overflow refusal, struct elements, double free.
- Map (15): put, get and overwrite, key copying, binary and empty keys, growth to 5000 entries, removal and probe-chain integrity, a 20,000 operation randomised run checked against a reference model, iteration, clear, reserve, zero-sized values as a set, struct values and hash stability.

