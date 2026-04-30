#include <stdio.h>
#include <string.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

int main() {
    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(dpy, NULL, NULL);

    EGLint cfg_attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_NONE
    };
    EGLConfig cfg;
    EGLint n;
    eglChooseConfig(dpy, cfg_attribs, &cfg, 1, &n);

    EGLint ctx_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attribs);

    EGLint surf_attribs[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
    EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, surf_attribs);

    eglMakeCurrent(dpy, surf, surf, ctx);

    printf("Renderer : %s\n", glGetString(GL_RENDERER));
    printf("Version  : %s\n", glGetString(GL_VERSION));
    printf("\n--- GLES Extensions ---\n");

    const char *exts = (const char*)glGetString(GL_EXTENSIONS);
    char buf[16384];
    strncpy(buf, exts, sizeof(buf)-1);
    char *tok = strtok(buf, " ");
    while (tok) { printf("%s\n", tok); tok = strtok(NULL, " "); }

    printf("\n--- EGL Extensions ---\n");
    const char *egl_exts = eglQueryString(dpy, EGL_EXTENSIONS);
    strncpy(buf, egl_exts, sizeof(buf)-1);
    tok = strtok(buf, " ");
    while (tok) { printf("%s\n", tok); tok = strtok(NULL, " "); }

    return 0;
}
