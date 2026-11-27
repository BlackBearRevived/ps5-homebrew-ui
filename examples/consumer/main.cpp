// ps5-homebrew-ui - An external app with settings, details and focus restoration.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/save_file.hpp"
#include "gfx/gl_program.hpp"
#include "gfx/renderer.hpp"
#include "platform/ps5/system.hpp"
#include "ui/components/artwork_detail.hpp"
#include "ui/components/form.hpp"

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <cstdarg>
#include <chrono>
#include <cstdio>

void hui::sys::log(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    std::fputc('\n', stderr);
    va_end(args);
}

std::int64_t hui::sys::monotonic_us()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

namespace
{
struct Egl
{
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;

    bool open()
    {
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (display == EGL_NO_DISPLAY || !eglInitialize(display, nullptr, nullptr) ||
            !eglBindAPI(EGL_OPENGL_API))
            return false;
        const EGLint attrs[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE,
                                EGL_OPENGL_BIT,   EGL_RED_SIZE,    8,
                                EGL_NONE};
        EGLConfig config{};
        EGLint count = 0;
        if (!eglChooseConfig(display, attrs, &config, 1, &count) || count != 1)
            return false;
        const EGLint ctx[] = {EGL_CONTEXT_MAJOR_VERSION_KHR,
                              4,
                              EGL_CONTEXT_MINOR_VERSION_KHR,
                              5,
                              EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,
                              EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
                              EGL_NONE};
        const EGLint size[] = {EGL_WIDTH, 960, EGL_HEIGHT, 540, EGL_NONE};
        context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctx);
        surface = eglCreatePbufferSurface(display, config, size);
        return context != EGL_NO_CONTEXT && surface != EGL_NO_SURFACE &&
               eglMakeCurrent(display, surface, surface, context);
    }
    ~Egl()
    {
        if (display == EGL_NO_DISPLAY)
            return;
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context != EGL_NO_CONTEXT)
            eglDestroyContext(display, context);
        if (surface != EGL_NO_SURFACE)
            eglDestroySurface(display, surface);
        eglTerminate(display);
    }
};
} // namespace

int main(int argc, char **argv)
{
    using namespace hui;
    if (argc != 2)
        return 1;
    Egl egl;
    if (!egl.open())
        return 2;
    gfx::set_glsl_prefix("#version 450 core\n");
    gfx::Renderer renderer;
    gfx::Font regular;
    std::string bytes;
    if (!renderer.init() ||
        !save::read_file(std::string(argv[1]) + "/fonts/inter-regular.huifont", &bytes) ||
        !regular.load(bytes))
        return 3;
    const ui::FontRef face{&regular, renderer.batch().create_font_texture(regular)};
    ui::Fonts fonts{face, face, face, face, face, face, face};
    gfx::DrawList list;
    ui::Canvas canvas{list, fonts};
    ui::Feedback feedback;
    ui::Form settings;
    settings.style.theme = *ui::find_theme("cinema");
    settings.set_bounds({96, 190, 1000, 640});
    settings.add_toggle(1, "Play menu sounds", true);
    settings.add_action(2, "Open display details");
    settings.set_active(true);
    ui::ArtworkDetailHeader details;
    details.style.theme = settings.style.theme;
    details.title = "Display details";
    details.metadata = "1920 x 1080 virtual canvas";
    details.action = "Back";
    bool detail_open = false;

    const auto tick = [&](InputFrame input)
    {
        if (detail_open)
        {
            if (input.is_pressed(Action::back) ||
                details.handle(input, feedback) == ui::Event::activated)
            {
                detail_open = false;
                settings.set_active(true);
            }
        }
        else if (settings.handle(input, feedback) == ui::Event::activated)
        {
            detail_open = true;
            settings.set_active(false);
            details.set_active(true);
        }
        settings.update(1.0f / 60.0f);
        details.update(1.0f / 60.0f);
        list.clear();
        if (detail_open)
            details.draw(canvas);
        else
            settings.draw(canvas);
        renderer.begin();
        renderer.backdrop(settings.style.theme.backdrop);
        renderer.draw(list);
        renderer.present(0, 960, 540);
        eglSwapBuffers(egl.display, egl.surface);
        return glGetError() == GL_NO_ERROR && !list.empty();
    };

    InputFrame input;
    input.nav = Direction::down;
    if (!tick(input) || settings.focus() != 1)
        return 4;
    input = {};
    input.pressed = action_bit(Action::confirm);
    if (!tick(input) || !detail_open)
        return 5;
    input.pressed = action_bit(Action::back);
    if (!tick(input) || detail_open || settings.focus() != 1)
        return 6;
    std::puts("Settings to details and back preserved the focused control.");
}
