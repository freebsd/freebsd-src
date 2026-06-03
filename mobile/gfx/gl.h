/*
 * Copyright (c) 2026 The FreeBSD Mobile Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _MOBILE_GFX_GL_H_
#define _MOBILE_GFX_GL_H_

#include <stdint.h>
#include <stdbool.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

/* GL wrapper */
typedef struct {
    void (*glClear)(GLclampf r, GLclampf g, GLclampf b, GLclampf a);
    void (*glClearColor)(GLclampf r, GLclampf g, GLclampf b, GLclampf a);
    void (*glClearDepthf)(GLclampf d);
    void (*glClearStencil)(GLint s);
    void (*glColorMask)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
    void (*glDisable)(GLenum cap);
    void (*glEnable)(GLenum cap);
    void (*glFinish)(void);
    void (*glFlush)(void);
    void (*glScissor)(GLint x, GLint y, GLsizei width, GLsizei height);
    void (*glViewport)(GLint x, GLint y, GLsizei width, GLsizei height);
    void (*glDrawArrays)(GLenum mode, GLint first, GLsizei count);
    void (*glDrawElements)(GLenum mode, GLsizei count, GLenum type, const void *indices);
    void (*glGenBuffers)(GLsizei n, GLuint *buffers);
    void (*glDeleteBuffers)(GLsizei n, const GLuint *buffers);
    void (*glBindBuffer)(GLenum target, GLuint buffer);
    void (*glBufferData)(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
    void (*glBufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size, const void *data);
    void (*glGenFramebuffers)(GLsizei n, GLuint *framebuffers);
    void (*glDeleteFramebuffers)(GLsizei n, const GLuint *framebuffers);
    void (*glBindFramebuffer)(GLenum target, GLuint framebuffer);
    void (*glFramebufferRenderbuffer)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
    void (*glFramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
    void (*glGenRenderbuffers)(GLsizei n, GLuint *renderbuffers);
    void (*glDeleteRenderbuffers)(GLsizei n, const GLuint *renderbuffers);
    void (*glBindRenderbuffer)(GLenum target, GLuint renderbuffer);
    void (*glRenderbufferStorage)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
    void (*glGenTextures)(GLsizei n, GLuint *textures);
    void (*glDeleteTextures)(GLsizei n, const GLuint *textures);
    void (*glBindTexture)(GLenum target, GLuint texture);
    void (*glCompressedTexImage2D)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data);
    void (*glCompressedTexSubImage2D)(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, const void *data);
    void (*glCopyTexImage2D)(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
    void (*glCopyTexSubImage2D)(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint xoffset, GLint yoffset);
    void (*glTexParameterf)(GLenum target, GLenum pname, GLfloat param);
    void (*glTexParameteri)(GLenum target, GLenum pname, GLint param);
    void (*glTexParameterfv)(GLenum target, GLenum pname, const GLfloat *params);
    void (*glTexParameteriv)(GLenum target, GLenum pname, const GLint *params);
    void (*glTexImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels);
    void (*glTexSubImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, const void *pixels);
    void (*glGenerateMipmap)(GLenum target);
    void (*glActiveTexture)(GLenum texture);
    void (*glAttachShader)(GLuint program, GLuint shader);
    void (*glBindAttribLocation)(GLuint program, GLuint index, const GLchar *name);
    void (*glCompileShader)(GLuint shader);
    GLuint (*glCreateProgram)(void);
    GLuint (*glCreateShader)(GLenum type);
    void (*glDeleteProgram)(GLuint program);
    void (*glDeleteShader)(GLuint shader);
    void (*glDetachShader)(GLuint program, GLuint shader);
    void (*glDisableVertexAttribArray)(GLuint index);
    void (*glEnableVertexAttribArray)(GLuint index);
    void (*glGetActiveAttrib)(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
    void (*glGetActiveUniform)(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
    void (*glGetAttachedShaders)(GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders);
    GLint (*glGetAttribLocation)(GLuint program, const GLchar *name);
    void (*glGetBooleanv)(GLenum pname, GLboolean *data);
    void (*glGetBufferParameteriv)(GLenum target, GLenum pname, GLint *data);
    GLenum (*glGetError)(void);
    void (*glGetFloatv)(GLenum pname, GLfloat *data);
    void (*glGetIntegerv)(GLenum pname, GLint *data);
    void (*glGetProgramiv)(GLuint program, GLenum pname, GLint *data);
    void (*glGetProgramInfoLog)(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
    void (*glGetRenderbufferParameteriv)(GLenum target, GLenum pname, GLint *data);
    void (*glGetShaderiv)(GLuint shader, GLenum pname, GLint *data);
    void (*glGetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
    void (*glGetShaderPrecisionFormat)(GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision);
    void (*glGetShaderSource)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source);
    const GLubyte *(*glGetString)(GLenum name);
    void (*glGetTexParameterfv)(GLenum target, GLenum pname, GLfloat *data);
    void (*glGetTexParameteriv)(GLenum target, GLenum pname, GLint *data);
    void (*glGetUniformfv)(GLuint program, GLint location, GLfloat *data);
    void (*glGetUniformiv)(GLuint program, GLint location, GLint *data);
    GLint (*glGetUniformLocation)(GLuint program, const GLchar *name);
    void (*glGetVertexAttribfv)(GLuint index, GLenum pname, GLfloat *data);
    void (*glGetVertexAttribiv)(GLuint index, GLenum pname, GLint *data);
    void (*glGetVertexAttribPointerv)(GLuint index, GLenum pname, void **pointer);
    void (*glHint)(GLenum target, GLenum mode, GLint value);
    GLboolean (*glIsBuffer)(GLuint buffer);
    GLboolean (*glIsEnabled)(GLenum cap);
    GLboolean (*glIsFramebuffer)(GLuint framebuffer);
    GLboolean (*glIsProgram)(GLuint program);
    GLboolean (*glIsRenderbuffer)(GLuint renderbuffer);
    GLboolean (*glIsShader)(GLuint shader);
    GLboolean (*glIsTexture)(GLuint texture);
    void (*glLineWidth)(GLfloat width);
    void (*glLinkProgram)(GLuint program);
    void (*glPixelStorei)(GLenum pname, GLint param);
    void (*glPolygonOffset)(GLfloat factor, GLfloat units);
    void (*glReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels);
    void (*glReleaseShaderCompiler)(void);
    void (*glSampleCoverage)(GLclampf value, GLboolean invert);
    void (*glStencilFunc)(GLenum func, GLint ref, GLuint mask);
    void (*glStencilFuncSeparate)(GLenum face, GLenum func, GLint ref, GLuint mask);
    void (*glStencilMask)(GLuint mask);
    void (*glStencilMaskSeparate)(GLenum face, GLuint mask);
    void (*glStencilOp)(GLenum fail, GLenum zfail, GLenum zpass);
    void (*glStencilOpSeparate)(GLenum face, GLenum fail, GLenum zfail, GLenum zpass);
    void (*glTexImage3D)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels);
    void (*glTexSubImage3D)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, const void *pixels);
    void (*glTexParameterIiv)(GLenum target, GLenum pname, const GLint *params);
    void (*glTexParameterIuiv)(GLenum target, GLenum pname, const GLuint *params);
    void (*glWaitSync)(GLsync sync, GLbitfield flags, GLuint64 timeout);
    void (*glClientWaitSync)(GLsync sync, GLbitfield flags, GLuint64 timeout);
    void (*glGetSynciv)(GLsync sync, GLenum pname, GLsizei count, GLsizei *length, GLint *values);
    GLsync (*glFenceSync)(GLenum condition, GLbitfield flags);
    GLboolean (*glIsSync)(GLsync sync);
    void (*glDeleteSync)(GLsync sync);
    void (*glGenVertexArrays)(GLsizei n, GLuint *arrays);
    void (*glDeleteVertexArrays)(GLsizei n, const GLuint *arrays);
    void (*glBindVertexArray)(GLuint array);
    void (*glVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
} gl_t;

/* Shader sources (as strings) */
static const char *vs_passthrough =
    "attribute vec2 a_position;\n"
    "attribute vec2 a_texcoord;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "   gl_Position = vec4(a_position, 0.0, 1.0);\n"
    "   v_texcoord = a_texcoord;\n"
    "}";

static const char *fs_solid_color =
    "precision mediump float;\n"
    "uniform vec4 u_color;\n"
    "void main() {\n"
    "   gl_FragColor = u_color;\n"
    "}";

static const char *fs_texture =
    "precision mediump float;\n"
    "uniform sampler2D u_texture;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "   gl_FragColor = texture2D(u_texture, v_texcoord);\n"
    "}";

static const char *fs_gradient =
    "precision mediump float;\n"
    "uniform vec2 u_resolution;\n"
    "uniform vec4 u_color1;\n"
    "uniform vec4 u_color2;\n"
    "void main() {\n"
    "   vec2 pos = gl_FragCoord.xy / u_resolution;\n"
    "   gl_FragColor = mix(u_color1, u_color2, pos.x);\n"
    "}";

/* Public API */
bool gl_init(gl_t *gl, EGLDisplay display);
GLuint gl_create_program(gl_t *gl, const char *vs_src, const char *fs_src);
GLuint gl_create_texture(gl_t *gl, int width, int height, const void *data, GLenum format, GLenum type);
void gl_draw_quad(gl_t *gl, GLuint texture, float x, float y, float w, float h);
void gl_clear(gl_t *gl, float r, float g, float b, float a);

#endif /* _MOBILE_GFX_GL_H_ */