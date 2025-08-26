<img src="assets/logo.png" style="height:75px" />

# RipDB — Blazing Fast, Zero‑Copy, Memory‑Mapped B+Tree Database Engine

### Why RipDB

- **Zero‑copy reads**: Data is directly memory‑mapped; `rdb_get` and cursor reads return pointers into the map. No buffers, no memcpy, no malloc during reads.
- **Epic heritage (LMDB)**: Based on the legendary LMDB architecture by Howard Chu. Expect predictable latency, extreme read performance, and obscene scale.
- **ACID with MVCC**: Fully transactional with multi‑version concurrency control. Readers are lock‑free and never block writers; writers never block readers. One writer at a time means no deadlocks.
- **Copy‑on‑write pages**: Writes never overwrite active pages. Crash‑safe without a WAL. No background compaction or GC; free pages are tracked and reused.
- **Simple and tiny**: A clean C API modeled after BerkeleyDB/LMDB, no page cache layer, leverages the OS page cache. Minimal dependencies.

### Core Concepts

- **Environment (`RDB_env`)**: The database system instance. Holds a single memory‑mapped file with one main DB plus optional named sub‑DBs.
- **Transactions (`RDB_txn`)**: All operations occur inside a transaction. Use `RDB_RDONLY` for read‑only transactions; one write transaction globally at a time.
- **Databases (`RDB_dbi`)**: The main DB or named sub‑DBs. Open with `rdb_dbi_open`. Flags include `RDB_CREATE`, `RDB_DUPSORT`, `RDB_INTEGERKEY`, `RDB_DUPFIXED`, `RDB_INTEGERDUP`, `RDB_REVERSEKEY`, `RDB_REVERSEDUP`.
- **Keys/Values (`RDB_val`)**: Struct with `mv_size` and `mv_data`. Returned pointers from reads are valid until the next write in the same txn or until the txn ends.
- **Cursors (`RDB_cursor`)**: High‑performance ordered navigation and batched writes. Supports seek/range scans (`RDB_SET_RANGE`, `RDB_FIRST`, `RDB_LAST`, `RDB_NEXT`, `RDB_PREV`, etc.).

### Highlights

- **Performance**
  - Zero‑copy reads through `mmap(2)`
  - Minimal syscalls; predictable cache behavior via OS page cache
  - Bulk loading via `RDB_APPEND`/`RDB_APPENDDUP`
- **Concurrency**
  - Single global writer; many concurrent readers (MVCC)
  - Readers use snapshots and do not block
- **Safety & durability**
  - Copy‑on‑write B+Tree pages
  - Crash‑safe without WAL; `rdb_env_sync` for explicit fsync
  - Optional `RDB_WRITEMAP` for writable mmap (higher write throughput; lower safety from stray writes)
- **Scalability**
  - 64‑bit addressing on modern OSes; very large datasets
  - No periodic compaction; database reuses free pages

### Quickstart

1) Build the library and tools

```bash
make            # builds build/libripdb.a and build/libripdb.dylib (macOS)
make tools      # builds ripdb_stat, ripdb_dump, ripdb_load, ripdb_copy
```

2) Minimal example (C)

```c
#include <stdio.h>
#include <string.h>
#include "ripdb.h"

int main(void) {
    RDB_env *env; RDB_txn *txn; RDB_dbi dbi; int rc;
    RDB_val key = { .mv_size = 3, .mv_data = "foo" };
    RDB_val val = { .mv_size = 3, .mv_data = "bar" };

    if ((rc = rdb_env_create(&env))) return rc;
    rdb_env_set_mapsize(env, 10485760);              // 10 MB
    if ((rc = rdb_env_open(env, "./db", 0, 0664))) return rc;

    if ((rc = rdb_txn_begin(env, NULL, 0, &txn))) return rc;
    if ((rc = rdb_dbi_open(txn, NULL, 0, &dbi))) return rc;
    if ((rc = rdb_put(txn, dbi, &key, &val, 0))) return rc;
    if ((rc = rdb_txn_commit(txn))) return rc;

    if ((rc = rdb_txn_begin(env, NULL, RDB_RDONLY, &txn))) return rc;
    if ((rc = rdb_get(txn, dbi, &key, &val))) return rc;
    printf("%.*s\n", (int)val.mv_size, (char*)val.mv_data);
    rdb_txn_abort(txn);

    rdb_dbi_close(env, dbi);
    rdb_env_close(env);
    return 0;
}
```

Compile against the static library:

```bash
cc -Iinclude -O2 -pthread -o hello hello.c build/libripdb.a
```

### Build matrix

- `make` builds:
  - `build/libripdb.a` (static)
  - `build/libripdb.dylib` on macOS or `.so` on Linux
- `make tools` builds CLI utilities in `build/`
- `make tests` builds and runs sample tests (writes to `tests/db`)

### CLI tools

- **ripdb_stat**: Inspect env and DB statistics
  - Usage: `ripdb_stat [-V] [-n] [-e] [-r[r]] [-f[f[f]]] [-a|-s subdb] dbpath`
  - Tips: `-e` prints env info; `-r` prints reader table; `-a` iterates sub‑DBs

- **ripdb_dump**: Dump DB content in a simple text format
  - Usage: `ripdb_dump [-V] [-f output] [-l] [-n] [-p] [-a|-s subdb] dbpath`
  - Tips: `-p` printable format; `-a` dump all sub‑DBs; `-s name` a specific sub‑DB

- **ripdb_load**: Load DB content from `ripdb_dump` output or plaintext
  - Usage: `ripdb_load [-V] [-a] [-f input] [-n] [-s name] [-N] [-T] dbpath`
  - Tips: `-T` plaintext mode; `-N` no‑overwrite; `-a` bulk append (fast sorted load)

- **ripdb_copy**: Make a live backup (optionally compacting)
  - Usage: `ripdb_copy [-V] [-c] [-n] srcpath [dstpath]`
  - Tips: `-c` compaction (omit free space); omit `dstpath` to stream to stdout

Round‑trip example:

```bash
./build/ripdb_dump db > dump.txt
./build/ripdb_load -f dump.txt db_loaded
./build/ripdb_copy -c db db_copy
./build/ripdb_stat -e db_copy
```

### Using the C API (overview)

- **Environment**
  - `rdb_env_create`, `rdb_env_open`, `rdb_env_close`
  - Tuning: `rdb_env_set_mapsize`, `rdb_env_set_maxreaders`, `rdb_env_set_maxdbs`
  - Flags: `RDB_NOSUBDIR`, `RDB_RDONLY`, `RDB_WRITEMAP`, `RDB_NOSYNC`, `RDB_MAPASYNC`, `RDB_NOLOCK`, `RDB_NORDAHEAD`, `RDB_NOMEMINIT`
  - Backup: `rdb_env_copy`, `rdb_env_copy2`, `rdb_env_copyfd2`

- **Transactions**
  - `rdb_txn_begin(env, parent, flags, &txn)`, `rdb_txn_commit`, `rdb_txn_abort`
  - Read‑only reuse: `rdb_txn_reset`, `rdb_txn_renew`

- **Databases**
  - `rdb_dbi_open(txn, name, flags, &dbi)`, `rdb_dbi_close`, `rdb_drop`
  - Custom comparators: `rdb_set_compare`, `rdb_set_dupsort`
  - Stats: `rdb_stat`, `rdb_dbi_flags`

- **Data operations**
  - Basic: `rdb_put`, `rdb_get`, `rdb_del`
  - Cursors: `rdb_cursor_open`, `rdb_cursor_get`, `rdb_cursor_put`, `rdb_cursor_del`, `rdb_cursor_count`
  - Cursor ops: `RDB_FIRST`, `RDB_LAST`, `RDB_NEXT`, `RDB_PREV`, `RDB_SET`, `RDB_SET_RANGE`, `RDB_GET_BOTH`, etc.
  - Put flags: `RDB_NOOVERWRITE`, `RDB_NODUPDATA`, `RDB_RESERVE`, `RDB_APPEND`, `RDB_APPENDDUP`, `RDB_MULTIPLE`

### Best practices & caveats

- **Choose a large mapsize up‑front**: `rdb_env_set_mapsize` sets both mmap size and max DB size. Resizing is supported but requires coordination.
- **Single writer, many readers**: Design your app around one writer at a time. Readers are cheap and lock‑free.
- **Avoid long‑lived read transactions**: They pin old pages and can grow the DB. Renew/reset readers frequently.
- **Writable mmap trade‑off**: `RDB_WRITEMAP` can improve write throughput but makes stray pointer writes dangerous. Prefer default read‑only maps for robustness.
- **No remote filesystems**: Use local disks only; remote FS can break locking/mmap semantics.
- **Locks and stale readers**: On crashes, reader entries can linger. Use `rdb_reader_check` or `ripdb_stat -r[r]` to list/clear.
- **Flags that relax durability**: `RDB_NOSYNC`, `RDB_NOMETASYNC`, and `RDB_MAPASYNC` improve throughput at the cost of durability after power loss. Use with care.

### Running tests

```bash
make tests
# Builds test binaries, resets tests/db, runs a suite of cursor/put/get/split tests
```

### License & attribution

- Licensed under the **OpenLDAP Public License 2.8** (see `LICENSE`).
- Derived from LMDB by Howard Chu (Symas) and from Martin Hedenfalk’s B‑Tree work. RipDB follows the same architectural principles that made LMDB fast and reliable.

### Project layout

- `include/ripdb.h` — public API (flags, types, functions, docs)
- `src/ripdb.c` — engine implementation (B+Tree, MVCC, COW, mmap IO)
- `tools/` — CLI utilities: `ripdb_stat`, `ripdb_dump`, `ripdb_load`, `ripdb_copy`
- `tests/` — small programs/exercises that insert, iterate, delete, and stress splits/merges
- `Makefile` — builds libs, tools, and runs tests

### Support

Issues, questions, or performance tuning help? Open a discussion or issue on your project using RipDB and reference this repository.
