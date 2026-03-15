/*
 * obol_modern_shaders.h  –  GLSL 3.30 core-profile shaders for Obol
 *
 * This header provides GLSL shader source strings that implement Obol's
 * standard rendering operations using the OpenGL 3.3 core profile API.
 * These shaders replace the fixed-function pipeline calls currently used by
 * Obol (glBegin/End, glMaterial, glLight, glMatrixMode, glShadeModel, etc.)
 * and are a prerequisite for the PortableGL software backend.
 *
 * Shader programs:
 *
 *   OBOL_SHADER_PHONG_VERT / FRAG
 *     Full Phong lighting: up to 8 directional/point/spot lights,
 *     per-material ambient/diffuse/specular/emission.  Replaces the
 *     fixed-function lighting model activated by glEnable(GL_LIGHTING).
 *
 *   OBOL_SHADER_BASECOLOR_VERT / FRAG
 *     Flat colour (no lighting): uses per-vertex colour or a uniform
 *     colour.  Replaces glDisable(GL_LIGHTING) + glColor* calls.
 *
 *   OBOL_SHADER_TEXTURE2D_VERT / FRAG
 *     Textured with optional Phong lighting: drives SoTexture2 nodes.
 *
 * Vertex attribute layout (binding indices):
 *
 *   location 0  –  aPosition    (vec3)
 *   location 1  –  aNormal      (vec3)
 *   location 2  –  aTexCoord    (vec2)
 *   location 3  –  aColor       (vec4)
 *
 * Uniform blocks:
 *
 *   uModelView        (mat4)  – model-view matrix
 *   uProjection       (mat4)  – projection matrix
 *   uNormalMatrix     (mat3)  – inverse-transpose of model-view (for normals)
 *   uHasNormals       (bool)  – whether aNormal contains valid data
 *   uHasColors        (bool)  – whether aColor contains valid data
 *   uHasTexCoords     (bool)  – whether aTexCoord contains valid data
 *
 *   uNumLights        (int)   – number of active lights [0..OBOL_MAX_LIGHTS]
 *   uLights[i].position              (vec4)
 *   uLights[i].ambient               (vec4)
 *   uLights[i].diffuse               (vec4)
 *   uLights[i].specular              (vec4)
 *   uLights[i].spotDirection         (vec3)
 *   uLights[i].spotCutoff            (float) cos of half-angle; -1=non-spot
 *   uLights[i].spotExponent          (float)
 *   uLights[i].constantAttenuation   (float)
 *   uLights[i].linearAttenuation     (float)
 *   uLights[i].quadraticAttenuation  (float)
 *
 *   uMatAmbient    (vec4)
 *   uMatDiffuse    (vec4)
 *   uMatSpecular   (vec4)
 *   uMatEmission   (vec4)
 *   uMatShininess  (float)
 *   uTwoSided      (bool)   – replaces GL_LIGHT_MODEL_TWO_SIDE
 *
 *   uTexture       (sampler2D)
 *
 * Usage (system GL or PortableGL-backed rendering):
 *
 *   GLuint prog = obol_compile_shader_program(
 *       OBOL_SHADER_PHONG_VERT, OBOL_SHADER_PHONG_FRAG);
 *   glUseProgram(prog);
 *   glUniformMatrix4fv(glGetUniformLocation(prog, "uModelView"), 1, GL_FALSE, mv);
 *   ...
 */

#ifndef OBOL_MODERN_SHADERS_H
#define OBOL_MODERN_SHADERS_H

/* Maximum number of simultaneously active lights (matches GL_MAX_LIGHTS = 8) */
#define OBOL_MAX_LIGHTS 8

/* Stringify helpers for use in shader source strings.  These must be defined
 * before the shader macro strings that reference OBOL_MAX_LIGHTS_STR.
 * Two-level indirection is required so that OBOL_MAX_LIGHTS is first
 * expanded to its numeric value (8) and THEN stringified to "8". */
#define OBOL_MAX_LIGHTS_XSTR(s) #s
#define OBOL_MAX_LIGHTS_STR(x)  OBOL_MAX_LIGHTS_XSTR(x)

/* =========================================================================
 * Phong vertex shader
 * ===================================================================== */

#define OBOL_SHADER_PHONG_VERT \
"#version 330 core\n" \
"\n" \
"layout(location = 0) in vec3 aPosition;\n" \
"layout(location = 1) in vec3 aNormal;\n" \
"layout(location = 2) in vec2 aTexCoord;\n" \
"layout(location = 3) in vec4 aColor;\n" \
"\n" \
"uniform mat4 uModelView;\n" \
"uniform mat4 uProjection;\n" \
"uniform mat3 uNormalMatrix;\n" \
"uniform bool uHasNormals;\n" \
"uniform bool uHasColors;\n" \
"uniform bool uHasTexCoords;\n" \
"\n" \
"out vec3 vNormalEye;\n" \
"out vec3 vPositionEye;\n" \
"out vec2 vTexCoord;\n" \
"out vec4 vColor;\n" \
"\n" \
"void main() {\n" \
"    vec4 posEye  = uModelView * vec4(aPosition, 1.0);\n" \
"    vPositionEye = posEye.xyz;\n" \
"    vNormalEye   = uHasNormals ? normalize(uNormalMatrix * aNormal)\n" \
"                               : vec3(0.0, 0.0, 1.0);\n" \
"    vTexCoord    = uHasTexCoords ? aTexCoord : vec2(0.0);\n" \
"    vColor       = uHasColors   ? aColor    : vec4(1.0);\n" \
"    gl_Position  = uProjection  * posEye;\n" \
"}\n"

/* =========================================================================
 * Phong fragment shader
 * ===================================================================== */

#define OBOL_SHADER_PHONG_FRAG \
"#version 330 core\n" \
"\n" \
"#define OBOL_MAX_LIGHTS " OBOL_MAX_LIGHTS_STR(OBOL_MAX_LIGHTS) "\n" \
"\n" \
"struct ObolLight {\n" \
"    vec4  position;\n" \
"    vec4  ambient;\n" \
"    vec4  diffuse;\n" \
"    vec4  specular;\n" \
"    vec3  spotDirection;\n" \
"    float spotCutoff;\n" \
"    float spotExponent;\n" \
"    float constantAttenuation;\n" \
"    float linearAttenuation;\n" \
"    float quadraticAttenuation;\n" \
"};\n" \
"\n" \
"uniform int       uNumLights;\n" \
"uniform ObolLight uLights[OBOL_MAX_LIGHTS];\n" \
"\n" \
"uniform vec4  uMatAmbient;\n" \
"uniform vec4  uMatDiffuse;\n" \
"uniform vec4  uMatSpecular;\n" \
"uniform vec4  uMatEmission;\n" \
"uniform float uMatShininess;\n" \
"uniform bool  uTwoSided;\n" \
"\n" \
"uniform sampler2D uTexture;\n" \
"uniform bool      uHasTexture;\n" \
"\n" \
"in vec3 vNormalEye;\n" \
"in vec3 vPositionEye;\n" \
"in vec2 vTexCoord;\n" \
"in vec4 vColor;\n" \
"\n" \
"out vec4 fragColor;\n" \
"\n" \
"vec4 computePhong(vec3 N, vec3 pos) {\n" \
"    vec4 ambient  = vec4(0.0);\n" \
"    vec4 diffuse  = vec4(0.0);\n" \
"    vec4 specular = vec4(0.0);\n" \
"    vec3 V = normalize(-pos);\n" \
"    for (int i = 0; i < uNumLights; ++i) {\n" \
"        vec3 L;\n" \
"        float att = 1.0;\n" \
"        if (uLights[i].position.w == 0.0) {\n" \
"            L = normalize(-uLights[i].position.xyz);\n" \
"        } else {\n" \
"            vec3 d = uLights[i].position.xyz - pos;\n" \
"            float dist = length(d);\n" \
"            L = normalize(d);\n" \
"            float ca = uLights[i].constantAttenuation;\n" \
"            float la = uLights[i].linearAttenuation;\n" \
"            float qa = uLights[i].quadraticAttenuation;\n" \
"            att = 1.0 / (ca + la * dist + qa * dist * dist);\n" \
"        }\n" \
"        float spot = 1.0;\n" \
"        if (uLights[i].spotCutoff > -0.5) {\n" \
"            float cosA = dot(-L, normalize(uLights[i].spotDirection));\n" \
"            spot = (cosA >= uLights[i].spotCutoff)\n" \
"                   ? pow(cosA, uLights[i].spotExponent) : 0.0;\n" \
"        }\n" \
"        float diff = max(dot(N, L), 0.0);\n" \
"        float spec = 0.0;\n" \
"        if (diff > 0.0) {\n" \
"            vec3 R = reflect(-L, N);\n" \
"            spec = pow(max(dot(R, V), 0.0), uMatShininess);\n" \
"        }\n" \
"        float f = att * spot;\n" \
"        ambient  += f * uLights[i].ambient;\n" \
"        diffuse  += f * diff * uLights[i].diffuse;\n" \
"        specular += f * spec * uLights[i].specular;\n" \
"    }\n" \
"    return uMatEmission\n" \
"           + ambient  * uMatAmbient\n" \
"           + diffuse  * uMatDiffuse\n" \
"           + specular * uMatSpecular;\n" \
"}\n" \
"\n" \
"void main() {\n" \
"    vec3 N = normalize(vNormalEye);\n" \
"    if (!gl_FrontFacing && uTwoSided) N = -N;\n" \
"    vec4 litColor = (uNumLights > 0)\n" \
"                    ? computePhong(N, vPositionEye)\n" \
"                    : uMatDiffuse * vColor;\n" \
"    if (uHasTexture) litColor *= texture(uTexture, vTexCoord);\n" \
"    fragColor = clamp(litColor, 0.0, 1.0);\n" \
"}\n"

/* =========================================================================
 * Base-colour vertex shader (no lighting; replaces glDisable(GL_LIGHTING))
 * ===================================================================== */

#define OBOL_SHADER_BASECOLOR_VERT \
"#version 330 core\n" \
"\n" \
"layout(location = 0) in vec3 aPosition;\n" \
"layout(location = 2) in vec2 aTexCoord;\n" \
"layout(location = 3) in vec4 aColor;\n" \
"\n" \
"uniform mat4 uModelView;\n" \
"uniform mat4 uProjection;\n" \
"uniform bool uHasTexCoords;\n" \
"uniform bool uHasColors;\n" \
"uniform vec4 uBaseColor;\n" \
"\n" \
"out vec2 vTexCoord;\n" \
"out vec4 vColor;\n" \
"\n" \
"void main() {\n" \
"    vColor       = uHasColors    ? aColor   : uBaseColor;\n" \
"    vTexCoord    = uHasTexCoords ? aTexCoord : vec2(0.0);\n" \
"    gl_Position  = uProjection * uModelView * vec4(aPosition, 1.0);\n" \
"}\n"

/* =========================================================================
 * Base-colour fragment shader
 * ===================================================================== */

#define OBOL_SHADER_BASECOLOR_FRAG \
"#version 330 core\n" \
"\n" \
"uniform sampler2D uTexture;\n" \
"uniform bool      uHasTexture;\n" \
"\n" \
"in vec2 vTexCoord;\n" \
"in vec4 vColor;\n" \
"\n" \
"out vec4 fragColor;\n" \
"\n" \
"void main() {\n" \
"    fragColor = vColor;\n" \
"    if (uHasTexture) fragColor *= texture(uTexture, vTexCoord);\n" \
"}\n"

/* =========================================================================
 * Utility: compile and link a shader program from source strings.
 *
 * Returns the GL program handle on success, or 0 on failure.
 * Error messages are printed to stderr.
 *
 * This function requires a system GL or PortableGL context to be current.
 * It is defined inline so this header can be used without a separate .cpp.
 * ===================================================================== */

#include <Inventor/system/gl.h>
#include <cstdio>

static inline GLuint
obol_compile_shader_program(const char * vert_src, const char * frag_src)
{
    auto compile = [](GLenum type, const char* src) -> GLuint {
        GLuint sh = glCreateShader(type);
        glShaderSource(sh, 1, &src, nullptr);
        glCompileShader(sh);
        GLint ok = 0;
        glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[1024];
            glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
            std::fprintf(stderr, "obol shader compile error:\n%s\n", log);
            glDeleteShader(sh);
            return 0u;
        }
        return sh;
    };

    GLuint vs = compile(GL_VERTEX_SHADER,   vert_src);
    GLuint fs = compile(GL_FRAGMENT_SHADER, frag_src);
    if (!vs || !fs) { glDeleteShader(vs); glDeleteShader(fs); return 0u; }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::fprintf(stderr, "obol shader link error:\n%s\n", log);
        glDeleteProgram(prog);
        return 0u;
    }
    return prog;
}

#endif /* OBOL_MODERN_SHADERS_H */
