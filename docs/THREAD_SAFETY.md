# Thread Safety Analysis for Obol

## Overview

Obol inherits its scene-graph architecture from Coin3D, which was historically
documented as not thread safe for general use.  This document provides a deep
analysis of *why* Obol is not thread safe today, categorises every blocker,
assesses whether thread safety is theoretically achievable, and outlines the
work required to reach that goal.

The short answer is: **complete thread safety is achievable, but it requires
significant and carefully sequenced work across several independent subsystems.**
A number of subsystems already carry locking scaffolding (guarded by
`OBOL_THREADSAFE`) — the remaining gaps are well-defined and fixable.

---

## Definitions

The analysis uses the following threat model:

| Scenario | Description |
|---|---|
| **Init-then-read** | `SoDB::init()` is called once on thread A; afterwards multiple threads only *read* the scene graph (no mutations). |
| **Concurrent render** | Multiple threads each own an independent GL context and traverse independent scene graphs simultaneously. |
| **Concurrent mutation** | Multiple threads create, modify, or connect scene-graph nodes and fields simultaneously. |
| **Mixed read/write** | One or more threads traverse while others mutate. |

The strongest guarantee Obol should eventually offer is *Mixed read/write* with
`SoDB::readlock()` / `SoDB::writelock()` protecting mutations, and *Concurrent
render* without any application-level locking when each thread owns its own GL
context.

---

## Current State: What Obol Already Does

Several subsystems already have correct or near-correct locking, always compiled
in when `OBOL_THREADSAFE` is defined (the default, per `CMakeLists.txt`).

### 1. Scene-graph traversal — `SoDB` read/write mutex

`src/misc/SoDBP.h` declares a global `SbRWMutex * globalmutex` that is
allocated in `SoDB::init()` when `OBOL_THREADSAFE` is set.  `SoAction::apply()`
always calls `SoDB::readlock()` before traversing and `SoDB::readunlock()`
afterward.  Write-locking is left to the application when mutating the graph.

*Status:* Correct for the single-scene-graph case; does **not** protect other
global state described below.

### 2. Field notification — global recursive mutex

`src/threads/recmutex_global_cxx17.cpp` provides two process-wide recursive
mutexes, one for field operations and one for notification chains:

```
cc_recmutex_cxx17_field_lock()   / cc_recmutex_cxx17_field_unlock()
cc_recmutex_cxx17_notify_lock()  / cc_recmutex_cxx17_notify_unlock()
```

`SoField` uses `SOFIELD_RECLOCK` / `SOFIELD_RECUNLOCK` (which map to the field
mutex) around value operations.  `SoDB::startNotify()` / `SoDB::endNotify()`
acquire the notify mutex around notification traversals.

*Status:* Protects the hot path of field evaluation and notification.  See the
gaps listed in §5 below.

### 3. Sensor queues — per-queue `SbMutex`

`SoSensorManager` allocates four separate `SbMutex` objects (timer, delay,
immediate, reschedule) and uses `LOCK_*_QUEUE` / `UNLOCK_*_QUEUE` macros around
every enqueue and dequeue operation.

*Status:* Correct.

### 4. GL texture cache — shared `SbMutex`

`SoGLImageP::mutex` is a single shared mutex that serialises all
`getGLDisplayList()` calls across all `SoGLImage` instances.  `SoGLBigImage`
uses a per-instance `SbMutex`.

*Status:* Correct within each class; does not protect the unrelated GL context
list described below.

### 5. Per-`SoSeparator` render-cache mutex

Each `SoSeparator` private implementation carries an `SbMutex` that serialises
`SoGLCacheList` creation and invalidation.  GL cache storage is per-thread via
`SbStorage`, so caches themselves are thread-local.

*Status:* Correct.

### 6. Normal-cache mutex — `SoVertexShape`

`SoVertexShapeP::normalcachemutex` is a global `SbRWMutex` protecting shared
generation of vertex normals.

*Status:* Correct.

### 7. Thread-local state — `SbStorage` / `thread_local`

`SbStorage` (backed by `std::unordered_map` and a `std::shared_mutex` registry)
provides per-thread data.  Its registry is correctly protected.  Several
subsystems use it for per-thread GL cache pointers, big-image tiles, color
packers, texture-coordinate data, and copy-map dictionaries.  `SoGL.cpp`
declares a `thread_local` GL-context pointer for the current render context.

*Status:* Correct.

### 8. `SoProtoInstance` and `CoinTidbits` atexit list

Both use a `std::mutex` (unconditionally) around their respective global
structures.

*Status:* Correct.

### 9. Debug thread check — `OBOL_CHECK_THREAD`

When the CMake option `OBOL_DEBUG_CHECK_THREAD` is enabled, `OBOL_CHECK_THREAD()`
compares `std::this_thread::get_id()` against the thread that called
`SoDB::init()` and logs a `SoDebugError` if they differ.  This is a diagnostic
tool, not a safety mechanism; it emits a warning but does not prevent the race.

*Status:* Useful for development; not a fix.

---

## Current State: What Is Still Broken

The following subsystems have either no locking or insufficient locking.

### Blocker 1 — `SbName` / `namemap`: unprotected string interning *(CRITICAL)*

**File:** `src/base/namemap.cpp`

`SbName` is used pervasively — every type name, field name, and node name goes
through it.  Internally, `namemap_find_or_add_string()` operates on two global,
unprotected data structures:

```c
static NamemapBucketEntry ** nametable;  // hash table of name → entry
static NamemapMemChunk     * headchunk;  // slab allocator for strings
```

Two threads calling `SbName::SbName(const char*)` simultaneously can:

- Corrupt the linked-list buckets (`entry->next = nametable[i]; nametable[i] = entry;` is not atomic).
- Corrupt the slab allocator (both threads decrement `bytesleft` and advance `curbyte` without synchronization, causing overlapping writes).
- Create duplicate entries (both threads observe the entry as absent and insert it twice).

Because `SbName` is used to look up types, field names, and node names, any
corruption here can silently produce wrong type matches or crashes anywhere in
the library.

**Fix required:** Add a `std::mutex` (or a `std::shared_mutex` for read-heavy
loads) protecting `namemap_find_or_add_string()`.  Because `cc_namemap_peek_string()`
is read-only, it qualifies for a shared (reader) lock.

---

### Blocker 2 — `SoType` registration: unprotected type dictionary *(CRITICAL)*

**File:** `src/misc/SoType.cpp`

The type system manages three global data structures, none protected by any mutex:

```cpp
static Name2IdMap     * type_dict    = NULL;  // name → type index
static Name2HandleMap * module_dict  = NULL;  // for dynamic loading
SbList<SoTypeData *>  * typedatalist = NULL;  // indexed type data (class member)
```

`SoType::createType()` performs a read followed by a conditional append:

```cpp
if (type_dict->get(name, discard)) { /* already registered */ return; }
newType.index = SoType::typedatalist->getLength();   // read length
SoType::typedatalist->append(typeData);              // append
type_dict->put(name, newType.getKey());              // insert
```

Two threads registering different types simultaneously can corrupt both
`typedatalist` (wrong indices) and `type_dict` (duplicate or missing entries).
Dynamic loading in `internal_fromName()` compounds this: it touches `module_dict`
and falls through into `createType()` with no lock held.

**Fix required:** A single `std::mutex` protecting all of `createType()`,
`deregisterType()`, `internal_fromName()`, and any `typedatalist` access.
Type lookups (`fromName()`, `getAllDerivedFrom()`) need at least a shared lock.

---

### Blocker 3 — `SoBase` reference count: non-atomic bit-field *(CRITICAL)*

**File:** `include/Inventor/misc/SoBase.h`

The reference count is stored in a bit-packed struct:

```cpp
struct {
  mutable signed int  referencecount : 28;
  mutable unsigned int alive         :  4;
} objdata;
```

`ref()` and `unref()` perform non-atomic read-increment-write sequences:

```cpp
// SoBase.cpp, SoBase::ref():
this->objdata.referencecount++;
```

```cpp
// SoBase.cpp, SoBase::unref():
this->objdata.referencecount--;
int refcount = this->objdata.referencecount;
if (refcount == 0) base->destroy();
```

The classic lost-update race applies: two threads calling `ref()` on the same
object simultaneously can both read the same value, both increment it, and write
back the same incremented value — dropping a reference count.  For `unref()`,
both threads can read a count of 1, both decrement to 0, and both call
`destroy()`, causing a double-free.

The C++ standard does not guarantee atomic access to bit-fields, even on
architectures with native word-size operations.

**Fix required:** The bit-packing cannot be preserved — C++ provides no atomic
operations on bit-fields.  The replacement trades 4 bytes per `SoBase` instance
for correctness: replace `referencecount:28` with a standalone
`std::atomic<int32_t>` (4 bytes), and promote `alive:4` to a separate
`std::atomic<uint8_t>` or a plain `uint8_t` protected by a compare-exchange on
the reference count.  `SoBase` instances are heap-allocated and already carry a
virtual-function table pointer (8 bytes on 64-bit), so the 4-byte growth of the
`objdata` struct is not a practical concern.  Use `std::memory_order_acq_rel` for
increment/decrement and `std::memory_order_acquire` for the zero-check.

---

### Blocker 4 — `SoNode::nextUniqueId`: non-atomic global counter *(CRITICAL)*

**File:** `src/nodes/SoNode.cpp`

Every node creation, modification, and copy stamps a unique ID via:

```cpp
SbUniqueId SoNode::nextUniqueId = 1;  // global, unprotected

#define SET_UNIQUE_NODE_ID(obj)                   \
  (obj)->uniqueId = SoNode::nextUniqueId++;       \
  if (obj->uniqueId == 0) {                       \
    obj->uniqueId = SoNode::nextUniqueId++;       \
  }
```

`SbUniqueId` is a plain integer type (`uint32_t` or `uint64_t`).
`nextUniqueId++` is **not** atomic in C++.  Two threads creating nodes
simultaneously can receive the same unique ID, which breaks the change-detection
caches (VBO validation, bounding-box caches, etc.) used throughout rendering.

`SbUniqueId` is platform-configurable: `uint64_t` by default, or `uint32_t`
when `OBOL_UNIQUE_ID_UINT32` is set (see `include/Inventor/basic.h.cmake.in`).
Both widths are natively lock-free atomics on all platforms Obol targets.

**Fix required:** Change `nextUniqueId` to `std::atomic<SbUniqueId>` and use
`fetch_add(1, std::memory_order_relaxed)` (ordering is not required for ID
uniqueness — only for the zero-skip check, which can use a local compare).
The fix is identical for both the 32-bit and 64-bit variants because
`std::atomic<uint32_t>` and `std::atomic<uint64_t>` are both always lock-free
on x86-64 and ARM64.

---

### Blocker 5 — `SoBase::PImpl` global name/auditor dictionaries ✅ **(Fixed)**

**File:** `src/misc/SoBaseP.cpp`, `src/misc/SoBaseP.h`

Four global data structures were accessed without synchronization.  All four
are now protected by `SoBase::PImpl::base_dict_mutex` (`std::shared_mutex`)
declared in `SoBaseP.h` and defined in `SoBaseP.cpp`.  Shared (read) lock for
`getName()`, `getNamedBase()`, `getNamedBases()`; exclusive (write) lock for
`setName()`, `getAuditors()`, and the constructor/destructor tracking.

---

### Blocker 6 — `SoGLCacheContextElement` static state ✅ **(Fixed)**

**File:** `src/elements/GL/SoGLCacheContextElement.cpp`

`biggest_cache_context_id` is now `std::atomic<int>` with CAS-loop updates in
`set()` and `fetch_add(1)` in `getUniqueCacheContext()`.  A `std::mutex
glcache_list_mutex` protects all three `SbList` containers.

---

### Blocker 7 — `SoDBP::notificationcounter`: non-atomic plain `int` ✅ **(Fixed in Phase 1)**

**File:** `src/misc/SoDBP.h` and `src/misc/SoDB.cpp`

Changed to `std::atomic<int>` in Phase 1.

---

### Blocker 8 — `SoActionMethodList` and `SoEnabledElementsList`: conditional locking ✅ **(Fixed)**

**Files:** `src/lists/SoActionMethodList.cpp`, `src/lists/SoEnabledElementsList.cpp`

`SoActionMethodList` now uses `std::shared_mutex` (removing `OBOL_THREADSAFE`
guard).  A new `getMethod(int)` function provides a shared-lock read path;
`SoAction::traverse()` uses it instead of `operator[]` for the traversal lookup.
`SoEnabledElementsList` uses `std::recursive_mutex` for per-instance locking
and `std::atomic<int>` for `enable_counter`; `enable()` and `merge()` now take
the per-instance lock.

---

### Blocker 9 — `SoAuditorList` iteration during concurrent modification *(MEDIUM)*

**File:** `src/lists/SoAuditorList.cpp`

`SoAuditorList` is the linked notification mechanism between fields, sensors,
and nodes.  It is protected by the field recursive mutex when accessed through
`SoField`.  However, `SoBase::addAuditor()` and `SoBase::removeAuditor()` in
`src/misc/SoBase.cpp` reach the list through the unprotected `auditordict` in
`SoBaseP.cpp` (Blocker 5), so the mutex in the field path does not cover all
entry points.

Additionally, notification delivery iterates the `SoAuditorList` without
holding a lock that prevents concurrent structural modifications (additions or
removals during iteration).  A sensor callback that connects or disconnects
a field can trigger a re-entrancy that modifies the list being walked.

**Fix required:** Consistent lock ownership for `SoAuditorList` access, plus
snapshot-iteration (copy-on-notify) if re-entrant modification is expected.

---

### Blocker 10 — `SbHash<>` and `SbList<>` are not thread-safe containers *(PERVASIVE)*

**Files:** `src/misc/SbHash.h`, `include/Inventor/lists/SbList.h`

`SbHash` is an open-addressing hash map backed by a plain C array.  `SbList` is
a growable array backed by a `new[]`-allocated block.  Neither provides any
internal synchronization.  They are used as containers for the global state in
blockers 1–6 above.

Fixing the individual blockers (by wrapping access with appropriate mutexes) is
sufficient; replacing these containers with lock-free or concurrent variants is
not required but would improve scalability.

---

## Design-level Issues: Not Easily Fixed by Locking Alone

The following problems require design changes, not just added mutexes.

### D1 — OpenGL's per-context single-thread model

OpenGL contexts are implicitly associated with exactly one thread at a time
(`glXMakeCurrent`, `wglMakeCurrent`).  Obol currently has no mechanism to
enforce or detect cross-thread GL-context access.  `SoGLCacheContextElement`
tracks which context an `SoGLDisplayList` was created in, but there is no
check that the thread issuing GL calls is the thread that owns the current
context.

This is not a deficiency that can be corrected with mutexes.  The correct
architecture is: **each rendering thread owns exactly one GL context, and GL
objects are never migrated between contexts.**  Obol's existing per-thread GL
cache storage (`SbStorage` in `SoSeparator`) is already aligned with this
model; the remaining work is documentation and assertion enforcement.

### D2 — Re-entrant notification during traversal

`SoDB::startNotify()` acquires the notify recursive mutex, then walks the
auditor list, which can trigger sensor callbacks, which in turn may modify
fields, which calls `startNotify()` again (allowed by the recursive mutex).
This re-entrancy is by design but makes it impossible to snapshot the auditor
list at the start of a notification pass — mutations during delivery are
immediately visible to the in-progress walk.

Full thread safety requires either:

1. **Snapshot-then-deliver**: Copy the auditor list under lock, release the
   lock, deliver to the snapshot.  Auditors removed between snapshot and
   delivery must be handled gracefully (weak-reference or validity check).
2. **Deferred mutation**: Queue auditor add/remove operations and apply them
   after the current notification pass completes.

### D3 — `SoState` per-action, not per-thread

`SoState` is created and owned by a single `SoAction` instance and carries all
element stacks.  It is inherently single-threaded: no part of the traversal
machinery assumes concurrent access to the same `SoState`.

For *Concurrent render* (multiple threads each traversing their own independent
scene graph), this is already safe because each thread creates its own action
and thus its own `SoState`.  This design constraint simply means that one
`SoAction` cannot be shared between threads, which is documented behaviour.

### D4 — Static `SbStorage` across lifetime boundaries

`SbStorage` registers all per-thread storage objects in a global registry so
that thread-exit cleanup can walk all live storages and release each thread's
slot.  The registry itself is protected by a `std::shared_mutex` (see
`src/threads/storage_cxx17.h`).  However, `cc_storage_apply_to_all()` (used by
`SoGLBigImage`) iterates all threads' data without acquiring per-thread
lifetimes.  If a thread exits while this iteration is in progress, its storage
slot is freed concurrently.

**Fix required:** Either take a strong reference to the thread-local data
structure before iterating, or replace `apply_to_all` with a snapshot operation
under the registry lock.

---

## Summary Table

| # | Subsystem | File(s) | Severity | Existing Lock | Gap |
|---|---|---|---|---|---|
| 1 | `SbName` interning | `base/namemap.cpp` | **CRITICAL** | None | Needs `std::mutex` |
| 2 | `SoType` registration | `misc/SoType.cpp` | **CRITICAL** | None | Needs `std::mutex` |
| 3 | `SoBase` refcount | `misc/SoBase.h/.cpp` | **CRITICAL** | None | Needs `std::atomic` |
| 4 | `SoNode::nextUniqueId` | `nodes/SoNode.cpp` | **CRITICAL** | None | Needs `std::atomic` |
| 5 | `SoBase::PImpl` dicts | `misc/SoBaseP.cpp/.h` | **CRITICAL** | None | Needs `std::mutex` |
| 6 | GL context element statics | `elements/GL/SoGLCacheContextElement.cpp` | **HIGH** | None | Needs `std::atomic` + `std::mutex` |
| 7 | Notification counter | `misc/SoDBP.h/.cpp` | **MEDIUM** | Recursive mutex (partial) | Needs `std::atomic` |
| 8 | Action/element method lists | `lists/SoAction*.cpp`, `lists/SoEnabled*.cpp` | **MEDIUM** | `SbRWMutex` (partial) | Needs consistent read lock on lookups |
| 9 | `SoAuditorList` iteration | `lists/SoAuditorList.cpp`, `misc/SoBase.cpp` | **MEDIUM** | Field recmutex (partial) | Needs snapshot-iterate or consistent lock |
| 10 | `SbHash` / `SbList` | `misc/SbHash.h`, `lists/SbList.h` | **PERVASIVE** | None | Protected by caller locks in blockers above |
| D1 | GL context / thread binding | `elements/GL/SoGLCacheContextElement.cpp` | Design | None | Per-thread GL context convention + assertions |
| D2 | Re-entrant notification | `misc/SoDB.cpp`, `fields/SoField.cpp` | Design | Recursive mutex | Snapshot-then-deliver or deferred mutation |
| D3 | `SoState` per-action | Traversal machinery | Design | N/A | Document: do not share actions between threads |
| D4 | `cc_storage_apply_to_all` | `threads/storage_cxx17.*` | Design | Registry `shared_mutex` | Snapshot under lock before iteration |

---

## Recommended Migration Strategy

The blockers fall into three natural phases, ordered by dependency and risk.

### Phase 1 — Eliminate data races in global infrastructure (Weeks 1–3)

These changes are prerequisite for everything else; they do not require design
changes and have well-understood fixes.

1. **`SoNode::nextUniqueId` → `std::atomic<SbUniqueId>`** (1 hour)
   Zero application impact.  Pure mechanical change.

2. **`SbName`/`namemap` mutex** (half a day)
   Add a `static std::mutex namemap_mutex` inside `namemap.cpp`.  Lock it for
   the duration of `namemap_find_or_add_string()`.  Use a
   `std::shared_mutex` if profiling shows contention.

3. **`SoBase` refcount → `std::atomic<int32_t>`** (1 day)
   Requires splitting `objdata` into a separate `alive` marker (a 1-byte
   sentinel, e.g. a `bool` or enum).  Change `ref()`, `unref()`, and
   `unrefNoDelete()` to use `fetch_add`/`fetch_sub` with appropriate memory
   ordering.  Keep the overflow check.

4. **`SoType` registration mutex** (1 day)
   Add a `static std::shared_mutex type_mutex` in `SoType.cpp`.  Writer-lock
   in `createType()` and `deregisterType()`; reader-lock in `fromName()`,
   `getAllDerivedFrom()`, etc.

5. **`SoDBP::notificationcounter` → `std::atomic<int>`** (2 hours)
   Mechanical.  Keep the existing notify recursive mutex for serialising the
   notification pass; the atomic counter only needs relaxed ordering for the
   increment/decrement and acquire/release for the zero-check.

### Phase 2 — Protect global object registries ✅ **(Complete)**

6. **`SoBase::PImpl` dicts** — added `static std::shared_mutex base_dict_mutex`
   in `SoBaseP.h`/`SoBaseP.cpp`.  Shared (read) lock for `getName()`,
   `getNamedBase()`, `getNamedBases()`; exclusive (write) lock for `setName()`
   (reads old name directly from `obj2name` to avoid recursive lock),
   `getAuditors()` (lazy-initialises `auditordict` entries), and `allbaseobj`
   / `auditordict` tracking in the constructor and destructor.

7. **`SoGLCacheContextElement` statics** — `std::atomic<int>` for
   `biggest_cache_context_id`; `std::mutex glcache_list_mutex` around all
   three `SbList` operations (`extsupportlist`, `scheduledeletelist`,
   `scheduledeletecblist`).  `set()` uses a CAS loop; `getUniqueCacheContext()`
   uses `fetch_add(1)`.

8. **`SoActionMethodList` / `SoEnabledElementsList`** — `SoActionMethodList`
   upgraded from `SbMutex` to `std::shared_mutex` (removing `OBOL_THREADSAFE`
   guard); added `getMethod(int)` with shared lock for the traversal read path;
   `SoAction::traverse()` updated to use `getMethod()`.
   `SoEnabledElementsList` uses `std::recursive_mutex` (allowing re-entrant
   calls from `getElements()` → `merge()` → `enable()`); `enable_counter`
   changed to `static std::atomic<int>`; explicit locks added to `enable()`
   and `merge()`.

### Phase 3 — Design-level work (Weeks 5–10)

9. **`SoAuditorList` snapshot iteration** — introduce a copy-on-notify
   strategy or a deferred-remove list to make notification delivery safe when
   callbacks modify the auditor list.  This is the most complex change and
   should be gated on a comprehensive test suite for field notification.

10. **`cc_storage_apply_to_all` — snapshot under lock** — take a copy of all
    active storage pointers under the registry lock before iterating.

11. **GL context/thread binding assertions** — add `assert` or
    `SoDebugError::post` checks in `SoGLRenderAction::apply()` and
    `SoSceneTexture2` ensuring the calling thread is the one that created the
    GL context.

12. **Remove `OBOL_THREADSAFE` conditionals** — once all blockers are fixed,
    the conditional locking scaffolding should be replaced with unconditional
    code.  Performance-critical paths (field evaluation, sensor queuing) can
    be profiled and optimised with lock-free data structures if contention is
    measured.

### Phase 4 — Validation (Weeks 10–12)

- Run the existing test suite under ThreadSanitizer (TSan): `cmake … -DCMAKE_CXX_FLAGS="-fsanitize=thread"`.
- Add targeted multi-threaded tests: concurrent `SbName` construction,
  concurrent node creation, parallel rendering of independent scene graphs,
  concurrent field connection/disconnection under notification.
- Document the resulting thread-safety guarantees in the public API
  documentation.

---

## Conclusion

Obol is **not currently thread safe** for concurrent mutation or concurrent
rendering of a shared scene graph.  The blockers are:

- Four *critical* data races on truly global state (name map, type table, node
  ID counter, reference count) that can produce corruption with no locking at
  all.
- Two more *critical* races in global object registries that undermine the field
  notification system.
- Several *medium-severity* gaps in existing, partially-correct locking.
- Three design-level issues that require code restructuring rather than
  added mutexes.

None of these are insurmountable.  The infrastructure for thread safety
(mutex wrappers, thread-local storage, recursive mutexes for notification) is
already present.  The critical path races (#1–#5 above) are each small,
isolated changes.  The design-level work (#D1–#D4) is more involved but
bounded.

The recommended approach is to fix the critical blockers first (Phase 1,
primarily mechanical changes), then the registries (Phase 2), then invest in
the design-level work (Phase 3), and finally harden with TSan-driven testing
(Phase 4).  The entire effort is estimated at **8–12 engineering weeks** for
a small team already familiar with the codebase.
