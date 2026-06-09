#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <cstdint>
#include <string>
#include <memory>

namespace ezGui {

// WGL ARB tokens (normally provided by wglext.h, redefined here for a pure Win32 build)
#ifndef WGL_DRAW_TO_WINDOW_ARB
#define WGL_DRAW_TO_WINDOW_ARB           0x2001
#define WGL_ACCELERATION_ARB             0x2003
#define WGL_SUPPORT_OPENGL_ARB           0x2010
#define WGL_DOUBLE_BUFFER_ARB            0x2011
#define WGL_PIXEL_TYPE_ARB               0x2013
#define WGL_COLOR_BITS_ARB               0x2014
#define WGL_DEPTH_BITS_ARB               0x2022
#define WGL_STENCIL_BITS_ARB             0x2023
#define WGL_FULL_ACCELERATION_ARB        0x2027
#define WGL_TYPE_RGBA_ARB                0x202B
#endif // WGL_DRAW_TO_WINDOW_ARB
#ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
#define WGL_CONTEXT_MAJOR_VERSION_ARB    0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB    0x2092
#define WGL_CONTEXT_FLAGS_ARB            0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB     0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#endif // WGL_CONTEXT_MAJOR_VERSION_ARB

class Win32Window {
public:
    // ARB extension entry points, resolved once through a throwaway context
    using PfnWglCreateContextAttribsArb = HGLRC (WINAPI*)(HDC a_hdc, HGLRC a_share, const int32_t* ap_attribs);
    using PfnWglChoosePixelFormatArb = BOOL (WINAPI*)(HDC a_hdc, const int32_t* ap_int_attribs, const FLOAT* ap_float_attribs, UINT a_max_formats, int32_t* ap_formats, UINT* ap_format_count);

    static std::unique_ptr<Win32Window> create() {
        return std::unique_ptr<Win32Window>(new Win32Window());
    }
    ~Win32Window() {
        unit();
    }
    Win32Window(const Win32Window&) = delete;
    Win32Window& operator=(const Win32Window&) = delete;

    bool init(const std::string& a_title, int32_t a_width, int32_t a_height, int32_t a_gl_major = 3, int32_t a_gl_minor = 3) {
        m_hinstance = ::GetModuleHandleA(nullptr);
        if (!register_window_class()) {
            return false;
        }
        PfnWglCreateContextAttribsArb create_context_arb = nullptr;
        PfnWglChoosePixelFormatArb choose_format_arb = nullptr;
        if (!load_wgl_extensions(create_context_arb, choose_format_arb)) {
            return false;
        }
        if (!create_window(a_title, a_width, a_height)) {
            return false;
        }
        if (!create_gl_context(create_context_arb, choose_format_arb, a_gl_major, a_gl_minor)) {
            return false;
        }
        ::ShowWindow(m_hwnd, SW_SHOW);
        return true;
    }
    void unit() {
        if (m_hglrc != nullptr) {
            ::wglMakeCurrent(m_hdc, nullptr);
            ::wglDeleteContext(m_hglrc);
            m_hglrc = nullptr;
        }
        if ((m_hwnd != nullptr) && (m_hdc != nullptr)) {
            ::ReleaseDC(m_hwnd, m_hdc);
            m_hdc = nullptr;
        }
        if (m_hwnd != nullptr) {
            ::DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }
        // The window class is intentionally kept registered: it is shared across instances.
    }
    // Pumps every pending message; returns false once the window has been asked to close.
    bool process_events() {
        MSG message = {};
        while (::PeekMessageA(&message, nullptr, 0u, 0u, PM_REMOVE) != FALSE) {
            if (message.message == WM_QUIT) {
                m_should_close = true;
            }
            ::TranslateMessage(&message);
            ::DispatchMessageA(&message);
        }
        return !m_should_close;
    }
    void make_current() {
        ::wglMakeCurrent(m_hdc, m_hglrc);
    }
    void swap_buffers() {
        ::SwapBuffers(m_hdc);
    }
    HWND get_hwnd() const { return m_hwnd; }
    HDC get_hdc() const { return m_hdc; }
    HGLRC get_hglrc() const { return m_hglrc; }
    int32_t get_width() const { return m_width; }
    int32_t get_height() const { return m_height; }
    bool should_close() const { return m_should_close; }

private:
    Win32Window() = default;
    static LRESULT CALLBACK window_proc(HWND a_hwnd, UINT a_message, WPARAM a_wparam, LPARAM a_lparam) {
        if (a_message == WM_NCCREATE) {
            auto* ap_create = reinterpret_cast<CREATESTRUCTA*>(a_lparam);
            ::SetWindowLongPtrA(a_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ap_create->lpCreateParams));
            return ::DefWindowProcA(a_hwnd, a_message, a_wparam, a_lparam);
        }
        auto* ap_self = reinterpret_cast<Win32Window*>(::GetWindowLongPtrA(a_hwnd, GWLP_USERDATA));
        if (ap_self == nullptr) {
            return ::DefWindowProcA(a_hwnd, a_message, a_wparam, a_lparam);
        }
        switch (a_message) {
            case WM_SIZE: {
                ap_self->m_width = static_cast<int32_t>(LOWORD(a_lparam));
                ap_self->m_height = static_cast<int32_t>(HIWORD(a_lparam));
                return 0;
            }
            case WM_CLOSE: {
                ap_self->m_should_close = true;
                return 0;
            }
            case WM_DESTROY: {
                ::PostQuitMessage(0);
                return 0;
            }
            default: {
                return ::DefWindowProcA(a_hwnd, a_message, a_wparam, a_lparam);
            }
        }
    }
    const char* class_name() const {
        return "ezGLWin32WindowClass";
    }
    bool register_window_class() {
        WNDCLASSEXA window_class = {};
        window_class.cbSize = sizeof(WNDCLASSEXA);
        window_class.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
        window_class.lpfnWndProc = &Win32Window::window_proc;
        window_class.hInstance = m_hinstance;
        window_class.hCursor = ::LoadCursorA(nullptr, IDC_ARROW);
        window_class.lpszClassName = class_name();
        if (::RegisterClassExA(&window_class) == 0) {
            // Any failure other than an already-registered class is fatal.
            return ::GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
        }
        return true;
    }
    // Creates a throwaway window and legacy context to resolve the WGL ARB entry points.
    bool load_wgl_extensions(PfnWglCreateContextAttribsArb& ao_create_context, PfnWglChoosePixelFormatArb& ao_choose_format) {
        HWND dummy_hwnd = ::CreateWindowExA(0, class_name(), "dummy", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1, 1, nullptr, nullptr, m_hinstance, nullptr);
        if (dummy_hwnd == nullptr) {
            return false;
        }
        HDC dummy_hdc = ::GetDC(dummy_hwnd);
        PIXELFORMATDESCRIPTOR descriptor = {};
        descriptor.nSize = sizeof(PIXELFORMATDESCRIPTOR);
        descriptor.nVersion = 1;
        descriptor.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        descriptor.iPixelType = PFD_TYPE_RGBA;
        descriptor.cColorBits = 32;
        descriptor.cDepthBits = 24;
        descriptor.cStencilBits = 8;
        const int32_t dummy_format = ::ChoosePixelFormat(dummy_hdc, &descriptor);
        ::SetPixelFormat(dummy_hdc, dummy_format, &descriptor);
        HGLRC dummy_context = ::wglCreateContext(dummy_hdc);
        ::wglMakeCurrent(dummy_hdc, dummy_context);
        ao_create_context = reinterpret_cast<PfnWglCreateContextAttribsArb>(::wglGetProcAddress("wglCreateContextAttribsARB"));
        ao_choose_format = reinterpret_cast<PfnWglChoosePixelFormatArb>(::wglGetProcAddress("wglChoosePixelFormatARB"));
        ::wglMakeCurrent(dummy_hdc, nullptr);
        ::wglDeleteContext(dummy_context);
        ::ReleaseDC(dummy_hwnd, dummy_hdc);
        ::DestroyWindow(dummy_hwnd);
        return (ao_create_context != nullptr) && (ao_choose_format != nullptr);
    }
    bool create_window(const std::string& a_title, int32_t a_width, int32_t a_height) {
        RECT rectangle = {0, 0, a_width, a_height};
        ::AdjustWindowRect(&rectangle, WS_OVERLAPPEDWINDOW, FALSE);
        m_hwnd = ::CreateWindowExA(0, class_name(), a_title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rectangle.right - rectangle.left, rectangle.bottom - rectangle.top, nullptr, nullptr, m_hinstance, this);
        if (m_hwnd == nullptr) {
            return false;
        }
        m_hdc = ::GetDC(m_hwnd);
        m_width = a_width;
        m_height = a_height;
        return m_hdc != nullptr;
    }
    bool create_gl_context(PfnWglCreateContextAttribsArb a_create_context, PfnWglChoosePixelFormatArb a_choose_format, int32_t a_gl_major, int32_t a_gl_minor) {
        const int32_t format_attribs[] = {
            WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
            WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
            WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
            WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
            WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
            WGL_COLOR_BITS_ARB, 32,
            WGL_DEPTH_BITS_ARB, 24,
            WGL_STENCIL_BITS_ARB, 8,
            0
        };
        int32_t chosen_format = 0;
        UINT format_count = 0;
        if (a_choose_format(m_hdc, format_attribs, nullptr, 1u, &chosen_format, &format_count) == FALSE) {
            return false;
        }
        if (format_count == 0) {
            return false;
        }
        PIXELFORMATDESCRIPTOR descriptor = {};
        ::DescribePixelFormat(m_hdc, chosen_format, sizeof(PIXELFORMATDESCRIPTOR), &descriptor);
        if (::SetPixelFormat(m_hdc, chosen_format, &descriptor) == FALSE) {
            return false;
        }
        const int32_t context_attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, a_gl_major,
            WGL_CONTEXT_MINOR_VERSION_ARB, a_gl_minor,
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            0
        };
        m_hglrc = a_create_context(m_hdc, nullptr, context_attribs);
        if (m_hglrc == nullptr) {
            return false;
        }
        ::wglMakeCurrent(m_hdc, m_hglrc);
        return true;
    }

    HINSTANCE m_hinstance = nullptr;
    HWND m_hwnd = nullptr;
    HDC m_hdc = nullptr;
    HGLRC m_hglrc = nullptr;
    int32_t m_width = 0;
    int32_t m_height = 0;
    bool m_should_close = false;
};

} // namespace ezGui
