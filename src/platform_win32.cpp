#include "gl.h"
#include <GL/wglext.h>
#include <mmsystem.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "platform.h"

static HWND hwnd;
static HDC hdc;
static HGLRC glrc;
static bool closing;
static bool focus_lost;
static int client_w, client_h;
static WINDOWPLACEMENT prev_placement = { sizeof(WINDOWPLACEMENT) };
static LARGE_INTEGER perf_freq, perf_start;

// wndproc gets WM_CHAR and WM_MOUSEWHEEL, it needs somewhere to put them
static Input* cur_input;

static int map_key(WPARAM vk)
{
    if (vk >= 'A' && vk <= 'Z') return KEY_A + int(vk - 'A');
    if (vk >= '0' && vk <= '9') return KEY_0 + int(vk - '0');
    if (vk >= VK_F1 && vk <= VK_F12) return KEY_F1 + int(vk - VK_F1);
    switch (vk) {
    case VK_LEFT:    return KEY_LEFT;
    case VK_RIGHT:   return KEY_RIGHT;
    case VK_UP:      return KEY_UP;
    case VK_DOWN:    return KEY_DOWN;
    case VK_SPACE:   return KEY_SPACE;
    case VK_RETURN:  return KEY_ENTER;
    case VK_ESCAPE:  return KEY_ESCAPE;
    case VK_TAB:     return KEY_TAB;
    case VK_BACK:    return KEY_BACKSPACE;
    case VK_DELETE:  return KEY_DELETE;
    case VK_HOME:    return KEY_HOME;
    case VK_END:     return KEY_END;
    case VK_PRIOR:   return KEY_PAGEUP;
    case VK_NEXT:    return KEY_PAGEDOWN;
    case VK_SHIFT:   return KEY_SHIFT;
    case VK_CONTROL: return KEY_CTRL;
    case VK_MENU:    return KEY_ALT;
    }
    return KEY_NONE;
}

void gamepad_init_win32();
void gamepad_poll_win32(Input* in);

static LRESULT CALLBACK wndproc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CLOSE:
        closing = true;
        return 0;
    case WM_SIZE:
        client_w = LOWORD(lp);
        client_h = HIWORD(lp);
        return 0;
    case WM_KILLFOCUS:
        focus_lost = true;
        break;
    case WM_CHAR:
        // stored as utf-8, so accents typed in the editor survive
        if (cur_input && wp >= 32 && wp != 127 && (wp < 0xd800 || wp > 0xdfff)) {
            char buf[3];
            int n;
            if (wp < 0x80) {
                buf[0] = (char)wp;
                n = 1;
            } else if (wp < 0x800) {
                buf[0] = (char)(0xc0 | (wp >> 6));
                buf[1] = (char)(0x80 | (wp & 0x3f));
                n = 2;
            } else {
                buf[0] = (char)(0xe0 | (wp >> 12));
                buf[1] = (char)(0x80 | ((wp >> 6) & 0x3f));
                buf[2] = (char)(0x80 | (wp & 0x3f));
                n = 3;
            }
            if (cur_input->text_len + n < (int)sizeof(cur_input->text)) {
                for (int i = 0; i < n; i++)
                    cur_input->text[cur_input->text_len++] = buf[i];
            }
        }
        return 0;
    case WM_MOUSEWHEEL:
        if (cur_input)
            cur_input->wheel += GET_WHEEL_DELTA_WPARAM(wp) / (float)WHEEL_DELTA;
        return 0;
    }
    return DefWindowProcA(h, msg, wp, lp);
}

static bool create_context()
{
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;

    int pf = ChoosePixelFormat(hdc, &pfd);
    if (!pf || !SetPixelFormat(hdc, pf, &pfd)) {
        fprintf(stderr, "no usable pixel format\n");
        return false;
    }

    // wglCreateContextAttribsARB can only be fetched with a context current,
    // so make an old style one first and throw it away after
    HGLRC legacy = wglCreateContext(hdc);
    if (!legacy || !wglMakeCurrent(hdc, legacy)) {
        fprintf(stderr, "wglCreateContext failed\n");
        return false;
    }

    auto create_attribs = (PFNWGLCREATECONTEXTATTRIBSARBPROC)(void*)wglGetProcAddress("wglCreateContextAttribsARB");
    if (!create_attribs) {
        fprintf(stderr, "wglCreateContextAttribsARB not supported\n");
        return false;
    }

    int attrs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };
    glrc = create_attribs(hdc, 0, attrs);
    wglMakeCurrent(0, 0);
    wglDeleteContext(legacy);

    if (!glrc || !wglMakeCurrent(hdc, glrc)) {
        fprintf(stderr, "could not get a GL 3.3 core context\n");
        return false;
    }

    auto swap_interval = (PFNWGLSWAPINTERVALEXTPROC)(void*)wglGetProcAddress("wglSwapIntervalEXT");
    if (swap_interval)
        swap_interval(1);

    return gl_load();
}

bool platform_init(const char* title, int w, int h)
{
    SetProcessDPIAware();
    timeBeginPeriod(1);
    QueryPerformanceFrequency(&perf_freq);
    QueryPerformanceCounter(&perf_start);

    HINSTANCE inst = GetModuleHandleA(0);

    WNDCLASSA wc = {};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorA(0, IDC_ARROW);
    wc.hIcon = LoadIconA(0, IDI_APPLICATION);
    wc.lpszClassName = "kiln";
    RegisterClassA(&wc);

    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT r = { 0, 0, w, h };
    AdjustWindowRect(&r, style, FALSE);
    int ww = r.right - r.left;
    int wh = r.bottom - r.top;
    int x = (GetSystemMetrics(SM_CXSCREEN) - ww) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - wh) / 2;
    if (y < 0) y = 0;

    hwnd = CreateWindowExA(0, "kiln", title, style, x, y, ww, wh, 0, 0, inst, 0);
    if (!hwnd) {
        fprintf(stderr, "CreateWindow failed (%lu)\n", GetLastError());
        return false;
    }
    hdc = GetDC(hwnd);

    if (!create_context())
        return false;

    printf("GL %s, %s\n", (const char*)glGetString(GL_VERSION), (const char*)glGetString(GL_RENDERER));

    gamepad_init_win32();

    client_w = w;
    client_h = h;
    ShowWindow(hwnd, SW_SHOW);
    return true;
}

void platform_shutdown()
{
    wglMakeCurrent(0, 0);
    if (glrc) wglDeleteContext(glrc);
    if (hdc) ReleaseDC(hwnd, hdc);
    if (hwnd) DestroyWindow(hwnd);
    glrc = 0;
    hdc = 0;
    hwnd = 0;
    timeEndPeriod(1);
}

static void mouse_button(Input* in, int b, bool down)
{
    if (down && !in->mouse_down[b])
        in->mouse_pressed[b] = true;
    if (!down && in->mouse_down[b])
        in->mouse_released[b] = true;
    in->mouse_down[b] = down;

    // keep getting mouse ups when a drag leaves the window
    bool any = in->mouse_down[0] || in->mouse_down[1] || in->mouse_down[2];
    if (any)
        SetCapture(hwnd);
    else
        ReleaseCapture();
}

bool platform_poll(Input* in)
{
    cur_input = in;

    MSG msg;
    while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
        switch (msg.message) {
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP: {
            bool down = msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN;
            int k = map_key(msg.wParam);
            if (k != KEY_NONE) {
                if (down && !in->down[k])
                    in->pressed[k] = true;
                if (!down && in->down[k])
                    in->released[k] = true;
                in->down[k] = down;
            }
            // WM_CHAR comes out of TranslateMessage. alt+f4 needs DefWindowProc
            TranslateMessage(&msg);
            if (msg.message == WM_SYSKEYDOWN || msg.message == WM_SYSKEYUP)
                DispatchMessageA(&msg);
            break;
        }
        case WM_LBUTTONDOWN: mouse_button(in, MOUSE_LEFT, true); break;
        case WM_LBUTTONUP:   mouse_button(in, MOUSE_LEFT, false); break;
        case WM_RBUTTONDOWN: mouse_button(in, MOUSE_RIGHT, true); break;
        case WM_RBUTTONUP:   mouse_button(in, MOUSE_RIGHT, false); break;
        case WM_MBUTTONDOWN: mouse_button(in, MOUSE_MIDDLE, true); break;
        case WM_MBUTTONUP:   mouse_button(in, MOUSE_MIDDLE, false); break;
        default:
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    // otherwise a key held while alt-tabbing stays down forever
    if (focus_lost) {
        memset(in->down, 0, sizeof(in->down));
        memset(in->mouse_down, 0, sizeof(in->mouse_down));
        ReleaseCapture();
        focus_lost = false;
    }

    POINT p;
    GetCursorPos(&p);
    ScreenToClient(hwnd, &p);
    in->mouse_px = (float)p.x;
    in->mouse_py = (float)p.y;

    gamepad_poll_win32(in);

    cur_input = 0;
    return !closing;
}

void platform_swap()
{
    SwapBuffers(hdc);
}

void platform_window_size(int* w, int* h)
{
    *w = client_w;
    *h = client_h;
}

void platform_screen_size(int* w, int* h)
{
    RECT r;
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &r, 0);
    *w = r.right - r.left;
    *h = r.bottom - r.top;
}

// borderless fullscreen, the way Raymond Chen does it
void platform_toggle_fullscreen()
{
    DWORD style = GetWindowLongA(hwnd, GWL_STYLE);
    if (style & WS_OVERLAPPEDWINDOW) {
        MONITORINFO mi = { sizeof(mi) };
        if (GetWindowPlacement(hwnd, &prev_placement) &&
            GetMonitorInfoA(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLongA(hwnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    } else {
        SetWindowLongA(hwnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(hwnd, &prev_placement);
        SetWindowPos(hwnd, 0, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

bool platform_fullscreen()
{
    return !(GetWindowLongA(hwnd, GWL_STYLE) & WS_OVERLAPPEDWINDOW);
}

void platform_set_title(const char* title)
{
    SetWindowTextA(hwnd, title);
}

double platform_time()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return double(now.QuadPart - perf_start.QuadPart) / double(perf_freq.QuadPart);
}

void platform_sleep(int ms)
{
    Sleep(ms);
}
