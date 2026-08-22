/*
    Copyright 2021 natinusala

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include <borealis/core/application.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/platforms/sdl/sdl_video.hpp>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#ifdef BOREALIS_USE_OPENGL
#ifdef __PSV__
#include <GLES2/gl2.h>
extern "C"
{
#include <gpu_es4/psp2_pvr_hint.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/sysmem.h>
}
#define NANOVG_GLES2_IMPLEMENTATION
#elif defined(PS4)
#include <orbis/Pigletv2VSH.h>
#define NANOVG_GLES2_IMPLEMENTATION
#else
#include <glad/glad.h>
#ifdef USE_GLES2
#define NANOVG_GLES2_IMPLEMENTATION
#elif USE_GLES3
#define NANOVG_GLES3_IMPLEMENTATION
#elif USE_GL2
#define NANOVG_GL2_IMPLEMENTATION
#else
#define NANOVG_GL3_IMPLEMENTATION
#endif
#endif
#include <nanovg_gl.h>
#elif defined(BOREALIS_USE_D3D11)
#include <nanovg_d3d11.h>

#include <borealis/platforms/driver/d3d11.hpp>
std::unique_ptr<brls::D3D11Context> D3D11_CONTEXT;
#elif defined(BOREALIS_USE_METAL)
#if defined(__SDL3__)
#include <SDL3/SDL_metal.h>
#else
#include <SDL_metal.h>
#endif
#include <nanovg_mtl.h>
#endif

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace brls
{

static double scaleFactor = 1.0;

#ifdef BOREALIS_USE_METAL
static SDL_MetalView metalView = nullptr;
#endif

#if defined(BOREALIS_USE_D3D11)
static void sdlGetWindowPixelSize(SDL_Window* window, int* width, int* height)
{
#if SDL_VERSION_ATLEAST(2, 26, 0)
    SDL_GetWindowSizeInPixels(window, width, height);
#else
    SDL_GetWindowSize(window, width, height);

    if (D3D11_CONTEXT != nullptr)
    {
        const double currentScale = D3D11_CONTEXT->getScaleFactor();
        *width                    = static_cast<int>(std::lround(*width * currentScale));
        *height                   = static_cast<int>(std::lround(*height * currentScale));
    }
#endif
}
#endif

static void sdlWindowFramebufferSizeCallback(SDL_Window* window, int width, int height)
{
    if (!width || !height)
        return;

    int fWidth, fHeight;
#ifdef BOREALIS_USE_OPENGL
#if defined(__SDL3__)
    SDL_GetWindowSizeInPixels(window, &fWidth, &fHeight);
#else
    SDL_GL_GetDrawableSize(window, &fWidth, &fHeight);
#endif
    scaleFactor = fWidth * 1.0 / width;
#if defined(ANDROID)
    // On Android, doing this is to ensure that glViewport is called from the main thread
    brls::sync([fWidth, fHeight]()
        { glViewport(0, 0, fWidth, fHeight); });
#else
    glViewport(0, 0, fWidth, fHeight);
#endif
#elif defined(BOREALIS_USE_METAL)
#if defined(__SDL3__)
    SDL_GetWindowSizeInPixels(window, &fWidth, &fHeight);
#else
    SDL_Metal_GetDrawableSize(window, &fWidth, &fHeight);
#endif
    scaleFactor = fWidth * 1.0 / width;
    fWidth      = width;
    fHeight     = height;
#elif defined(BOREALIS_USE_D3D11)
    sdlGetWindowPixelSize(window, &fWidth, &fHeight);
    scaleFactor = fWidth * 1.0 / width;
    D3D11_CONTEXT->onFramebufferSize(fWidth, fHeight);
#endif

    Application::onWindowResized(fWidth, fHeight);

    if (!VideoContext::FULLSCREEN)
    {
        VideoContext::sizeW = width;
        VideoContext::sizeH = height;
    }
}

static void sdlWindowPositionCallback(SDL_Window* window, int windowXPos, int windowYPos)
{
    Application::onWindowReposition(windowXPos, windowYPos);

    if (!VideoContext::FULLSCREEN)
    {
        VideoContext::posX = (float)windowXPos;
        VideoContext::posY = (float)windowYPos;
    }
}

#if defined(__SDL3__)
static bool sdlWindowEventWatcher(void* data, SDL_Event* event)
#else
static int sdlWindowEventWatcher(void* data, SDL_Event* event)
#endif
{
#if defined(__SDL3__)
    if (event->type == SDL_EVENT_WINDOW_RESIZED || event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
    {
        SDL_Window* win = SDL_GetWindowFromID(event->window.windowID);
        if (win == (SDL_Window*)data)
        {
            int width, height;
            SDL_GetWindowSize(win, &width, &height);
            sdlWindowFramebufferSizeCallback(win, width, height);
        }
    }
    else if (event->type == SDL_EVENT_WINDOW_MOVED)
    {
        SDL_Window* win = SDL_GetWindowFromID(event->window.windowID);
        if (win == (SDL_Window*)data)
            sdlWindowPositionCallback(win, event->window.data1, event->window.data2);
    }
    return true;
#else
    if (event->type == SDL_WINDOWEVENT)
    {
        SDL_Window* win = SDL_GetWindowFromID(event->window.windowID);
        switch (event->window.event)
        {
            case SDL_WINDOWEVENT_RESIZED:
                if (win == (SDL_Window*)data)
                {
                    sdlWindowFramebufferSizeCallback(win,
                        event->window.data1,
                        event->window.data2);
                }
                break;
            case SDL_WINDOWEVENT_MOVED:
                if (win == (SDL_Window*)data)
                {
                    sdlWindowPositionCallback(win,
                        event->window.data1,
                        event->window.data2);
                }
                break;
        }
    }
    return 0;
#endif
}

SDLVideoContext::SDLVideoContext(std::string windowTitle, uint32_t windowWidth, uint32_t windowHeight, float windowXPos, float windowYPos)
{
#ifdef __PSV__
#define MAX_PATH 256
    /// Huge thanks to SonicMastr for his kindness help and contribution in psv homebrew.

    windowWidth  = 960;
    windowHeight = 544;

    /* Disable Back Touchpad to prevent "misclicks" */
    SDL_setenv("VITA_DISABLE_TOUCH_BACK", "1", 1);

    /* We need to use some custom hints */
    SDL_setenv("VITA_PVR_SKIP_INIT", "yeet", 1);

    PVRSRV_PSP2_APPHINT hint;
    char target_path[MAX_PATH];
    const char* default_path = "app0:module";

    auto logFreeMemory = []()
    {
        SceKernelFreeMemorySizeInfo memoryInfo {};
        memoryInfo.size = sizeof(memoryInfo);
        int result      = sceKernelGetFreeMemorySize(&memoryInfo);
        Logger::info("sdl: Vita free memory result=0x{:08x}, user={} KiB, CDRAM={} KiB, phycont={} KiB",
            static_cast<unsigned int>(result),
            memoryInfo.size_user / 1024,
            memoryInfo.size_cdram / 1024,
            memoryInfo.size_phycont / 1024);
    };
    auto loadModule = [](const char* path)
    {
        int result = sceKernelLoadStartModule(path, 0, NULL, 0, NULL, NULL);
        Logger::info("sdl: Vita module {} load result=0x{:08x}", path, static_cast<unsigned int>(result));
    };

    logFreeMemory();

    /* Load Modules */
    loadModule("vs0:sys/external/libfios2.suprx");
    loadModule("vs0:sys/external/libc.suprx");
    snprintf(target_path, MAX_PATH, "%s/%s", default_path, "libgpu_es4_ext.suprx");
    loadModule(target_path);
    snprintf(target_path, MAX_PATH, "%s/%s", default_path, "libIMGEGL.suprx");
    loadModule(target_path);

    /* Set PVR Hints */
    unsigned int initializeHintResult = PVRSRVInitializeAppHint(&hint);
    snprintf(hint.szGLES1, MAX_PATH, "%s/%s", default_path, "libGLESv1_CM.suprx");
    snprintf(hint.szGLES2, MAX_PATH, "%s/%s", default_path, "libGLESv2.suprx");
    snprintf(hint.szWindowSystem, MAX_PATH, "%s/%s", default_path, "libpvrPSP2_WSEGL.suprx");

    hint.ui32SwTexOpCleanupDelay = 32000; // Set to 32 milliseconds to prevent a pool of unfreed memory
    unsigned int createHintResult = PVRSRVCreateVirtualAppHint(&hint);
    Logger::info("sdl: Vita PVR hints initialize={}, create={}", initializeHintResult, createHintResult);
    logFreeMemory();
#endif

#if defined(_WIN32) && !defined(__SDL3__)
    if (!SDL_GetHint(SDL_HINT_WINDOWS_DPI_AWARENESS))
    {
        SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    }
    if (!SDL_GetHint(SDL_HINT_WINDOWS_DPI_SCALING))
    {
        SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
    }
#endif

#if defined(__SDL3__)
    if (!SDL_Init(SDL_INIT_VIDEO))
#else
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
#endif
    {
        Logger::error("sdl: failed to initialize");
        return;
    }

    // Create window
#if defined(__SDL3__)
    SDL_WindowFlags windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
#else
    Uint32 windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;
#endif
#ifdef BOREALIS_USE_OPENGL
#ifdef __SWITCH__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_RETAINED_BACKING, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#elif defined(__PSV__)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_RETAINED_BACKING, 0);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#elif defined(USE_GLES2)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_RETAINED_BACKING, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#elif defined(USE_GLES3)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_RETAINED_BACKING, 0);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#elif defined(USE_GL2)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_RETAINED_BACKING, 0);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 6);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
#endif
#if defined(USE_EGL)
#if defined(__SDL3__)
    SDL_SetHint(SDL_HINT_VIDEO_FORCE_EGL, "1");
#else
    SDL_SetHint(SDL_HINT_VIDEO_X11_FORCE_EGL, "1");
#endif
#endif
    windowFlags |= SDL_WINDOW_OPENGL;
#elif defined(BOREALIS_USE_METAL)
    windowFlags |= SDL_WINDOW_METAL;
#endif
    if (VideoContext::FULLSCREEN)
    {
#ifdef __WINRT__
        windowFlags |= SDL_WINDOW_FULLSCREEN;
#else
#if defined(__SDL3__)
        windowFlags |= SDL_WINDOW_FULLSCREEN;
#else
        windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
#endif
#endif
    }
#if !defined(__SDL3__)
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

#if defined(__SDL3__)
    this->window = SDL_CreateWindow(windowTitle.c_str(), windowWidth, windowHeight, windowFlags);
    if (this->window && !std::isnan(windowXPos) && !std::isnan(windowYPos))
    {
        SDL_SetWindowPosition(this->window,
            windowXPos > 0 ? windowXPos : SDL_WINDOWPOS_UNDEFINED,
            windowYPos > 0 ? windowYPos : SDL_WINDOWPOS_UNDEFINED);
    }
#else
    if (std::isnan(windowXPos) || std::isnan(windowYPos))
    {
        this->window = SDL_CreateWindow(windowTitle.c_str(),
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            windowWidth,
            windowHeight,
            windowFlags);
    }
    else
    {
        this->window = SDL_CreateWindow(windowTitle.c_str(),
            windowXPos > 0 ? windowXPos : SDL_WINDOWPOS_UNDEFINED,
            windowYPos > 0 ? windowYPos : SDL_WINDOWPOS_UNDEFINED,
            windowWidth,
            windowHeight,
            windowFlags);
    }
#endif

    if (!this->window)
    {
        fatal("sdl: failed to create window: " + std::string(SDL_GetError()));
    }
#ifdef BOREALIS_USE_OPENGL
    // Configure window
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context)
    {
        fatal("sdl: failed to create OpenGL context: " + std::string(SDL_GetError()));
    }
#if defined(__SDL3__)
    if (!SDL_GL_MakeCurrent(window, context))
#else
    if (SDL_GL_MakeCurrent(window, context) < 0)
#endif
    {
        fatal("sdl: failed to activate OpenGL context: " + std::string(SDL_GetError()));
    }
#elif defined(BOREALIS_USE_METAL)
    metalView = SDL_Metal_CreateView(window);
    if (!metalView)
    {
        fatal("sdl: failed to create metal view");
    }
#endif
    SDL_AddEventWatch(sdlWindowEventWatcher, window);
#ifdef BOREALIS_USE_OPENGL
#if !defined(__PSV__) && !defined(PS4)
    // Load OpenGL routines using glad
    gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress);
#endif

    const char* glVendor   = (const char*)glGetString(GL_VENDOR);
    const char* glRenderer = (const char*)glGetString(GL_RENDERER);
    const char* glVersion  = (const char*)glGetString(GL_VERSION);
    Logger::info("sdl: GL Vendor: {}", glVendor ? glVendor : "<unavailable>");
    Logger::info("sdl: GL Renderer: {}", glRenderer ? glRenderer : "<unavailable>");
    Logger::info("sdl: GL Version: {}", glVersion ? glVersion : "<unavailable>");

    // Initialize nanovg
#ifdef __PSV__
    this->nvgContext = nvgCreateGLES2(0);
#elif PS4
    // Same as GLES2, but with pre-compiled shaders, so the flags must be "NVG_STENCIL_STROKES | NVG_ANTIALIAS" now
    this->nvgContext = nvgCreateGLES2(NVG_STENCIL_STROKES | NVG_ANTIALIAS);
#elif USE_GLES2
    this->nvgContext = nvgCreateGLES2(NVG_STENCIL_STROKES | NVG_ANTIALIAS);
#elif USE_GLES3
    this->nvgContext = nvgCreateGLES3(NVG_STENCIL_STROKES | NVG_ANTIALIAS);
#elif USE_GL2
    this->nvgContext = nvgCreateGL2(NVG_STENCIL_STROKES | NVG_ANTIALIAS);
#else
    this->nvgContext = nvgCreateGL3(NVG_STENCIL_STROKES | NVG_ANTIALIAS);
#endif
#elif defined(BOREALIS_USE_METAL)
    void* metalLayer = SDL_Metal_GetLayer(metalView);
    if (!metalLayer)
    {
        fatal("sdl: failed to get metal layer");
    }

    this->nvgContext = nvgCreateMTL(metalLayer, NVG_STENCIL_STROKES | NVG_ANTIALIAS);
#elif defined(BOREALIS_USE_D3D11)
    Logger::info("sdl: USE_D3D11");
    D3D11_CONTEXT    = std::make_unique<D3D11Context>(this->window, windowWidth, windowHeight);
    this->nvgContext = nvgCreateD3D11(D3D11_CONTEXT->getDevice(), NVG_ANTIALIAS | NVG_STENCIL_STROKES);
#endif
    if (!this->nvgContext)
    {
        brls::fatal("sdl: unable to init nanovg");
    }

    setSwapInterval(VideoContext::swapInterval);

    // Setup window state
    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    int fWidth, fHeight;
#ifdef BOREALIS_USE_OPENGL
#if defined(__SDL3__)
    SDL_GetWindowSizeInPixels(window, &fWidth, &fHeight);
#else
    SDL_GL_GetDrawableSize(window, &fWidth, &fHeight);
#endif
    scaleFactor = fWidth * 1.0 / width;
    Application::setWindowSize(fWidth, fHeight);
    glViewport(0, 0, fWidth, fHeight);
#elif defined(BOREALIS_USE_METAL)
#if defined(__SDL3__)
    SDL_GetWindowSizeInPixels(window, &fWidth, &fHeight);
#else
    SDL_Metal_GetDrawableSize(window, &fWidth, &fHeight);
#endif
    scaleFactor = fWidth * 1.0 / width;
    Application::setWindowSize(width, height);
#elif defined(BOREALIS_USE_D3D11)
    sdlGetWindowPixelSize(window, &fWidth, &fHeight);
    scaleFactor      = fWidth * 1.0 / width;
    Application::setWindowSize(fWidth, fHeight);
    D3D11_CONTEXT->onFramebufferSize(fWidth, fHeight);
#endif

    int xPos, yPos;
    SDL_GetWindowPosition(window, &xPos, &yPos);
    Application::setWindowPosition(xPos, yPos);

    if (!VideoContext::FULLSCREEN)
    {
        VideoContext::sizeW = width;
        VideoContext::sizeH = height;
        VideoContext::posX  = (float)xPos;
        VideoContext::posY  = (float)yPos;
    }
}

void SDLVideoContext::beginFrame()
{
#ifdef  __SWITCH__ // TODO: Not work, needs to be implemented properly to apply correct screen resolution on Switch (example in GLFW video)
    s32 width = 0, height = 0;
    static s32 oldWidth = 0, oldHeight = 0;
    appletGetDefaultDisplayResolution(&width, &height);

    if (oldWidth != width || oldHeight != height)
    {
        oldWidth  = width;
        oldHeight = height;

        brls::Logger::info("Resolution chaned: {} / {}", oldWidth, oldHeight);

        SDL_SetWindowSize(window, width, height);
        Application::setWindowSize(width, height);
    }
#elif defined(BOREALIS_USE_D3D11)
    D3D11_CONTEXT->beginFrame();
#endif
}

void SDLVideoContext::endFrame()
{
#ifdef BOREALIS_USE_OPENGL
    SDL_GL_SwapWindow(this->window);
#elif defined(BOREALIS_USE_D3D11)
    D3D11_CONTEXT->endFrame();
#endif
}

void SDLVideoContext::setSwapInterval(int interval)
{
    VideoContext::swapInterval = interval;
#ifdef BOREALIS_USE_OPENGL
    SDL_GL_SetSwapInterval(interval);
#elif defined(BOREALIS_USE_D3D11)
    D3D11_CONTEXT->setSwapInterval(interval);
#endif
}

void SDLVideoContext::clear(NVGcolor color)
{
#ifdef BOREALIS_USE_OPENGL
    glClearColor(
        color.r,
        color.g,
        color.b,
        color.a);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
#elif defined(BOREALIS_USE_METAL)
    mnvgClearWithColor(this->nvgContext, color);
#elif defined(BOREALIS_USE_D3D11)
    D3D11_CONTEXT->clear(nvgRGBAf(
        color.r,
        color.g,
        color.b,
        color.a));
#endif
}

void SDLVideoContext::resetState()
{
#ifdef BOREALIS_USE_OPENGL
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
#endif
}

double SDLVideoContext::getScaleFactor()
{
    return scaleFactor;
}

SDLVideoContext::~SDLVideoContext()
{
#if defined(__SDL3__)
    SDL_RemoveEventWatch(sdlWindowEventWatcher, this->window);
#else
    SDL_DelEventWatch(sdlWindowEventWatcher, this->window);
#endif
    try
    {
        if (this->nvgContext)
        {
#ifdef BOREALIS_USE_OPENGL
#ifdef USE_GLES2
            nvgDeleteGLES2(this->nvgContext);
#elif USE_GLES3
            nvgDeleteGLES3(this->nvgContext);
#elif USE_GL2
            nvgDeleteGL2(this->nvgContext);
#else
            nvgDeleteGL3(this->nvgContext);
#endif
#elif defined(BOREALIS_USE_METAL)
            nvgDeleteMTL(this->nvgContext);
#elif defined(BOREALIS_USE_D3D11)
            nvgDeleteD3D11(this->nvgContext);
            D3D11_CONTEXT = nullptr;
#endif
        }
    }
    catch (...)
    {
        Logger::error("Cannot delete nvg Context");
    }
#ifdef BOREALIS_USE_METAL
    if (metalView)
    {
        SDL_Metal_DestroyView(metalView);
        metalView = nullptr;
    }
#endif
    SDL_DestroyWindow(this->window);
    SDL_Quit();
}

NVGcontext* SDLVideoContext::getNVGContext()
{
    return this->nvgContext;
}

SDL_Window* SDLVideoContext::getSDLWindow()
{
    return this->window;
}

#ifdef BOREALIS_USE_D3D11
D3D11Context* SDLVideoContext::getD3D11Context()
{
    return D3D11_CONTEXT.get();
}
#endif

void SDLVideoContext::fullScreen(bool fs)
{
#if defined(__SDL3__)
    SDL_SetWindowFullscreen(this->window, fs);
#else
#ifdef __WINRT__
    // win32 会很模糊，而且点击事件貌似也错位了，只给 winrt 使用。
    static unsigned int flag = SDL_WINDOW_FULLSCREEN;
#else
    static unsigned int flag = SDL_WINDOW_FULLSCREEN_DESKTOP;
#endif
    SDL_SetWindowFullscreen(this->window, fs ? flag : 0);
#endif
}

} // namespace brls
