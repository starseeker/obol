# OSMesa GLSL Compiler Bug: `gl_TextureMatrix[n]` subscript access fails with "Error: invalid assignment"

**Affects:** Mesa 7.0.4 OSMesa (slang GLSL compiler)  
**Component:** `src/shader/slang/slang_codegen.c`, `slang_emit.c`  
**Severity:** Any GLSL vertex shader that references `gl_TextureMatrix[n]` (with any constant
integer index `n`) fails to compile, including the standard pattern
`gl_TexCoord[n] = gl_TextureMatrix[n] * gl_MultiTexCoordN` used by real-world shadow and
multi-texture shaders.

---

## Minimal Self-Contained Reproducer

The following C program can be compiled against any Mesa 7.0.4 OSMesa build and reproduces
the bug without any dependency on obol or Coin3D.

```c
/*
 * osmesa_texmatrix_bug.c
 *
 * Demonstrates that gl_TextureMatrix[n] subscript access fails to compile
 * in Mesa 7.0.4 OSMesa's slang GLSL compiler, while superficially similar
 * patterns (direct matrix identifiers, user-defined uniforms) succeed.
 *
 * Build (with MGL name mangling, as used by the starseeker/osmesa submodule):
 *
 *   gcc -DUSE_MGL_NAMESPACE \
 *       -I/path/to/osmesa/include \
 *       osmesa_texmatrix_bug.c \
 *       /path/to/libosmesa.so \
 *       -Wl,-rpath,/path/to/ \
 *       -o osmesa_texmatrix_bug
 *
 * Without name mangling (system OSMesa):
 *
 *   gcc -I/usr/include \
 *       osmesa_texmatrix_bug.c \
 *       -lOSMesa -lGL \
 *       -o osmesa_texmatrix_bug
 *
 * Expected: all six GLSL shaders compile successfully (Compiled: 1).
 * Actual:   the four shaders that reference gl_TextureMatrix[n] fail
 *           with "Error: invalid assignment" and Compiled: 0.
 */

#ifdef USE_MGL_NAMESPACE
#  include <OSMesa/osmesa.h>
#  include <OSMesa/gl.h>
#else
#  include <GL/osmesa.h>
#  include <GL/gl.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Compile a GLSL vertex shader and report pass/fail + info log. */
static void test_vert_shader(const char *src, const char *name)
{
    GLuint sh = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
    glShaderSourceARB(sh, 1, &src, NULL);
    glCompileShaderARB(sh);

    GLint compiled = 0;
    glGetObjectParameterivARB(sh, GL_OBJECT_COMPILE_STATUS_ARB, &compiled);

    GLint logLen = 0;
    glGetObjectParameterivARB(sh, GL_OBJECT_INFO_LOG_LENGTH_ARB, &logLen);

    printf("[%s] Compiled=%d", name, compiled);
    if (logLen > 1) {
        char *log = (char *)malloc(logLen);
        glGetInfoLogARB(sh, logLen, NULL, log);
        printf("  log: %s", log);   /* already ends with \n from Mesa */
        free(log);
    } else {
        putchar('\n');
    }
    glDeleteObjectARB(sh);
}

int main(void)
{
    /* Create a minimal OSMesa context. */
    unsigned char buf[64 * 64 * 4];
    OSMesaContext ctx = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
    if (!ctx) { fprintf(stderr, "OSMesaCreateContextExt failed\n"); return 1; }
    if (!OSMesaMakeCurrent(ctx, buf, GL_UNSIGNED_BYTE, 64, 64)) {
        fprintf(stderr, "OSMesaMakeCurrent failed\n"); return 1;
    }

    printf("GL_VERSION  : %s\n", (const char *)glGetString(GL_VERSION));
    printf("GL_RENDERER : %s\n", (const char *)glGetString(GL_RENDERER));

    GLint maxCoords = 0;
    glGetIntegerv(GL_MAX_TEXTURE_COORDS, &maxCoords);
    printf("GL_MAX_TEXTURE_COORDS = %d\n\n", maxCoords);

    /* ------------------------------------------------------------------
     * PASSING cases: these all compile without error.
     * ------------------------------------------------------------------ */

    /* 1. Trivial shader – sanity check. */
    test_vert_shader(
        "void main() { gl_Position = ftransform(); }",
        "PASS_trivial");

    /* 2. Built-in matrix identifier (no subscript) * vertex attribute.
     *    The matmul optimization in slang_codegen.c rewrites this as
     *    gl_Vertex * gl_ModelViewProjectionMatrixTranspose, which works. */
    test_vert_shader(
        "void main() {\n"
        "  gl_TexCoord[0] = gl_ModelViewMatrix * gl_Vertex;\n"
        "  gl_Position = ftransform();\n"
        "}",
        "PASS_modelview_mul");

    /* 3. User-defined uniform mat4 (non-state-var) * attribute – works fine. */
    test_vert_shader(
        "uniform mat4 myMatrix;\n"
        "void main() {\n"
        "  gl_TexCoord[0] = myMatrix * gl_MultiTexCoord0;\n"
        "  gl_Position = ftransform();\n"
        "}",
        "PASS_user_uniform_mul");

    /* 4. Direct passthrough of a texture coordinate attribute – works fine. */
    test_vert_shader(
        "void main() {\n"
        "  gl_TexCoord[0] = gl_MultiTexCoord0;\n"
        "  gl_Position = ftransform();\n"
        "}",
        "PASS_texcoord_passthrough");

    putchar('\n');

    /* ------------------------------------------------------------------
     * FAILING cases: all involve gl_TextureMatrix[n] subscript access.
     * ------------------------------------------------------------------
     * Expected: Compiled=1.   Actual: Compiled=0, "Error: invalid assignment"
     * ------------------------------------------------------------------ */

    /* 5. Standard texture-coordinate transform (GLSL 1.10 idiom).
     *    This is the pattern that breaks real-world shadow/multi-texture
     *    vertex shaders.                                                 */
    test_vert_shader(
        "void main() {\n"
        "  gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;\n"
        "  gl_Position = ftransform();\n"
        "}",
        "FAIL_texmat0_mul");

    /* 6. Second texture unit – same failure, different index. */
    test_vert_shader(
        "void main() {\n"
        "  gl_TexCoord[1] = gl_TextureMatrix[1] * gl_MultiTexCoord1;\n"
        "  gl_Position = ftransform();\n"
        "}",
        "FAIL_texmat1_mul");

    /* 7. Just reading gl_TextureMatrix[0] into a local variable – also fails,
     *    showing the bug is in the subscript, not the multiply. */
    test_vert_shader(
        "void main() {\n"
        "  mat4 m = gl_TextureMatrix[0];\n"  /* read only, no multiply */
        "  gl_Position = ftransform();\n"
        "}",
        "FAIL_texmat_read_to_local");

    /* 8. Column access via double subscript – also fails. */
    test_vert_shader(
        "void main() {\n"
        "  vec4 col = gl_TextureMatrix[0][0];\n"
        "  gl_TexCoord[0] = col;\n"
        "  gl_Position = ftransform();\n"
        "}",
        "FAIL_texmat_double_subscript");

    /* 9. Two-texture-coordinate transform (the canonical failing pattern
     *    from real shadow-group vertex shaders). */
    test_vert_shader(
        "void main() {\n"
        "  gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;\n"
        "  gl_TexCoord[1] = gl_TextureMatrix[1] * gl_MultiTexCoord1;\n"
        "  gl_Position = ftransform();\n"
        "}",
        "FAIL_two_texcoords");

    OSMesaDestroyContext(ctx);
    return 0;
}
```

### Expected output

```
GL_VERSION  : 2.0 Mesa 7.0.4
GL_RENDERER : Mesa OffScreen
GL_MAX_TEXTURE_COORDS = 8

[PASS_trivial] Compiled=1
[PASS_modelview_mul] Compiled=1
[PASS_user_uniform_mul] Compiled=1
[PASS_texcoord_passthrough] Compiled=1

[FAIL_texmat0_mul] Compiled=1
[FAIL_texmat1_mul] Compiled=1
[FAIL_texmat_read_to_local] Compiled=1
[FAIL_texmat_double_subscript] Compiled=1
[FAIL_two_texcoords] Compiled=1
```

### Actual output (Mesa 7.0.4 OSMesa)

```
GL_VERSION  : 2.0 Mesa 7.0.4
GL_RENDERER : Mesa OffScreen
GL_MAX_TEXTURE_COORDS = 8

[PASS_trivial] Compiled=1
[PASS_modelview_mul] Compiled=1
[PASS_user_uniform_mul] Compiled=1
[PASS_texcoord_passthrough] Compiled=1

[FAIL_texmat0_mul] Compiled=0  log: Error: invalid assignment
[FAIL_texmat1_mul] Compiled=0  log: Error: invalid assignment
[FAIL_texmat_read_to_local] Compiled=0  log: Error: invalid assignment
[FAIL_texmat_double_subscript] Compiled=0  log: Error: invalid assignment
[FAIL_two_texcoords] Compiled=0  log: Error: invalid assignment
```

---

## Root Cause Analysis

The failure is caused by two interacting issues in the slang GLSL compiler.

### Issue 1 — `_slang_check_matmul_optimization` ignores subscripted arrays (`slang_codegen.c:2470`)

Matrix–vector multiplications that use built-in state-variable matrices are only valid in
osmesa when the compiler applies the *matmul optimization*: it rewrites
`gl_SomeMatrix * vec` as `vec * gl_SomeMatrixTranspose`, because the transpose form is stored
as four consecutive `PROGRAM_STATE_VAR` rows and can be emitted as four dot products by the
`vec4 * mat4` built-in assembly operator.

The guard at line 2484 is:

```c
/* slang_codegen.c:2484 */
if (oper->children[0].type == SLANG_OPER_IDENTIFIER) {
```

This means the optimization fires only when the matrix operand is a plain identifier such as
`gl_ModelViewMatrix`.  For `gl_TextureMatrix[0]`, the AST node type is
`SLANG_OPER_SUBSCRIPT`, so the optimization is skipped and the compiler attempts a generic
matrix multiplication using direct subscript access into the `gl_TextureMatrix` array.

### Issue 2 — State-variable matrix arrays cannot be subscripted in the emit phase (`slang_emit.c`)

When the matmul optimization is NOT applied, the code path falls through to
`_slang_gen_subscript` → produces an `IR_ELEMENT(IR_VAR(gl_TextureMatrix), IR_FLOAT(n))`
node. During the emit phase, `emit_array_element` sets `n->Store->File = PROGRAM_STATE_VAR`
and calls `_slang_alloc_statevar` which does successfully allocate four consecutive
`PROGRAM_STATE_VAR` rows for the requested texture unit and returns a valid index.

However, the parent expression — typically a matrix–vector multiply, a local variable
assignment, or even a simple load — subsequently fails because the IR emitter does not
know how to generate the four `DP4` instructions that a mat4-to-vec4 multiply via a
state-variable requires.  For a direct read (`mat4 m = gl_TextureMatrix[0]`) the emitter
eventually reaches `emit_move`, finds that `n->Children[1]->Store->Index < 0` (the RHS
store index was never properly finalized), and reports:

```c
/* slang_emit.c:957 */
slang_info_log_error(emitInfo->log, "invalid assignment");
```

In contrast, the non-subscripted form `gl_ModelViewMatrix * gl_Vertex` never reaches this
path: the matmul optimization rewrites it to `gl_Vertex * gl_ModelViewMatrixTranspose`
before codegen, and `ftransform()` is similarly implemented in the vertex built-in as
`gl_Vertex * gl_ModelViewProjectionMatrixTranspose` (see `slang_vertex_builtin_gc.h`).
Both use a plain identifier that the emit phase can fully handle as four consecutive
state-variable rows.

### Why only `gl_TextureMatrix` is affected

Every other built-in matrix (`gl_ModelViewMatrix`, `gl_ProjectionMatrix`,
`gl_ModelViewProjectionMatrix`, `gl_NormalMatrix`) is a single `mat4` variable, not an
array.  The GLSL 1.10 spec declares `gl_TextureMatrix` as
`uniform mat4 gl_TextureMatrix[gl_MaxTextureCoords]` — the *only* built-in matrix that is
an array and therefore requires subscript access.  All other matrices are used as plain
identifiers, where the matmul optimization fires unconditionally.

---

## Suggested Fix

Extend `_slang_check_matmul_optimization` in `slang_codegen.c` to also handle the
subscripted case, transforming `gl_TextureMatrix[n] * vec` into
`vec * gl_TextureMatrixTranspose[n]` (swapping operands and renaming the array, keeping
the subscript intact).  This mirrors exactly what the existing optimization does for
non-array matrices.  The subscripted transpose form must then also be handled in
`emit_array_element` / `_slang_alloc_statevar` the same way the four-row state-var
allocation already handles it for `lookup_statevar`.

Conceptual diff for `slang_codegen.c` (around line 2484):

```c
-    if (oper->children[0].type == SLANG_OPER_IDENTIFIER) {
+    /* Also handle subscripted array matrices: gl_TextureMatrix[n] * vec */
+    if (oper->children[0].type == SLANG_OPER_IDENTIFIER ||
+        oper->children[0].type == SLANG_OPER_SUBSCRIPT) {
         GLuint i;
+        slang_operation *matOp = (oper->children[0].type == SLANG_OPER_IDENTIFIER)
+            ? &oper->children[0]
+            : &oper->children[0].children[0]; /* base of subscript */
         for (i = 0; matrices[i].orig; i++) {
-            if (oper->children[0].a_id
+            if (matOp->a_id
                 == slang_atom_pool_atom(A->atoms, matrices[i].orig)) {
-                oper->children[0].a_id
+                matOp->a_id
                     = slang_atom_pool_atom(A->atoms, matrices[i].tranpose);
                 _slang_operation_swap(&oper->children[0], &oper->children[1]);
                 return;
             }
         }
     }
```

---

## Context: How This Was Discovered

This bug was found while testing `SoShadowGroup` in the [obol](https://github.com/starseeker/obol)
C++ scene-graph library (a fork of Coin3D) using the starseeker/osmesa submodule as a
headless rendering backend.

The shadow-group vertex shader generated by `SoShadowGroupP::setVertexShader`
(`src/shadows/SoShadowGroup.cpp`) unconditionally emits:

```glsl
gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;
gl_TexCoord[1] = gl_TextureMatrix[1] * gl_MultiTexCoord1;
```

These two lines cause the entire vertex shader compilation to fail (silently falling
back to fixed-function rendering), so shadow mapping never activates even when
`SoShadowGroup::isSupported()` returns `true`.

A secondary bug was also found in `include/Inventor/SbString.h`
(`SbString::vsprintf`): the `va_list` was consumed by the size-determination
`vsnprintf(nullptr, 0, ...)` call before being used for the actual write, causing a
segfault when the GLSL info log (containing the above error message) was printed.
That bug has been fixed separately in this repository.

---

## Files and Line Numbers (Mesa 7.0.4)

| File | Line | Relevance |
|------|------|-----------|
| `src/shader/slang/slang_codegen.c` | 2484 | `SLANG_OPER_IDENTIFIER` guard that excludes subscript case |
| `src/shader/slang/slang_codegen.c` | 2374 | `_slang_gen_subscript` – IR_ELEMENT construction |
| `src/shader/slang/slang_emit.c` | 940 | `emit_move` – where "invalid assignment" is raised |
| `src/shader/slang/slang_emit.c` | 957 | `slang_info_log_error(emitInfo->log, "invalid assignment")` |
| `src/shader/slang/slang_emit.c` | 1460 | `emit_array_element` – PROGRAM_STATE_VAR allocation path |
| `src/shader/slang/slang_builtin.c` | 77 | `gl_TextureMatrix` → `STATE_TEXTURE_MATRIX` mapping |
| `src/shader/slang/slang_builtin.c` | 112 | Texture-matrix-specific `tokens[1] = index1` handling |
| `src/shader/slang/slang_builtin.c` | 305 | Matrix state var allocation loop (four rows) |
