# Mesa/OSMesa Mipmap Generation Bug Fixes

## Background

This document describes memory-management bugs found in Mesa 7.0.4's
`_mesa_generate_mipmap()` function (`src/mesa/main/mipmap.c`) and the
associated `_mesa_init_teximage_fields()` function
(`src/mesa/main/teximage.c`).  The bugs cause memory leaks and potential
null-pointer dereferences on any out-of-memory code path during mipmap
generation.

These bugs were independently discovered while maintaining a downstream
fork of the Mesa OSMesa software renderer.  The patches below apply
cleanly to Mesa 7.0.4 and should carry forward without conflict to later
versions.

---

## Bug 1 — `_mesa_init_teximage_fields`: missing OOM check after `malloc`

### File
`src/mesa/main/teximage.c`

### Function
`_mesa_init_teximage_fields()`

### Problem
`ImageOffsets` is allocated with `malloc()` but the return value is never
checked.  If the allocation fails (even for the typical 4-byte 2-D case),
the subsequent loop immediately dereferences the NULL pointer:

```c
img->ImageOffsets = (GLuint *) malloc(depth * sizeof(GLuint));
/* BUG: if malloc returns NULL the loop below crashes */
for (i = 0; i < depth; i++) {
    img->ImageOffsets[i] = i * width * height;
}
```

### Fix
Check the return value and return early on failure.  All callers that
need `ImageOffsets` to be non-NULL already have to handle partial
initialisation, and the call site in `_mesa_generate_mipmap()` is
updated below to propagate the error correctly.

```diff
-    img->ImageOffsets = (GLuint *) malloc(depth * sizeof(GLuint));
-    for (i = 0; i < depth; i++) {
-        img->ImageOffsets[i] = i * width * height;
-    }
+    img->ImageOffsets = (GLuint *) malloc(depth * sizeof(GLuint));
+    if (!img->ImageOffsets) {
+        /* Caller must check for this failure */
+        return;
+    }
+    for (i = 0; i < depth; i++) {
+        img->ImageOffsets[i] = i * width * height;
+    }
```

---

## Bug 2 — `_mesa_generate_mipmap`: missing NULL-out and cleanup on OOM paths

### File
`src/mesa/main/mipmap.c`

### Function
`_mesa_generate_mipmap()`

### Problem A — dangling pointer after `free(dstImage->ImageOffsets)`

```c
if (dstImage->ImageOffsets)
    free(dstImage->ImageOffsets);
/* ImageOffsets is now freed but still holds the old address */
```

The pointer is not set to NULL after being freed.  `_mesa_init_teximage_fields()`
is called immediately after and unconditionally overwrites the field, so
no double-free can occur here in practice, but the pattern is fragile
and conceals the OOM failure described below.

### Problem B — `dstImage->ImageOffsets` leaked on OOM

After `_mesa_init_teximage_fields()` successfully allocates
`dstImage->ImageOffsets`, two subsequent `_mesa_alloc_texmemory()` calls
may fail.  Both error-return paths leave `dstImage->ImageOffsets` allocated
but unreachable:

**Compressed texture OOM path (was):**
```c
dstImage->Data = _mesa_alloc_texmemory(dstImage->CompressedSize);
if (!dstImage->Data) {
    _mesa_error(ctx, GL_OUT_OF_MEMORY, "generating mipmaps");
    if (srcData) free((void *)srcData);
    if (dstData) free(dstData);
    return;   /* BUG: dstImage->ImageOffsets is leaked */
}
```

**Uncompressed texture OOM path (was):**
```c
dstImage->Data = _mesa_alloc_texmemory(dstWidth * dstHeight
                                       * dstDepth * bytesPerTexel);
if (!dstImage->Data) {
    _mesa_error(ctx, GL_OUT_OF_MEMORY, "generating mipmaps");
    return;   /* BUG: dstImage->ImageOffsets is leaked */
}
```

### Problem C — no check for `_mesa_init_teximage_fields` OOM failure

Because `_mesa_init_teximage_fields()` previously had no OOM check
(Bug 1), callers could never detect the failure.  After fixing Bug 1,
`_mesa_generate_mipmap()` must check whether `ImageOffsets` was actually
allocated before proceeding.

### Fix

```diff
-    if (dstImage->ImageOffsets)
-        free(dstImage->ImageOffsets);
+    if (dstImage->ImageOffsets) {
+        free(dstImage->ImageOffsets);
+        dstImage->ImageOffsets = NULL;
+    }
 
     /* Free old image data */
     if (dstImage->Data)
         ctx->Driver.FreeTexImageData(ctx, dstImage);
 
     /* initialize new image */
     _mesa_init_teximage_fields(ctx, target, dstImage, dstWidth, dstHeight,
                                dstDepth, border, srcImage->InternalFormat);
+    if (!dstImage->ImageOffsets) {
+        /* _mesa_init_teximage_fields failed to allocate ImageOffsets */
+        _mesa_error(ctx, GL_OUT_OF_MEMORY, "generating mipmaps");
+        if (srcImage->IsCompressed) {
+            if (srcData) free((void *)srcData);
+            if (dstData) free(dstData);
+        }
+        return;
+    }
     dstImage->DriverData = NULL;
     /* ... rest of field setup ... */
 
     if (dstImage->IsCompressed) {
         dstImage->Data = _mesa_alloc_texmemory(dstImage->CompressedSize);
         if (!dstImage->Data) {
             _mesa_error(ctx, GL_OUT_OF_MEMORY, "generating mipmaps");
             if (srcData) free((void *)srcData);
             if (dstData) free(dstData);
+            free(dstImage->ImageOffsets);
+            dstImage->ImageOffsets = NULL;
             return;
         }
     } else {
         /* ... */
         dstImage->Data = _mesa_alloc_texmemory(dstWidth * dstHeight
                                                * dstDepth * bytesPerTexel);
         if (!dstImage->Data) {
             _mesa_error(ctx, GL_OUT_OF_MEMORY, "generating mipmaps");
+            free(dstImage->ImageOffsets);
+            dstImage->ImageOffsets = NULL;
             return;
         }
     }
```

---

## Summary of changes

| File | Function | Change |
|------|----------|--------|
| `src/mesa/main/teximage.c` | `_mesa_init_teximage_fields` | Add NULL check after `malloc(ImageOffsets)` |
| `src/mesa/main/mipmap.c` | `_mesa_generate_mipmap` | NULL-out `ImageOffsets` after free; check for OOM from `_mesa_init_teximage_fields`; free `ImageOffsets` on both OOM return paths |

These changes are strictly additive error-handling improvements.  They do
not alter any normal (non-OOM) code path and have no effect on rendering
correctness or performance.
