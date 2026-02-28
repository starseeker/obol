## Upstream Osmesa Patches

This directory contains patches intended for upstreaming to the standalone
osmesa project (the source of `external/osmesa/`).

Each patch file includes:
- A description of the bug being fixed
- The context/motivation
- The exact diff to apply

### Available patches

| File | Description |
|------|-------------|
| `0001-Fix-MGL-namespace-GetProcAddress-and-glprocs-core-aliases.patch` | Fix `OSMesaGetProcAddress` under `USE_MGL_NAMESPACE`: add `mgl`→`gl` fallback in `_glapi_get_proc_address` and fix 34 OpenGL 1.3 core function entries in the static dispatch table |

### Problem statement for upstream osmesa agent

**Title:** Fix `OSMesaGetProcAddress` name-mangling support and `glprocs.h`
OpenGL 1.3 core function dispatch entries

**Context:**

When OSMesa is built with `USE_MGL_NAMESPACE` (the default in this fork),
all exported GL symbols are renamed from `glFoo` to `mglFoo`. Callers
retrieve extension/proc addresses via `OSMesaGetProcAddress("mglFoo")`.

Two bugs prevent the OpenGL 1.3 core multitexture functions from being
correctly resolved:

**Bug 1 – `src/glapi/glapi.c`**

`_glapi_get_proc_address()` returns `NULL` for any name that does not begin
with `"gl"`. Under `USE_MGL_NAMESPACE` all callers pass `"mgl"` prefixed
names, so they get `NULL` immediately.

Even if the guard were relaxed, the static dispatch table stores function
names WITHOUT the `"mgl"` prefix (e.g., `"glActiveTexture"`), so a direct
lookup of `"mglActiveTexture"` in `find_entry()` finds no match.

**Fix:** extend `_glapi_get_proc_address` to:
1. Accept names that begin with `"mgl"` (not just `"gl"`) when
   `USE_MGL_NAMESPACE` is defined.
2. After the primary static-table lookup fails for an `"mglFoo"` name,
   fall back to stripping the leading `"m"` and retrying with `"glFoo"`.

**Bug 2 – `src/glapi/glprocs.h`**

The `static_functions[]` table has two blocks of entries for the multitexture
functions:

- **ARB extension block** (offsets ~5000s): stores `"glActiveTextureARB"` →
  `mglActiveTextureARB`
- **OpenGL 1.3 core alias block** (offsets ~14000s): stores
  `"glActiveTexture"` → **incorrectly** `mglActiveTextureARB`

The core-alias block (34 entries starting at offset 14879) uses the ARB
function pointer despite the string table containing the non-ARB name at
those offsets.  With `USE_MGL_NAMESPACE` the `NAME_FUNC_OFFSET` macro
expands to:

```c
{ 14879, (_glapi_proc) mglActiveTextureARB, _gloffset_ActiveTextureARB }
```

`OSMesaGetProcAddress("mglActiveTexture")` therefore:
1. Finds no direct match (table key is `"glActiveTexture"`, not
   `"mglActiveTexture"`)
2. Falls back (via Bug 1 fix) to `"glActiveTexture"` → finds the entry →
   returns **`mglActiveTextureARB`** instead of **`mglActiveTexture`**

While `mglActiveTexture` and `mglActiveTextureARB` both dispatch to the same
`_mesa_ActiveTextureARB`, returning the wrong pointer causes problems for
callers that compare the returned address against the known symbol address of
`mglActiveTexture` (e.g., to validate that the correct library was loaded).

**Fix:** change the function name field in all 34 core-alias entries from the
ARB name to the non-ARB name, e.g.:

```c
// Before:
NAME_FUNC_OFFSET(14879, glActiveTextureARB, _gloffset_ActiveTextureARB),
// After:
NAME_FUNC_OFFSET(14879, glActiveTexture,    _gloffset_ActiveTextureARB),
```

The dispatch-offset field (`_gloffset_ActiveTextureARB`) is intentionally
unchanged — the OpenGL 1.3 core function shares the same dispatch slot as the
ARB extension.

**Files to change:**

1. `src/glapi/glapi.c` — modify `_glapi_get_proc_address` (see patch for
   exact diff)
2. `src/glapi/glprocs.h` — fix 34 entries in the second `static_functions[]`
   block, from `NAME_FUNC_OFFSET(14879, …)` through
   `NAME_FUNC_OFFSET(15490, …)` (see patch for exact diff)

See `0001-Fix-MGL-namespace-GetProcAddress-and-glprocs-core-aliases.patch`
for the exact unified diff to apply.
