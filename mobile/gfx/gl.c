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

#include "gl.h"
#include <stdlib.h>
#include <string.h>
#include <EGL/egl.h>

/* Helper to get GL function pointer */
static void *
get_gl_proc(const char *name, EGLDisplay display)
{
    void *proc = eglGetProcAddress(name);
    if (proc)
        return proc;
    /* Fallback to dlsym or direct call if needed */
    return (void *)((uintptr_t)name); /* Placeholder */
}

bool
gl_init(gl_t *gl, EGLDisplay display)
{
    if (!gl)
        return false;

    memset(gl, 0, sizeof(*gl));

    /* Load all GL functions */
    #define LOAD_GL_FUNC(func) gl->func = (typeof(gl->func))get_gl_proc(#func, display)
    /* Core GL 2.0 */
    LOAD_GL_FUNC(glClear);
    LOAD_GL_FUNC(glClearColor);
    LOAD_GL_FUNC(glClearDepthf);
    LOAD_GL_FUNC(glClearStencil);
    LOAD_GL_FUNC(glColorMask);
    LOAD_GL_FUNC(glDisable);
    LOAD_GL_FUNC(glEnable);
    LOAD_GL_FUNC(glFinish);
    LOAD_GL_FUNC(glFlush);
    LOAD_GL_FUNC(glScissor);
    LOAD_GL_FUNC(glViewport);
    LOAD_GL_FUNC(glDrawArrays);
    LOAD_GL_FUNC(glDrawElements);
    LOAD_GL_FUNC(glGenBuffers);
    LOAD_GL_FUNC(glDeleteBuffers);
    LOAD_GL_FUNC(glBindBuffer);
    LOAD_GL_FUNC(glBufferData);
    LOAD_GL_FUNC(glBufferSubData);
    LOAD_GL_FUNC(glGenFramebuffers);
    LOAD_GL_FUNC(glDeleteFramebuffers);
    LOAD_GL_FUNC(glBindFramebuffer);
    LOAD_GL_FUNC(glFramebufferRenderbuffer);
    LOAD_GL_FUNC(glFramebufferTexture2D);
    LOAD_GL_FUNC(glGenRenderbuffers);
    LOAD_GL_FUNC(glDeleteRenderbuffers);
    LOAD_GL_FUNC(glBindRenderbuffer);
    LOAD_GL_FUNC(glRenderbufferStorage);
    LOAD_GL_FUNC(glGenTextures);
    LOAD_GL_FUNC(glDeleteTextures);
    LOAD_GL_FUNC(glBindTexture);
    LOAD_GL_FUNC(glCompressedTexImage2D);
    LOAD_GL_FUNC(glCompressedTexSubImage2D);
    LOAD_GL_FUNC(glCopyTexImage2D);
    LOAD_GL_FUNC(glCopyTexSubImage2D);
    LOAD_GL_FUNC(glTexParameterf);
    LOAD_GL_FUNC(glTexParameteri);
    LOAD_GL_FUNC(glTexParameterfv);
    LOAD_GL_FUNC(glTexParameteriv);
    LOAD_GL_FUNC(glTexImage2D);
    LOAD_GL_FUNC(glTexSubImage2D);
    LOAD_GL_FUNC(glGenerateMipmap);
    LOAD_GL_FUNC(glActiveTexture);
    LOAD_GL_FUNC(glAttachShader);
    LOAD_GL_FUNC(glBindAttribLocation);
    LOAD_GL_FUNC(glCompileShader);
    LOAD_GL_FUNC(glCreateProgram);
    LOAD_GL_FUNC(glCreateShader);
    LOAD_GL_FUNC(glDeleteProgram);
    LOAD_GL_FUNC(glDeleteShader);
    LOAD_GL_FUNC(glDetachShader);
    LOAD_GL_FUNC(glDisableVertexAttribArray);
    LOAD_GL_FUNC(glEnableVertexAttribArray);
    LOAD_GL_FUNC(glGetActiveAttrib);
    LOAD_GL_FUNC(glGetActiveUniform);
    LOAD_GL_FUNC(glGetAttachedShaders);
    LOAD_GL_FUNC(glGetAttribLocation);
    LOAD_GL_FUNC(glGetBooleanv);
    LOAD_GL_FUNC(glGetBufferParameteriv);
    LOAD_GL_FUNC(glGetError);
    LOAD_GL_FUNC(glGetFloatv);
    LOAD_GL_FUNC(glGetIntegerv);
    LOAD_GL_FUNC(glGetProgramiv);
    LOAD_GL_FUNC(glGetProgramInfoLog);
    LOAD_GL_FUNC(glGetRenderbufferParameteriv);
    LOAD_GL_FUNC(glGetShaderiv);
    LOAD_GL_FUNC(glGetShaderInfoLog);
    LOAD_GL_FUNC(glGetShaderPrecisionFormat);
    LOAD_GL_FUNC(glGetShaderSource);
    LOAD_GL_FUNC(glGetString);
    LOAD_GL_FUNC(glGetTexParameterfv);
    LOAD_GL_FUNC(glGetTexParameteriv);
    LOAD_GL_FUNC(glGetUniformfv);
    LOAD_GL_FUNC(glGetUniformiv);
    LOAD_GL_FUNC(glGetUniformLocation);
    LOAD_GL_FUNC(glGetVertexAttribfv);
    LOAD_GL_FUNC(glGetVertexAttribiv);
    LOAD_GL_FUNC(glGetVertexAttribPointerv);
    LOAD_GL_FUNC(glHint);
    LOAD_GL_FUNC(glIsBuffer);
    LOAD_GL_FUNC(glIsEnabled);
    LOAD_GL_FUNC(glIsFramebuffer);
    LOAD_GL_FUNC(glIsProgram);
    LOAD_GL_FUNC(glIsRenderbuffer);
    LOAD_GL_FUNC(glIsShader);
    LOAD_GL_FUNC(glIsTexture);
    LOAD_GL_FUNC(glLineWidth);
    LOAD_GL_FUNC(glLinkProgram);
    LOAD_GL_FUNC(glPixelStorei);
    LOAD_GL_FUNC(glPolygonOffset);
    LOAD_GL_FUNC(glReadPixels);
    LOAD_GL_FUNC(glReleaseShaderCompiler);
    LOAD_GL_FUNC(glSampleCoverage);
    LOAD_GL_FUNC(glStencilFunc);
    LOAD_GL_FUNC(glStencilFuncSeparate);
    LOAD_GL_FUNC(glStencilMask);
    LOAD_GL_FUNC(glStencilMaskSeparate);
    LOAD_GL_FUNC(glStencilOp);
    LOAD_GL_FUNC(glStencilOpSeparate);
    /* GL 3.0 */
    LOAD_GL_FUNC(glTexImage3D);
    LOAD_GL_FUNC(glTexSubImage3D);
    LOAD_GL_FUNC(glTexParameterIiv);
    LOAD_GL_FUNC(glTexParameterIuiv);
    LOAD_GL_FUNC(glWaitSync);
    LOAD_GL_FUNC(glClientWaitSync);
    LOAD_GL_FUNC(glGetSynciv);
    LOAD_GL_FUNC(glFenceSync);
    LOAD_GL_FUNC(glIsSync);
    LOAD_GL_FUNC(glDeleteSync);
    LOAD_GL_FUNC(glGenVertexArrays);
    LOAD_GL_FUNC(glDeleteVertexArrays);
    LOAD_GL_FUNC(glBindVertexArray);
    LOAD_GL_FUNC(glVertexAttribPointer);
    #undef LOAD_GL_FUNC

    /* Check if essential functions are loaded */
    if (!gl->glClear || !gl->glCreateProgram || !gl->glTexImage2D)
        return false;

    return true;
}

GLuint
gl_create_program(gl_t *gl, const char *vs_src, const char *fs_src)
{
    GLuint vs, fs, program;
    const char *sources[] = { vs_src, fs_src };
    GLuint shaders[] = { GL_VERTEX_SHADER, GL_FRAGMENT_SHADER };
    int i;

    if (!gl || !vs_src || !fs_src)
        return 0;

    program = gl->glCreateProgram();
    if (!program)
        return 0;

    for (i = 0; i < 2; i++) {
        GLuint shader = gl->glCreateShader(shaders[i]);
        if (!shader) {
            gl->glDeleteProgram(program);
            return 0;
        }
        gl->glShaderSource(shader, 1, &sources[i], NULL);
        gl->glCompileShader(shader);

        GLint compiled;
        gl->glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint log_len;
            gl->glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);
            if (log_len > 0) {
                char *log = malloc(log_len);
                gl->glGetShaderInfoLog(shader, log_len, NULL, log);
                /* In a real system, we would log this */
                free(log);
            }
            gl->glDeleteShader(shader);
            gl->glDeleteProgram(program);
            return 0;
        }
        gl->glAttachShader(program, shader);
        gl->glDeleteShader(shader); /* Shader is attached to program, safe to delete */
    }

    gl->glLinkProgram(program);

    GLint linked;
    gl->glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint log_len;
        gl->glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);
        if (log_len > 0) {
            char *log = malloc(log_len);
            gl->glGetProgramInfoLog(program, log_len, NULL, log);
            free(log);
        }
        gl->glDeleteProgram(program);
        return 0;
    }

    return program;
}

GLuint
gl_create_texture(gl_t *gl, int width, int height, const void *data, GLenum format, GLenum type)
{
    GLuint tex;

    if (!gl || width <= 0 || height <= 0)
        return 0;

    gl->glGenTextures(1, &tex);
    if (!tex)
        return 0;

    gl->glBindTexture(GL_TEXTURE_2D, tex);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    gl->glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, type, data);

    gl->glBindTexture(GL_TEXTURE_2D, 0);

    return tex;
}

void
gl_draw_quad(gl_t *gl, GLuint texture, float x, float y, float w, float h)
{
    if (!gl || !texture)
        return;

    /* Use a simple shader program for textured quad */
    /* In a real implementation, we would have a shader program cached */
    /* For now, we assume a program is bound that uses the texture */
    GLfloat vertices[] = {
        x,     y,     0.0f, 0.0f, /* x, y, u, v */
        x + w, y,     1.0f, 0.0f,
        x,     y + h, 0.0f, 1.0f,
        x + w, y + h, 1.0f, 1.0f
    };
    GLuint vbo, vao;
    gl->glGenVertexArrays(1, &vao);
    gl->glGenBuffers(1, &vbo);
    gl->glBindVertexArray(vao);
    gl->glBindBuffer(GL_ARRAY_BUFFER, vbo);
    gl->glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    gl->glEnableVertexAttribArray(0); /* position */
    gl->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)0);
    gl->glEnableVertexAttribArray(1); /* texcoord */
    gl->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
    gl->glActiveTexture(GL_TEXTURE0);
    gl->glBindTexture(GL_TEXTURE_2D, texture);
    gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    gl->glBindTexture(GL_TEXTURE_2D, 0);
    gl->glBindBuffer(GL_ARRAY_BUFFER, 0);
    gl->glBindVertexArray(0);
    gl->glDeleteBuffers(1, &vbo);
    gl->glDeleteVertexArrays(1, &vao);
}

void
gl_clear(gl_t *gl, float r, float g, float b, float a)
{
    if (!gl)
        return;
    gl->glClearColor(r, g, b, a);
    gl->glClear(GL_COLOR_BUFFER_BIT);
}