#include "platform.h"

#if defined(__APPLE__)
    #include "platform_cocoa.h"
#elif defined(__linux__)
    #include "platform_wayland.h"
    #if defined(HAVE_X11_BACKEND)
        #include "platform_x11.h"
    #endif

    #include <std/lib/buffer.h>
    #include <std/str/view.h>
    #include <std/sys/throw.h>

    #include <cerrno>
    #include <cstdlib>
#else
    #error Unsupported platform
#endif

using namespace plt;

#if defined(__linux__)
namespace {
    bool environmentSet(const char* name) {
        const char* const value = getenv(name);
        return value != nullptr && value[0] != 0;
    }
}
#endif

Platform* Platform::create(stl::ObjPool& owner) {
#if defined(__APPLE__)
    return createCocoaPlatform(owner);
#elif defined(__linux__)
    // Wayland is authoritative when its display name or inherited connection
    // is present (as is normal in a Wayland session with XWayland). An
    // explicitly selected backend reports its own connection error instead of
    // silently opening a window on a different display server.
    if (environmentSet("WAYLAND_DISPLAY") || environmentSet("WAYLAND_SOCKET")) {
        return createWaylandPlatform(owner);
    }
    if (environmentSet("DISPLAY")) {
    #if defined(HAVE_X11_BACKEND)
        return createX11Platform(owner);
    #else
        stl::Errno(ENOTSUP).raise(stl::StringView(u8"DISPLAY is set, but this binary was built without X11 support"));
    #endif
    }
    stl::Errno(ENOENT).raise(stl::StringView(u8"Linux requires Wayland or X11; set WAYLAND_DISPLAY (or WAYLAND_SOCKET) for Wayland or DISPLAY for X11"));
#else
    #error Unsupported platform
#endif
}
