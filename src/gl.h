#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <GL/glcorearb.h>

#define GL_FUNCS(X) \
    X(PFNGLGETSTRINGPROC,               glGetString) \
    X(PFNGLGETERRORPROC,                glGetError) \
    X(PFNGLENABLEPROC,                  glEnable) \
    X(PFNGLDISABLEPROC,                 glDisable) \
    X(PFNGLBLENDFUNCPROC,               glBlendFunc) \
    X(PFNGLBLENDFUNCSEPARATEPROC,       glBlendFuncSeparate) \
    X(PFNGLVIEWPORTPROC,                glViewport) \
    X(PFNGLSCISSORPROC,                 glScissor) \
    X(PFNGLCLEARCOLORPROC,              glClearColor) \
    X(PFNGLCLEARPROC,                   glClear) \
    X(PFNGLPIXELSTOREIPROC,             glPixelStorei) \
    X(PFNGLREADPIXELSPROC,              glReadPixels) \
    X(PFNGLGENTEXTURESPROC,             glGenTextures) \
    X(PFNGLDELETETEXTURESPROC,          glDeleteTextures) \
    X(PFNGLBINDTEXTUREPROC,             glBindTexture) \
    X(PFNGLTEXPARAMETERIPROC,           glTexParameteri) \
    X(PFNGLTEXIMAGE2DPROC,              glTexImage2D) \
    X(PFNGLACTIVETEXTUREPROC,           glActiveTexture) \
    X(PFNGLDRAWELEMENTSPROC,            glDrawElements) \
    X(PFNGLCREATESHADERPROC,            glCreateShader) \
    X(PFNGLSHADERSOURCEPROC,            glShaderSource) \
    X(PFNGLCOMPILESHADERPROC,           glCompileShader) \
    X(PFNGLGETSHADERIVPROC,             glGetShaderiv) \
    X(PFNGLGETSHADERINFOLOGPROC,        glGetShaderInfoLog) \
    X(PFNGLDELETESHADERPROC,            glDeleteShader) \
    X(PFNGLCREATEPROGRAMPROC,           glCreateProgram) \
    X(PFNGLATTACHSHADERPROC,            glAttachShader) \
    X(PFNGLLINKPROGRAMPROC,             glLinkProgram) \
    X(PFNGLGETPROGRAMIVPROC,            glGetProgramiv) \
    X(PFNGLGETPROGRAMINFOLOGPROC,       glGetProgramInfoLog) \
    X(PFNGLDELETEPROGRAMPROC,           glDeleteProgram) \
    X(PFNGLUSEPROGRAMPROC,              glUseProgram) \
    X(PFNGLGETUNIFORMLOCATIONPROC,      glGetUniformLocation) \
    X(PFNGLUNIFORM1IPROC,               glUniform1i) \
    X(PFNGLUNIFORMMATRIX4FVPROC,        glUniformMatrix4fv) \
    X(PFNGLGENVERTEXARRAYSPROC,         glGenVertexArrays) \
    X(PFNGLDELETEVERTEXARRAYSPROC,      glDeleteVertexArrays) \
    X(PFNGLBINDVERTEXARRAYPROC,         glBindVertexArray) \
    X(PFNGLGENBUFFERSPROC,              glGenBuffers) \
    X(PFNGLDELETEBUFFERSPROC,           glDeleteBuffers) \
    X(PFNGLBINDBUFFERPROC,              glBindBuffer) \
    X(PFNGLBUFFERDATAPROC,              glBufferData) \
    X(PFNGLBUFFERSUBDATAPROC,           glBufferSubData) \
    X(PFNGLVERTEXATTRIBPOINTERPROC,     glVertexAttribPointer) \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) \
    X(PFNGLGENFRAMEBUFFERSPROC,         glGenFramebuffers) \
    X(PFNGLDELETEFRAMEBUFFERSPROC,      glDeleteFramebuffers) \
    X(PFNGLBINDFRAMEBUFFERPROC,         glBindFramebuffer) \
    X(PFNGLFRAMEBUFFERTEXTURE2DPROC,    glFramebufferTexture2D) \
    X(PFNGLCHECKFRAMEBUFFERSTATUSPROC,  glCheckFramebufferStatus)

#define X(type, name) extern type name;
GL_FUNCS(X)
#undef X

// needs a current context
bool gl_load();
