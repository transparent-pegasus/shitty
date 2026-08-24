#include "clipboard.h"
#include "drop.h"
#include "fiber.h"
#include "input.h"
#include "platform.h"
#include "poller.h"
#include "window.h"

#include <std/ios/input.h>
#include <std/ios/output.h>
#include <std/mem/obj_pool.h>
#include <std/ptr/scoped.h>
#include <std/str/view.h>
#include <std/thr/poll_fd.h>
#include <std/thr/runable.h>

#include <xcb/xcb.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace stl;

namespace plt::test {
    namespace {
        constexpr u64 pumpTime = 50'000;
        constexpr u64 scenarioTimeout = 12'000'000;
        constexpr u64 peerTimeout = 3'000'000;

        bool failure(const char* scenario, const char* message) {
            fprintf(stderr, "%s: %s\n", scenario, message);
            return false;
        }

        struct XConnection {
            XConnection() {
                int screenNumber = 0;
                connection = xcb_connect(nullptr, &screenNumber);
                if (connection == nullptr || xcb_connection_has_error(connection) != 0) {
                    return;
                }
                xcb_screen_iterator_t iterator = xcb_setup_roots_iterator(xcb_get_setup(connection));
                for (int index = 0; index != screenNumber && iterator.rem != 0; ++index) {
                    xcb_screen_next(&iterator);
                }
                if (iterator.rem != 0) {
                    screen = iterator.data;
                }
            }

            ~XConnection() noexcept {
                if (connection != nullptr) {
                    xcb_disconnect(connection);
                }
            }

            bool valid() const {
                return connection != nullptr && screen != nullptr && xcb_connection_has_error(connection) == 0;
            }

            xcb_atom_t atom(const char* name) const {
                const xcb_intern_atom_cookie_t cookie = xcb_intern_atom(connection, 0, (u16)(strlen(name)), name);
                xcb_intern_atom_reply_t* const reply = xcb_intern_atom_reply(connection, cookie, nullptr);
                if (reply == nullptr) {
                    return XCB_ATOM_NONE;
                }
                const xcb_atom_t result = reply->atom;
                free(reply);
                return result;
            }

            bool sync() const {
                const xcb_get_input_focus_cookie_t cookie = xcb_get_input_focus(connection);
                xcb_get_input_focus_reply_t* const reply = xcb_get_input_focus_reply(connection, cookie, nullptr);
                const bool success = reply != nullptr && xcb_connection_has_error(connection) == 0;
                free(reply);
                return success;
            }

            xcb_connection_t* connection = nullptr;
            xcb_screen_t* screen = nullptr;
        };

        struct PoolOwner {
            PoolOwner()
                : pool(ObjPool::fromMemoryRaw()) {
            }

            ~PoolOwner() noexcept {
                delete pool;
            }

            void reset() {
                ObjPool* const released = pool;
                pool = nullptr;
                delete released;
            }

            ObjPool* pool;
        };

        struct Property {
            xcb_atom_t type = XCB_ATOM_NONE;
            u8 format = 0;
            u32 bytesAfter = 0;
            std::vector<u8> value;

            u32 word(size_t index) const {
                u32 result = 0;
                if ((index + 1) * sizeof(result) <= value.size()) {
                    memcpy(&result, value.data() + index * sizeof(result), sizeof(result));
                }
                return result;
            }
        };

        Property property(XConnection& peer, xcb_window_t window, xcb_atom_t name, bool remove = false) {
            Property result;
            const xcb_get_property_cookie_t cookie = xcb_get_property(peer.connection, remove, window, name, XCB_GET_PROPERTY_TYPE_ANY, 0, 4 * 1024 * 1024);
            xcb_get_property_reply_t* const reply = xcb_get_property_reply(peer.connection, cookie, nullptr);
            if (reply == nullptr) {
                return result;
            }
            result.type = reply->type;
            result.format = reply->format;
            result.bytesAfter = reply->bytes_after;
            const size_t length = (size_t)(xcb_get_property_value_length(reply));
            const u8* const data = (const u8*)(xcb_get_property_value(reply));
            if (length != 0) {
                result.value.assign(data, data + length);
            }
            free(reply);
            return result;
        }

        bool synchronize(xcb_connection_t* connection) {
            const xcb_get_input_focus_cookie_t cookie = xcb_get_input_focus(connection);
            xcb_get_input_focus_reply_t* const reply = xcb_get_input_focus_reply(connection, cookie, nullptr);
            const bool success = reply != nullptr && xcb_connection_has_error(connection) == 0;
            free(reply);
            return success;
        }

        template <typename Event>
        bool sendEvent(XConnection& peer, xcb_window_t destination, u32 mask, const Event& event) {
            const xcb_void_cookie_t cookie = xcb_send_event_checked(peer.connection, 0, destination, mask, (const char*)(&event));
            xcb_generic_error_t* const error = xcb_request_check(peer.connection, cookie);
            const bool success = error == nullptr;
            free(error);
            xcb_flush(peer.connection);
            return success && peer.sync();
        }

        struct StopTimer final: TimerCallback {
            explicit StopTimer(Platform& platform_)
                : platform(platform_) {
            }

            void ready() override {
                fired = true;
                platform.stop();
            }

            Platform& platform;
            bool fired = false;
        };

        void pump(Platform& platform, u64 timeout = pumpTime) {
            StopTimer timer(platform);
            platform.poller()->timeout(timeout, timer);
            platform.run();
            platform.poller()->cancel(timer);
        }

        template <typename Body>
        bool runOnFiber(Platform& platform, Body body) {
            bool complete = false;
            bool success = false;
            auto task = makeRunable([&] {
                success = body();
                complete = true;
                platform.stop();
            });
            ObjPool::Ref fiberOwner = ObjPool::fromMemory();
            platform.scheduler()->create(*fiberOwner, task, 256 * 1024);
            StopTimer deadline(platform);
            platform.poller()->timeout(scenarioTimeout, deadline);
            platform.run();
            platform.poller()->cancel(deadline);
            return complete && success && !deadline.fired;
        }

        struct InputRecorder final: InputSink {
            void key(const KeyInput& input) override {
                if (input.action == InputAction::Press) {
                    ++keyPresses;
                    pressedKey = input.key;
                } else if (input.action == InputAction::Release) {
                    ++keyReleases;
                }
            }

            void text(const TextInput& input) override {
                ++texts;
                codepoint = input.codepoint;
            }

            void preedit(StringView, i32, i32) override {
            }

            void pointerMotion(const PointerMotionInput& input) override {
                ++motions;
                pointerX = input.pixelX;
                pointerY = input.pixelY;
            }

            void pointerButton(const PointerButtonInput& input) override {
                if (input.pressed) {
                    ++buttonPresses;
                    button = input.button;
                } else {
                    ++buttonReleases;
                }
            }

            void scroll(const ScrollInput& input) override {
                ++scrolls;
                scrollX = input.x;
                scrollY = input.y;
            }

            void focus(bool focused) override {
                focused ? ++focuses : ++blurs;
            }

            void pointerPresence(bool present) override {
                present ? ++enters : ++leaves;
            }

            void flush() override {
                ++flushes;
            }

            InputKey pressedKey = InputKey::Unknown;
            PointerButton button = PointerButton::Secondary;
            u32 codepoint = 0;
            i32 pointerX = 0;
            i32 pointerY = 0;
            double scrollX = 0;
            double scrollY = 0;
            u32 keyPresses = 0;
            u32 keyReleases = 0;
            u32 texts = 0;
            u32 motions = 0;
            u32 buttonPresses = 0;
            u32 buttonReleases = 0;
            u32 scrolls = 0;
            u32 focuses = 0;
            u32 blurs = 0;
            u32 enters = 0;
            u32 leaves = 0;
            u32 flushes = 0;
        };

        struct WindowRecorder final: WindowEvents, FrameCallback {
            void close() override {
                ++closes;
            }

            bool frame(const WindowInfo& info) override {
                ++frames;
                last = info;
                return true;
            }

            WindowInfo last;
            u32 closes = 0;
            u32 frames = 0;
        };

        bool hasAtom(const Property& value, xcb_atom_t atom) {
            if (value.format != 32 || value.value.size() % sizeof(u32) != 0) {
                return false;
            }
            for (size_t index = 0; index != value.value.size() / sizeof(u32); ++index) {
                if (value.word(index) == atom) {
                    return true;
                }
            }
            return false;
        }

        bool windowInputAndProtocols() {
            constexpr const char* scenario = "X11 window/input";
            XConnection peer;
            if (!peer.valid()) {
                return failure(scenario, "could not open the helper XCB connection");
            }

            const xcb_atom_t resourceManager = peer.atom("RESOURCE_MANAGER");
            const char resources[] = "Xft.dpi:\t192\n";
            xcb_change_property(peer.connection, XCB_PROP_MODE_REPLACE, peer.screen->root, resourceManager, XCB_ATOM_STRING, 8, sizeof(resources) - 1, resources);
            if (!peer.sync()) {
                return failure(scenario, "could not seed RESOURCE_MANAGER");
            }
            const xcb_atom_t netWmName = peer.atom("_NET_WM_NAME");
            const xcb_atom_t utf8String = peer.atom("UTF8_STRING");
            const xcb_atom_t wmNormalHints = peer.atom("WM_NORMAL_HINTS");
            const xcb_atom_t wmSizeHints = peer.atom("WM_SIZE_HINTS");
            const xcb_atom_t wmProtocols = peer.atom("WM_PROTOCOLS");
            const xcb_atom_t wmDeleteWindow = peer.atom("WM_DELETE_WINDOW");
            const xcb_atom_t netWmState = peer.atom("_NET_WM_STATE");
            const xcb_atom_t netWmStateMaximizedVert = peer.atom("_NET_WM_STATE_MAXIMIZED_VERT");
            const xcb_atom_t netWmStateMaximizedHorz = peer.atom("_NET_WM_STATE_MAXIMIZED_HORZ");
            const xcb_atom_t netWmStateFullscreen = peer.atom("_NET_WM_STATE_FULLSCREEN");
            const xcb_atom_t netWmStateDemandsAttention = peer.atom("_NET_WM_STATE_DEMANDS_ATTENTION");
            const xcb_atom_t netActiveWindow = peer.atom("_NET_ACTIVE_WINDOW");
            const xcb_atom_t wmChangeState = peer.atom("WM_CHANGE_STATE");

            InputRecorder input;
            WindowRecorder events;
            ObjPool::Ref owner = ObjPool::fromMemory();
            Platform* const platform = Platform::create(*owner);
            Window* const window = platform->createWindow(
                *owner,
                {
                    .appId = StringView(u8"plt.café.€"),
                    .title = StringView(u8"initial X11 title"),
                    .width = 480,
                    .height = 270,
                    .minimumWidth = 7,
                    .minimumHeight = 9,
                    .input = &input,
                    .events = &events,
                    .frame = &events,
                    .appName = StringView(u8"PLT Café €"),
                }
            );
            const RenderContext context = window->renderContext();
            const xcb_window_t handle = (xcb_window_t)(uintptr_t)(context.window);
            if (context.backend != RenderBackend::X11 || context.connection == nullptr || handle == XCB_WINDOW_NONE) {
                fprintf(stderr, "%s: backend=%u connection=%p window=%u\n", scenario, (u32)(context.backend), context.connection, handle);
                return failure(scenario, "Platform::create did not select a usable X11 render context");
            }
            if (std::abs(window->info().contentScale - 2.0f) > 0.01f) {
                return failure(scenario, "Xft.dpi was not reflected in WindowInfo.contentScale");
            }

            window->requestMaximized(true);
            window->requestFullscreen(true);
            window->requestAttention();
            window->requestIconify();
            window->requestFocus();
            synchronize((xcb_connection_t*)(context.connection));
            const Property initialState = property(peer, handle, netWmState);
            const Property initialHints = property(peer, handle, XCB_ATOM_WM_HINTS);
            const Property wmClass = property(peer, handle, XCB_ATOM_WM_CLASS);
            const char expectedWmClass[] = "plt.caf\xe9.?\0PLT Caf\xe9 ?";
            if (initialState.type != XCB_ATOM_ATOM || !hasAtom(initialState, netWmStateMaximizedVert) || !hasAtom(initialState, netWmStateMaximizedHorz) || !hasAtom(initialState, netWmStateFullscreen) || !hasAtom(initialState, netWmStateDemandsAttention) || initialHints.type != XCB_ATOM_WM_HINTS || initialHints.format != 32 || initialHints.value.size() < 3 * sizeof(u32) || (initialHints.word(0) & (1u << 1)) == 0 || initialHints.word(2) != 3 || wmClass.type != XCB_ATOM_STRING || wmClass.format != 8 || wmClass.value.size() != sizeof(expectedWmClass) || memcmp(wmClass.value.data(), expectedWmClass, sizeof(expectedWmClass)) != 0) {
                fprintf(stderr, "%s: WM_CLASS type=%u format=%u size=%zu expected=%zu\n", scenario, wmClass.type, wmClass.format, wmClass.value.size(), sizeof(expectedWmClass));
                return failure(scenario, "pre-show EWMH/WM_HINTS or appId/appName WM_CLASS was not published");
            }
            const u32 observeRoot = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY;
            xcb_change_window_attributes(peer.connection, peer.screen->root, XCB_CW_EVENT_MASK, &observeRoot);
            peer.sync();
            window->requestShow();
            if (!synchronize((xcb_connection_t*)(context.connection))) {
                return failure(scenario, "the platform XCB connection failed while mapping");
            }
            pump(*platform);
            peer.sync();
            bool deferredFocusSeen = false;
            while (xcb_generic_event_t* generic = xcb_poll_for_event(peer.connection)) {
                if ((generic->response_type & 0x7f) == XCB_CLIENT_MESSAGE) {
                    const xcb_client_message_event_t& message = *(const xcb_client_message_event_t*)(generic);
                    deferredFocusSeen = deferredFocusSeen || message.type == netActiveWindow;
                }
                free(generic);
            }
            xcb_get_window_attributes_reply_t* attributes = xcb_get_window_attributes_reply(peer.connection, xcb_get_window_attributes(peer.connection, handle), nullptr);
            const bool mapped = attributes != nullptr && attributes->map_state == XCB_MAP_STATE_VIEWABLE;
            free(attributes);
            if (!mapped || events.frames == 0 || !deferredFocusSeen) {
                return failure(scenario, "the mapped window, initial frame, or deferred focus request was not observable");
            }

            window->requestRestore();
            synchronize((xcb_connection_t*)(context.connection));
            const Property restoredHints = property(peer, handle, XCB_ATOM_WM_HINTS);
            if (restoredHints.type != XCB_ATOM_NONE || !restoredHints.value.empty()) {
                return failure(scenario, "requestRestore did not clear the stale pre-map IconicState WM_HINTS");
            }

            window->requestTitle(StringView(u8"updated café €"));
            window->requestMinimumSize(13, 17);
            window->requestResizeUnit(8, 16, 3, 4);
            window->requestMove(11, 19);
            window->requestResize(640, 360);
            synchronize((xcb_connection_t*)(context.connection));
            pump(*platform);

            const Property title = property(peer, handle, netWmName);
            const std::string titleText(title.value.begin(), title.value.end());
            const Property legacyTitle = property(peer, handle, XCB_ATOM_WM_NAME);
            const char expectedTitle[] = "updated caf\xc3\xa9 \xe2\x82\xac";
            const char expectedLegacyTitle[] = "updated caf\xe9 ?";
            const Property hints = property(peer, handle, wmNormalHints);
            xcb_get_geometry_reply_t* geometry = xcb_get_geometry_reply(peer.connection, xcb_get_geometry(peer.connection, handle), nullptr);
            const bool geometryOk = geometry != nullptr && geometry->x == 11 && geometry->y == 19 && geometry->width == 640 && geometry->height == 360;
            free(geometry);
            if (title.type != utf8String || titleText != expectedTitle || legacyTitle.type != XCB_ATOM_STRING || legacyTitle.format != 8 || legacyTitle.value.size() != sizeof(expectedLegacyTitle) - 1 || memcmp(legacyTitle.value.data(), expectedLegacyTitle, sizeof(expectedLegacyTitle) - 1) != 0 || hints.type != wmSizeHints || hints.format != 32 || hints.value.size() < 17 * sizeof(u32) || hints.word(5) != 13 || hints.word(6) != 17 || hints.word(9) != 8 || hints.word(10) != 16 || hints.word(15) != 3 || hints.word(16) != 4 || !geometryOk) {
                return failure(scenario, "title, geometry, or WM_NORMAL_HINTS did not match the public requests");
            }
            std::vector<u8> hugeTitle(320 * 1024 + 31);
            for (size_t index = 0; index != hugeTitle.size(); ++index) {
                hugeTitle[index] = (u8)('a' + index % 26);
            }
            window->requestTitle(StringView(hugeTitle.data(), hugeTitle.size()));
            synchronize((xcb_connection_t*)(context.connection));
            const Property hugeNetTitle = property(peer, handle, netWmName);
            const Property hugeLegacyTitle = property(peer, handle, XCB_ATOM_WM_NAME);
            if (hugeNetTitle.type != utf8String || hugeNetTitle.format != 8 || hugeNetTitle.bytesAfter != 0 || hugeNetTitle.value != hugeTitle || hugeLegacyTitle.type != XCB_ATOM_STRING || hugeLegacyTitle.format != 8 || hugeLegacyTitle.bytesAfter != 0 || hugeLegacyTitle.value != hugeTitle) {
                return failure(scenario, "a chunked 320 KiB title was not reconstructed exactly");
            }
            const WindowInfo info = window->info();
            if (info.width != 640 || info.height != 360 || !window->inLiveResize()) {
                return failure(scenario, "ConfigureNotify did not update WindowInfo/live-resize state");
            }

            xcb_focus_in_event_t focus{};
            focus.response_type = XCB_FOCUS_IN;
            focus.detail = XCB_NOTIFY_DETAIL_ANCESTOR;
            focus.event = handle;
            focus.mode = XCB_NOTIFY_MODE_NORMAL;
            xcb_enter_notify_event_t enter{};
            enter.response_type = XCB_ENTER_NOTIFY;
            enter.detail = XCB_NOTIFY_DETAIL_ANCESTOR;
            enter.time = 1;
            enter.root = peer.screen->root;
            enter.event = handle;
            enter.event_x = 23;
            enter.event_y = 29;
            enter.same_screen_focus = 1;
            enter.mode = XCB_NOTIFY_MODE_NORMAL;
            xcb_motion_notify_event_t motion{};
            motion.response_type = XCB_MOTION_NOTIFY;
            motion.time = 2;
            motion.root = peer.screen->root;
            motion.event = handle;
            motion.event_x = 31;
            motion.event_y = 37;
            motion.same_screen = 1;
            xcb_button_press_event_t button{};
            button.response_type = XCB_BUTTON_PRESS;
            button.detail = 1;
            button.time = 3;
            button.root = peer.screen->root;
            button.event = handle;
            button.event_x = 31;
            button.event_y = 37;
            button.same_screen = 1;
            xcb_button_release_event_t buttonRelease = button;
            buttonRelease.response_type = XCB_BUTTON_RELEASE;
            buttonRelease.time = 4;
            xcb_button_press_event_t wheel = button;
            wheel.detail = 4;
            wheel.time = 5;
            xcb_key_press_event_t key{};
            key.response_type = XCB_KEY_PRESS;
            key.detail = 38;
            key.time = 6;
            key.root = peer.screen->root;
            key.event = handle;
            key.same_screen = 1;
            xcb_key_release_event_t keyRelease = key;
            keyRelease.response_type = XCB_KEY_RELEASE;
            keyRelease.time = 7;
            const u32 framesBeforeFocus = events.frames;
            if (!sendEvent(peer, handle, XCB_EVENT_MASK_FOCUS_CHANGE, focus) || !sendEvent(peer, handle, XCB_EVENT_MASK_ENTER_WINDOW, enter) || !sendEvent(peer, handle, XCB_EVENT_MASK_POINTER_MOTION, motion) || !sendEvent(peer, handle, XCB_EVENT_MASK_BUTTON_PRESS, button) || !sendEvent(peer, handle, XCB_EVENT_MASK_BUTTON_RELEASE, buttonRelease) || !sendEvent(peer, handle, XCB_EVENT_MASK_BUTTON_PRESS, wheel) || !sendEvent(peer, handle, XCB_EVENT_MASK_KEY_PRESS, key) || !sendEvent(peer, handle, XCB_EVENT_MASK_KEY_RELEASE, keyRelease)) {
                return failure(scenario, "the helper could not inject core X11 events");
            }
            pump(*platform);
            if (events.frames <= framesBeforeFocus || input.focuses != 1 || input.enters != 1 || input.motions != 1 || input.pointerX != 31 || input.pointerY != 37 || input.buttonPresses != 1 || input.buttonReleases != 1 || input.button != PointerButton::Primary || input.scrolls != 1 || input.scrollX != 0 || input.scrollY != 1 || input.keyPresses != 1 || input.keyReleases != 1 || input.pressedKey != InputKey::Printable || input.texts != 1 || input.codepoint != 'a') {
                return failure(scenario, "focus, pointer, scroll, or keyboard delivery was incomplete");
            }

            const u32 rootMask = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY;
            const xcb_void_cookie_t redirectCookie = xcb_change_window_attributes_checked(peer.connection, peer.screen->root, XCB_CW_EVENT_MASK, &rootMask);
            xcb_generic_error_t* redirectError = xcb_request_check(peer.connection, redirectCookie);
            const bool redirected = redirectError == nullptr;
            free(redirectError);
            if (!redirected) {
                return failure(scenario, "the helper could not act as the test window manager");
            }
            while (xcb_generic_event_t* event = xcb_poll_for_event(peer.connection)) {
                free(event);
            }
            window->requestMaximized(true);
            window->requestFullscreen(true);
            window->requestAttention();
            window->requestFocus();
            window->requestIconify();
            synchronize((xcb_connection_t*)(context.connection));
            peer.sync();
            bool maximizeSeen = false;
            bool fullscreenSeen = false;
            bool attentionSeen = false;
            bool focusSeen = false;
            bool iconifySeen = false;
            while (xcb_generic_event_t* generic = xcb_poll_for_event(peer.connection)) {
                if ((generic->response_type & 0x7f) == XCB_CLIENT_MESSAGE) {
                    const xcb_client_message_event_t& message = *(const xcb_client_message_event_t*)(generic);
                    if (message.type == netWmState) {
                        maximizeSeen = maximizeSeen || (message.data.data32[1] == netWmStateMaximizedVert && message.data.data32[2] == netWmStateMaximizedHorz);
                        fullscreenSeen = fullscreenSeen || message.data.data32[1] == netWmStateFullscreen;
                        attentionSeen = attentionSeen || message.data.data32[1] == netWmStateDemandsAttention;
                    } else if (message.type == netActiveWindow) {
                        focusSeen = true;
                    } else if (message.type == wmChangeState && message.data.data32[0] == 3) {
                        iconifySeen = true;
                    }
                }
                free(generic);
            }
            if (!maximizeSeen || !fullscreenSeen || !attentionSeen || !focusSeen || !iconifySeen) {
                return failure(scenario, "one or more EWMH/ICCCM client messages were missing");
            }

            xcb_client_message_event_t close{};
            close.response_type = XCB_CLIENT_MESSAGE;
            close.format = 32;
            close.window = handle;
            close.type = wmProtocols;
            close.data.data32[0] = wmDeleteWindow;
            close.data.data32[1] = 8;
            if (!sendEvent(peer, handle, XCB_EVENT_MASK_NO_EVENT, close)) {
                return failure(scenario, "WM_DELETE_WINDOW injection failed");
            }
            pump(*platform);
            if (events.closes != 1) {
                return failure(scenario, "WM_DELETE_WINDOW did not reach WindowEvents::close");
            }
            return true;
        }

        std::vector<u8> patterned(size_t size, u8 salt) {
            std::vector<u8> result(size);
            for (size_t index = 0; index != result.size(); ++index) {
                result[index] = (u8)((index * 37 + salt) & 0xff);
            }
            return result;
        }

        struct SelectionPeer final: PollCallback {
            SelectionPeer(Platform& platform_, const char* selectionName, std::vector<u8> content_, bool incremental_, bool respond_ = true, bool stringOnly_ = false)
                : platform(platform_)
                , content(static_cast<std::vector<u8>&&>(content_))
                , incremental(incremental_)
                , respond(respond_)
                , stringOnly(stringOnly_) {
                if (!x.valid()) {
                    failed = true;
                    return;
                }
                selection = x.atom(selectionName);
                utf8String = x.atom("UTF8_STRING");
                incr = x.atom("INCR");
                xdndStatus = x.atom("XdndStatus");
                xdndFinished = x.atom("XdndFinished");
                xdndActionCopy = x.atom("XdndActionCopy");
                window = xcb_generate_id(x.connection);
                const u32 mask = XCB_EVENT_MASK_PROPERTY_CHANGE;
                xcb_create_window(x.connection, XCB_COPY_FROM_PARENT, window, x.screen->root, 0, 0, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, XCB_CW_EVENT_MASK, &mask);
                xcb_set_selection_owner(x.connection, window, selection, XCB_CURRENT_TIME);
                xcb_get_selection_owner_reply_t* reply = xcb_get_selection_owner_reply(x.connection, xcb_get_selection_owner(x.connection, selection), nullptr);
                const bool owned = reply != nullptr && reply->owner == window;
                free(reply);
                if (!owned) {
                    failed = true;
                    return;
                }
                waiter.fd = {
                    .fd = xcb_get_file_descriptor(x.connection),
                    .flags = PollFlag::In,
                };
                waiter.callback = this;
                platform.poller()->arm(waiter);
                armed = true;
            }

            ~SelectionPeer() noexcept {
                if (armed) {
                    platform.poller()->cancel(waiter);
                }
                if (x.valid() && window != XCB_WINDOW_NONE) {
                    xcb_destroy_window(x.connection, window);
                    xcb_flush(x.connection);
                }
            }

            void ready(PollFD event) override {
                armed = false;
                if (event.flags & (PollFlag::Err | PollFlag::Hup)) {
                    failed = true;
                    return;
                }
                while (xcb_generic_event_t* generic = xcb_poll_for_event(x.connection)) {
                    const u8 type = generic->response_type & 0x7f;
                    if (type == XCB_SELECTION_REQUEST) {
                        selectionRequest(*(const xcb_selection_request_event_t*)(generic));
                    } else if (type == XCB_PROPERTY_NOTIFY) {
                        propertyNotify(*(const xcb_property_notify_event_t*)(generic));
                    } else if (type == XCB_CLIENT_MESSAGE) {
                        clientMessage(*(const xcb_client_message_event_t*)(generic));
                    } else if (type == 0) {
                        failed = true;
                    }
                    free(generic);
                }
                if (!failed) {
                    platform.poller()->arm(waiter);
                    armed = true;
                }
            }

            void selectionRequest(const xcb_selection_request_event_t& request) {
                ++requests;
                if (!respond) {
                    return;
                }
                xcb_selection_notify_event_t notify{};
                notify.response_type = XCB_SELECTION_NOTIFY;
                notify.time = request.time;
                notify.requestor = request.requestor;
                notify.selection = request.selection;
                notify.target = request.target;
                notify.property = XCB_ATOM_NONE;
                const xcb_atom_t acceptedTarget = stringOnly ? (xcb_atom_t)(XCB_ATOM_STRING) : utf8String;
                if (request.selection == selection && request.target == acceptedTarget && request.property != XCB_ATOM_NONE) {
                    notify.property = request.property;
                    if (incremental) {
                        requestor = request.requestor;
                        transferProperty = request.property;
                        transferTarget = request.target;
                        offset = 0;
                        transferring = true;
                        const u32 mask = XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_STRUCTURE_NOTIFY;
                        xcb_change_window_attributes(x.connection, requestor, XCB_CW_EVENT_MASK, &mask);
                        const u32 length = (u32)(content.size());
                        xcb_change_property(x.connection, XCB_PROP_MODE_REPLACE, requestor, transferProperty, incr, 32, 1, &length);
                    } else {
                        xcb_change_property(x.connection, XCB_PROP_MODE_REPLACE, request.requestor, request.property, request.target, 8, (u32)(content.size()), content.data());
                    }
                }
                xcb_send_event(x.connection, 0, request.requestor, XCB_EVENT_MASK_NO_EVENT, (const char*)(&notify));
                xcb_flush(x.connection);
            }

            void propertyNotify(const xcb_property_notify_event_t& event) {
                if (!transferring || event.window != requestor || event.atom != transferProperty || event.state != XCB_PROPERTY_DELETE) {
                    return;
                }
                if (offset != content.size()) {
                    const size_t count = std::min((size_t)(32 * 1024), content.size() - offset);
                    xcb_change_property(x.connection, XCB_PROP_MODE_REPLACE, requestor, transferProperty, transferTarget, 8, (u32)(count), content.data() + offset);
                    offset += count;
                } else {
                    xcb_change_property(x.connection, XCB_PROP_MODE_REPLACE, requestor, transferProperty, transferTarget, 8, 0, nullptr);
                    transferring = false;
                }
                xcb_flush(x.connection);
            }

            void clientMessage(const xcb_client_message_event_t& event) {
                if (event.type == xdndStatus) {
                    ++statusCount;
                    memcpy(statusData, event.data.data32, sizeof(statusData));
                } else if (event.type == xdndFinished) {
                    ++finishedCount;
                    memcpy(finishedData, event.data.data32, sizeof(finishedData));
                }
            }

            bool sendClientMessage(xcb_window_t destination, xcb_atom_t type, const u32 (&data)[5]) {
                xcb_client_message_event_t event{};
                event.response_type = XCB_CLIENT_MESSAGE;
                event.format = 32;
                event.window = destination;
                event.type = type;
                memcpy(event.data.data32, data, sizeof(event.data.data32));
                return sendEvent(x, destination, XCB_EVENT_MASK_NO_EVENT, event);
            }

            Platform& platform;
            XConnection x;
            PollWaiter waiter;
            std::vector<u8> content;
            xcb_window_t window = XCB_WINDOW_NONE;
            xcb_window_t requestor = XCB_WINDOW_NONE;
            xcb_atom_t selection = XCB_ATOM_NONE;
            xcb_atom_t utf8String = XCB_ATOM_NONE;
            xcb_atom_t incr = XCB_ATOM_NONE;
            xcb_atom_t transferProperty = XCB_ATOM_NONE;
            xcb_atom_t transferTarget = XCB_ATOM_NONE;
            xcb_atom_t xdndStatus = XCB_ATOM_NONE;
            xcb_atom_t xdndFinished = XCB_ATOM_NONE;
            xcb_atom_t xdndActionCopy = XCB_ATOM_NONE;
            size_t offset = 0;
            u32 requests = 0;
            u32 statusCount = 0;
            u32 finishedCount = 0;
            u32 statusData[5]{};
            u32 finishedData[5]{};
            bool incremental = false;
            bool respond = true;
            bool stringOnly = false;
            bool transferring = false;
            bool armed = false;
            bool failed = false;
        };

        bool readClipboard(Clipboard& clipboard, const std::vector<u8>& expected) {
            const ScopedPtr<Input> stream{clipboard.read()};
            std::vector<u8> received;
            for (;;) {
                u8 chunk[8192];
                const size_t count = stream->read(chunk, sizeof(chunk));
                if (count == 0) {
                    break;
                }
                received.insert(received.end(), chunk, chunk + count);
            }
            return received == expected;
        }

        bool incomingSelections() {
            constexpr const char* scenario = "X11 incoming selections";
            ObjPool::Ref owner = ObjPool::fromMemory();
            Platform* const platform = Platform::create(*owner);
            Window* const window = platform->createWindow(*owner, {});
            const std::vector<u8> direct{'r', 'e', 'm', 'o', 't', 'e', ' ', 'c', 'l', 'i', 'p', 'b', 'o', 'a', 'r', 'd'};
            const std::vector<u8> incremental = patterned(192 * 1024 + 73, 19);
            {
                SelectionPeer clipboard(*platform, "CLIPBOARD", direct, false);
                SelectionPeer primary(*platform, "PRIMARY", incremental, true);
                if (clipboard.failed || primary.failed) {
                    return failure(scenario, "could not publish the helper selections");
                }
                const bool result = runOnFiber(*platform, [&] {
                    return readClipboard(*window->secondary(), direct) && readClipboard(*window->primary(), incremental);
                });
                if (!result || clipboard.failed || primary.failed || clipboard.requests != 1 || primary.requests != 1) {
                    return failure(scenario, "direct or INCR input did not complete with the exact payload");
                }
            }

            const std::vector<u8> latin1{'c', 'a', 'f', 0xe9, ' ', 0xa3};
            const std::vector<u8> utf8{'c', 'a', 'f', 0xc3, 0xa9, ' ', 0xc2, 0xa3};
            SelectionPeer legacy(*platform, "CLIPBOARD", latin1, false, true, true);
            if (legacy.failed ||
                !runOnFiber(
                    *platform,
                    [&] {
                return readClipboard(*window->secondary(), utf8);
            }
                ) ||
                legacy.failed || legacy.requests != 4) {
                return failure(scenario, "legacy STRING input was not converted from Latin-1 to UTF-8");
            }
            return true;
        }

        bool parkedClipboardTeardown() {
            constexpr const char* scenario = "X11 parked clipboard teardown";
            XConnection silentOwner;
            if (!silentOwner.valid()) {
                return failure(scenario, "could not open the silent selection owner");
            }
            const xcb_atom_t clipboard = silentOwner.atom("CLIPBOARD");
            const xcb_window_t selectionWindow = xcb_generate_id(silentOwner.connection);
            xcb_create_window(silentOwner.connection, XCB_COPY_FROM_PARENT, selectionWindow, silentOwner.screen->root, 0, 0, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, 0, nullptr);
            xcb_set_selection_owner(silentOwner.connection, selectionWindow, clipboard, XCB_CURRENT_TIME);
            xcb_get_selection_owner_reply_t* reply = xcb_get_selection_owner_reply(silentOwner.connection, xcb_get_selection_owner(silentOwner.connection, clipboard), nullptr);
            const bool owned = reply != nullptr && reply->owner == selectionWindow;
            free(reply);
            if (!owned) {
                return failure(scenario, "could not publish the silent clipboard");
            }

            bool entered = false;
            bool resumed = false;
            PoolOwner owner;
            Platform* const platform = Platform::create(*owner.pool);
            Window* const window = platform->createWindow(*owner.pool, {});
            auto task = makeRunable([&] {
                entered = true;
                const ScopedPtr<Input> stream{window->secondary()->read()};
                u8 byte;
                stream->read(&byte, 1);
                resumed = true;
            });
            platform->scheduler()->create(*owner.pool, task, 128 * 1024);
            if (!entered || resumed) {
                return failure(scenario, "the external clipboard read did not park");
            }
            owner.reset();
            if (resumed) {
                return failure(scenario, "pool teardown resumed a released clipboard fiber");
            }
            return true;
        }

        bool publish(Clipboard& clipboard, const std::vector<u8>& content) {
            const ScopedPtr<Output> stream{clipboard.write()};
            stream->write(content.data(), content.size());
            stream->finish();
            return true;
        }

        xcb_generic_event_t* nextEvent(XConnection& peer, Scheduler& scheduler) {
            for (;;) {
                if (xcb_generic_event_t* event = xcb_poll_for_event(peer.connection)) {
                    return event;
                }
                if (xcb_connection_has_error(peer.connection) != 0 || !scheduler.awaitReadable(xcb_get_file_descriptor(peer.connection), peerTimeout)) {
                    return nullptr;
                }
            }
        }

        bool convertSelection(XConnection& peer, Scheduler& scheduler, xcb_window_t requestor, xcb_atom_t selection, xcb_atom_t target, xcb_atom_t destination) {
            xcb_delete_property(peer.connection, requestor, destination);
            xcb_convert_selection(peer.connection, requestor, selection, target, destination, XCB_CURRENT_TIME);
            xcb_flush(peer.connection);
            for (;;) {
                xcb_generic_event_t* const generic = nextEvent(peer, scheduler);
                if (generic == nullptr) {
                    return false;
                }
                const u8 type = generic->response_type & 0x7f;
                if (type == XCB_SELECTION_NOTIFY) {
                    const xcb_selection_notify_event_t notify = *(const xcb_selection_notify_event_t*)(generic);
                    free(generic);
                    return notify.selection == selection && notify.target == target && notify.property == destination;
                }
                if (type == 0) {
                    free(generic);
                    return false;
                }
                free(generic);
            }
        }

        bool readIncrementalSelection(XConnection& peer, Scheduler& scheduler, xcb_window_t requestor, xcb_atom_t selection, xcb_atom_t target, xcb_atom_t destination, xcb_atom_t incr, std::vector<u8>& received) {
            if (!convertSelection(peer, scheduler, requestor, selection, target, destination)) {
                return false;
            }
            const Property announcement = property(peer, requestor, destination);
            if (announcement.type != incr || announcement.format != 32 || announcement.value.size() != sizeof(u32)) {
                return false;
            }
            xcb_delete_property(peer.connection, requestor, destination);
            xcb_flush(peer.connection);
            for (;;) {
                xcb_generic_event_t* const generic = nextEvent(peer, scheduler);
                if (generic == nullptr) {
                    return false;
                }
                const u8 type = generic->response_type & 0x7f;
                bool ready = false;
                if (type == XCB_PROPERTY_NOTIFY) {
                    const xcb_property_notify_event_t& event = *(const xcb_property_notify_event_t*)(generic);
                    ready = event.window == requestor && event.atom == destination && event.state == XCB_PROPERTY_NEW_VALUE;
                }
                free(generic);
                if (!ready) {
                    continue;
                }
                const Property chunk = property(peer, requestor, destination, true);
                if (chunk.type != target || chunk.format != 8 || chunk.bytesAfter != 0) {
                    return false;
                }
                if (chunk.value.empty()) {
                    return true;
                }
                received.insert(received.end(), chunk.value.begin(), chunk.value.end());
            }
        }

        bool outgoingSelection() {
            constexpr const char* scenario = "X11 outgoing selection";
            ObjPool::Ref owner = ObjPool::fromMemory();
            Platform* const platform = Platform::create(*owner);
            Window* const window = platform->createWindow(*owner, {});
            const std::vector<u8> direct{'c', 'a', 'f', 0xc3, 0xa9, ' ', 0xc2, 0xa3, ' ', 0xe2, 0x82, 0xac};
            const std::vector<u8> directLatin1{'c', 'a', 'f', 0xe9, ' ', 0xa3, ' ', '?'};
            const std::vector<u8> content = patterned(192 * 1024 + 113, 11);
            const std::vector<u8> primary{'o', 'w', 'n', 'e', 'd', ' ', 'p', 'r', 'i', 'm', 'a', 'r', 'y'};
            publish(*window->secondary(), direct);
            publish(*window->primary(), primary);

            XConnection peer;
            if (!peer.valid()) {
                return failure(scenario, "could not open the requestor connection");
            }
            const xcb_atom_t clipboard = peer.atom("CLIPBOARD");
            const xcb_atom_t targets = peer.atom("TARGETS");
            const xcb_atom_t timestamp = peer.atom("TIMESTAMP");
            const xcb_atom_t multiple = peer.atom("MULTIPLE");
            const xcb_atom_t atomPair = peer.atom("ATOM_PAIR");
            const xcb_atom_t utf8String = peer.atom("UTF8_STRING");
            const xcb_atom_t incr = peer.atom("INCR");
            const xcb_atom_t unsupported = peer.atom("_PLT_X11_TEST_UNSUPPORTED_TARGET");
            const xcb_atom_t directProperty = peer.atom("_PLT_X11_TEST_DIRECT");
            const xcb_atom_t stringProperty = peer.atom("_PLT_X11_TEST_STRING");
            const xcb_atom_t multipleProperty = peer.atom("_PLT_X11_TEST_MULTIPLE");
            const xcb_atom_t multipleUtf8Property = peer.atom("_PLT_X11_TEST_MULTIPLE_UTF8");
            const xcb_atom_t multipleInvalidProperty = peer.atom("_PLT_X11_TEST_MULTIPLE_INVALID");
            const xcb_atom_t multipleStringProperty = peer.atom("_PLT_X11_TEST_MULTIPLE_STRING");
            const xcb_atom_t targetsProperty = peer.atom("_PLT_X11_TEST_TARGETS");
            const xcb_atom_t timestampProperty = peer.atom("_PLT_X11_TEST_TIMESTAMP");
            const xcb_atom_t contentProperty = peer.atom("_PLT_X11_TEST_CONTENT");
            const xcb_window_t requestor = xcb_generate_id(peer.connection);
            const u32 requestorMask = XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_STRUCTURE_NOTIFY;
            xcb_create_window(peer.connection, XCB_COPY_FROM_PARENT, requestor, peer.screen->root, 0, 0, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, XCB_CW_EVENT_MASK, &requestorMask);
            xcb_get_selection_owner_reply_t* selectionOwner = xcb_get_selection_owner_reply(peer.connection, xcb_get_selection_owner(peer.connection, clipboard), nullptr);
            const bool owned = selectionOwner != nullptr && selectionOwner->owner != XCB_WINDOW_NONE;
            free(selectionOwner);
            if (!owned) {
                return failure(scenario, "the clipboard owner was not published");
            }

            Property directReceived;
            Property stringReceived;
            Property multipleResult;
            Property multipleUtf8;
            Property multipleInvalid;
            Property multipleString;
            Property advertised;
            Property ownershipTime;
            std::vector<u8> received;
            const bool result = runOnFiber(*platform, [&] {
                Scheduler& scheduler = *platform->scheduler();
                if (!convertSelection(peer, scheduler, requestor, clipboard, utf8String, directProperty)) {
                    return false;
                }
                directReceived = property(peer, requestor, directProperty, true);
                if (!convertSelection(peer, scheduler, requestor, clipboard, XCB_ATOM_STRING, stringProperty)) {
                    return false;
                }
                stringReceived = property(peer, requestor, stringProperty, true);

                const xcb_atom_t pairs[] = {
                    utf8String,
                    multipleUtf8Property,
                    unsupported,
                    multipleInvalidProperty,
                    XCB_ATOM_STRING,
                    multipleStringProperty,
                    utf8String,
                    multipleProperty,
                };
                xcb_change_property(peer.connection, XCB_PROP_MODE_REPLACE, requestor, multipleProperty, atomPair, 32, sizeof(pairs) / sizeof(pairs[0]), pairs);
                xcb_convert_selection(peer.connection, requestor, clipboard, multiple, multipleProperty, XCB_CURRENT_TIME);
                xcb_flush(peer.connection);
                bool multipleNotified = false;
                while (!multipleNotified) {
                    xcb_generic_event_t* const generic = nextEvent(peer, scheduler);
                    if (generic == nullptr) {
                        return false;
                    }
                    const u8 type = generic->response_type & 0x7f;
                    if (type == XCB_SELECTION_NOTIFY) {
                        const xcb_selection_notify_event_t& notify = *(const xcb_selection_notify_event_t*)(generic);
                        multipleNotified = notify.selection == clipboard && notify.target == multiple && notify.property == multipleProperty;
                    } else if (type == 0) {
                        free(generic);
                        return false;
                    }
                    free(generic);
                }
                multipleResult = property(peer, requestor, multipleProperty, true);
                multipleUtf8 = property(peer, requestor, multipleUtf8Property, true);
                multipleInvalid = property(peer, requestor, multipleInvalidProperty, true);
                multipleString = property(peer, requestor, multipleStringProperty, true);

                publish(*window->secondary(), content);
                if (!convertSelection(peer, scheduler, requestor, clipboard, targets, targetsProperty)) {
                    return false;
                }
                advertised = property(peer, requestor, targetsProperty, true);
                if (!convertSelection(peer, scheduler, requestor, clipboard, timestamp, timestampProperty)) {
                    return false;
                }
                ownershipTime = property(peer, requestor, timestampProperty, true);
                return readIncrementalSelection(peer, scheduler, requestor, clipboard, utf8String, contentProperty, incr, received);
            });
            xcb_destroy_window(peer.connection, requestor);
            xcb_flush(peer.connection);
            if (!result || directReceived.type != utf8String || directReceived.format != 8 || directReceived.value != direct || stringReceived.type != XCB_ATOM_STRING || stringReceived.format != 8 || stringReceived.value != directLatin1 || multipleResult.type != atomPair || multipleResult.format != 32 || multipleResult.value.size() != 8 * sizeof(xcb_atom_t) || multipleResult.word(0) != utf8String || multipleResult.word(1) != multipleUtf8Property || multipleResult.word(2) != unsupported || multipleResult.word(3) != XCB_ATOM_NONE || multipleResult.word(4) != XCB_ATOM_STRING || multipleResult.word(5) != multipleStringProperty || multipleResult.word(6) != utf8String || multipleResult.word(7) != XCB_ATOM_NONE || multipleUtf8.type != utf8String || multipleUtf8.format != 8 || multipleUtf8.value != direct || multipleInvalid.type != XCB_ATOM_NONE || !multipleInvalid.value.empty() || multipleString.type != XCB_ATOM_STRING || multipleString.format != 8 || multipleString.value != directLatin1 || advertised.type != XCB_ATOM_ATOM || !hasAtom(advertised, targets) || !hasAtom(advertised, timestamp) || !hasAtom(advertised, multiple) || !hasAtom(advertised, utf8String) || !hasAtom(advertised, XCB_ATOM_STRING) || ownershipTime.type != XCB_ATOM_INTEGER || ownershipTime.format != 32 || ownershipTime.value.size() != sizeof(u32) || ownershipTime.word(0) == XCB_CURRENT_TIME || received != content) {
                return failure(scenario, "STRING/MULTIPLE/TARGETS/TIMESTAMP or the outbound INCR stream was invalid");
            }
            return true;
        }

        struct DropRecorder final: DropTarget {
            DropReply dragOver(const DropOffer& offer, i32, i32) override {
                ++overs;
                formats = offer.formats();
                bool offered = false;
                for (size_t index = 0; index != offer.formats(); ++index) {
                    offered = offered || offer.format(index) == StringView(u8"UTF8_STRING");
                }
                return offered ? DropReply{.mime = StringView(u8"UTF8_STRING"), .action = DropAction::Copy} : DropReply{};
            }

            void dragLeft() override {
                ++left;
            }

            void dropped(Drop& drop) override {
                ++drops;
                const ScopedPtr<Input> stream{drop.read(StringView(u8"UTF8_STRING"))};
                for (;;) {
                    u8 chunk[4096];
                    const size_t count = stream->read(chunk, sizeof(chunk));
                    if (count == 0) {
                        break;
                    }
                    content.insert(content.end(), chunk, chunk + count);
                }
            }

            std::vector<u8> content;
            size_t formats = 0;
            u32 overs = 0;
            u32 left = 0;
            u32 drops = 0;
        };

        bool xdndTransfer() {
            constexpr const char* scenario = "X11 XDND";
            DropRecorder drop;
            ObjPool::Ref owner = ObjPool::fromMemory();
            Platform* const platform = Platform::create(*owner);
            Window* const window = platform->createWindow(*owner, {.drop = &drop});
            window->requestShow();
            synchronize((xcb_connection_t*)(window->renderContext().connection));
            pump(*platform);

            const std::vector<u8> content{'X', '1', '1', ' ', 'd', 'r', 'o', 'p'};
            SelectionPeer source(*platform, "XdndSelection", content, false);
            if (source.failed) {
                return failure(scenario, "could not create the XDND source");
            }
            const xcb_window_t target = (xcb_window_t)(uintptr_t)(window->renderContext().window);
            const xcb_atom_t xdndEnter = source.x.atom("XdndEnter");
            const xcb_atom_t xdndPosition = source.x.atom("XdndPosition");
            const xcb_atom_t xdndDrop = source.x.atom("XdndDrop");
            const u32 enter[5] = {
                source.window,
                5u << 24,
                source.utf8String,
                XCB_ATOM_NONE,
                XCB_ATOM_NONE,
            };
            const u32 position[5] = {
                source.window,
                0,
                (40u << 16) | 50u,
                XCB_CURRENT_TIME,
                source.xdndActionCopy,
            };
            if (!source.sendClientMessage(target, xdndEnter, enter) || !source.sendClientMessage(target, xdndPosition, position)) {
                return failure(scenario, "could not send XdndEnter/XdndPosition");
            }
            pump(*platform, 100'000);
            if (drop.overs != 1 || drop.formats != 1 || source.statusCount != 1 || (source.statusData[1] & 1) == 0 || source.statusData[4] != source.xdndActionCopy) {
                return failure(scenario, "the XDND offer was not accepted with XdndStatus");
            }

            const u32 dropped[5] = {
                source.window,
                0,
                XCB_CURRENT_TIME,
                0,
                0,
            };
            if (!source.sendClientMessage(target, xdndDrop, dropped)) {
                return failure(scenario, "could not send XdndDrop");
            }
            pump(*platform, 150'000);
            if (source.failed || source.requests != 1 || drop.drops != 1 || drop.content != content || source.finishedCount != 1 || source.finishedData[1] != 1 || source.finishedData[2] != source.xdndActionCopy) {
                return failure(scenario, "the drop payload or XdndFinished result was invalid");
            }
            return true;
        }

        bool parkedXdndWindowTeardown() {
            constexpr const char* scenario = "X11 parked XDND teardown";
            DropRecorder drop;
            ObjPool::Ref platformOwner = ObjPool::fromMemory();
            Platform* const platform = Platform::create(*platformOwner);
            PoolOwner windowOwner;
            Window* const window = platform->createWindow(*windowOwner.pool, {.drop = &drop});
            window->requestShow();
            synchronize((xcb_connection_t*)(window->renderContext().connection));
            pump(*platform);

            const std::vector<u8> content{'b', 'l', 'o', 'c', 'k', 'e', 'd'};
            SelectionPeer source(*platform, "XdndSelection", content, false, false);
            if (source.failed) {
                return failure(scenario, "could not create the blocking XDND source");
            }
            const xcb_window_t target = (xcb_window_t)(uintptr_t)(window->renderContext().window);
            const xcb_atom_t xdndEnter = source.x.atom("XdndEnter");
            const xcb_atom_t xdndPosition = source.x.atom("XdndPosition");
            const xcb_atom_t xdndDrop = source.x.atom("XdndDrop");
            const u32 enter[5] = {
                source.window,
                5u << 24,
                source.utf8String,
                XCB_ATOM_NONE,
                XCB_ATOM_NONE,
            };
            const u32 position[5] = {
                source.window,
                0,
                (60u << 16) | 70u,
                XCB_CURRENT_TIME,
                source.xdndActionCopy,
            };
            if (!source.sendClientMessage(target, xdndEnter, enter) || !source.sendClientMessage(target, xdndPosition, position)) {
                return failure(scenario, "could not start the blocking XDND session");
            }
            pump(*platform, 100'000);
            if (source.statusCount != 1 || (source.statusData[1] & 1) == 0) {
                return failure(scenario, "the blocking XDND offer was not accepted");
            }
            const u32 dropped[5] = {
                source.window,
                0,
                XCB_CURRENT_TIME,
                0,
                0,
            };
            if (!source.sendClientMessage(target, xdndDrop, dropped)) {
                return failure(scenario, "could not send the blocking XdndDrop");
            }
            pump(*platform, 100'000);
            if (source.requests != 1 || drop.drops != 1 || source.finishedCount != 0) {
                return failure(scenario, "the XDND payload read did not remain parked");
            }

            const std::vector<u8> secondContent{'a', 'l', 's', 'o', ' ', 'b', 'l', 'o', 'c', 'k', 'e', 'd'};
            SelectionPeer secondSource(*platform, "XdndSelection", secondContent, false, false);
            if (secondSource.failed) {
                return failure(scenario, "could not create the overlapping XDND source");
            }
            const xcb_atom_t secondEnter = secondSource.x.atom("XdndEnter");
            const xcb_atom_t secondPosition = secondSource.x.atom("XdndPosition");
            const xcb_atom_t secondDrop = secondSource.x.atom("XdndDrop");
            const u32 secondEnterData[5] = {
                secondSource.window,
                5u << 24,
                secondSource.utf8String,
                XCB_ATOM_NONE,
                XCB_ATOM_NONE,
            };
            const u32 secondPositionData[5] = {
                secondSource.window,
                0,
                (75u << 16) | 85u,
                XCB_CURRENT_TIME,
                secondSource.xdndActionCopy,
            };
            if (!secondSource.sendClientMessage(target, secondEnter, secondEnterData) || !secondSource.sendClientMessage(target, secondPosition, secondPositionData)) {
                return failure(scenario, "could not start the overlapping XDND session");
            }
            pump(*platform, 100'000);
            if (secondSource.statusCount != 1 || (secondSource.statusData[1] & 1) == 0) {
                return failure(scenario, "the overlapping XDND offer was not accepted");
            }
            const u32 secondDropped[5] = {
                secondSource.window,
                0,
                XCB_CURRENT_TIME,
                0,
                0,
            };
            if (!secondSource.sendClientMessage(target, secondDrop, secondDropped)) {
                return failure(scenario, "could not send the overlapping XdndDrop");
            }
            pump(*platform, 100'000);
            if (secondSource.requests != 1 || drop.drops != 2 || secondSource.finishedCount != 0) {
                return failure(scenario, "the overlapping XDND payload read did not remain parked");
            }

            windowOwner.reset();
            pump(*platform, 100'000);
            if (source.failed || secondSource.failed || source.finishedCount != 1 || secondSource.finishedCount != 1 || source.finishedData[0] != target || secondSource.finishedData[0] != target || source.finishedData[1] != 0 || secondSource.finishedData[1] != 0 || !drop.content.empty()) {
                return failure(scenario, "window teardown did not cancel both parked XDND transfers cleanly");
            }
            return true;
        }

        bool parkedXdndHoverPoolTeardown() {
            constexpr const char* scenario = "X11 parked XDND hover pool teardown";
            DropRecorder drop;
            PoolOwner owner;
            Platform* const platform = Platform::create(*owner.pool);
            Window* const window = platform->createWindow(*owner.pool, {.drop = &drop});
            window->requestShow();
            synchronize((xcb_connection_t*)(window->renderContext().connection));
            pump(*platform);

            XConnection source;
            if (!source.valid()) {
                return failure(scenario, "could not create the hover source connection");
            }
            const xcb_window_t sourceWindow = xcb_generate_id(source.connection);
            xcb_create_window(source.connection, XCB_COPY_FROM_PARENT, sourceWindow, source.screen->root, 0, 0, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, 0, nullptr);
            const xcb_window_t target = (xcb_window_t)(uintptr_t)(window->renderContext().window);
            const xcb_atom_t utf8String = source.atom("UTF8_STRING");
            const xcb_atom_t xdndEnter = source.atom("XdndEnter");
            const xcb_atom_t xdndPosition = source.atom("XdndPosition");
            const xcb_atom_t xdndStatus = source.atom("XdndStatus");
            const xcb_atom_t xdndActionCopy = source.atom("XdndActionCopy");
            const u32 enter[5] = {
                sourceWindow,
                5u << 24,
                utf8String,
                XCB_ATOM_NONE,
                XCB_ATOM_NONE,
            };
            const u32 position[5] = {
                sourceWindow,
                0,
                (80u << 16) | 90u,
                XCB_CURRENT_TIME,
                xdndActionCopy,
            };
            xcb_client_message_event_t enterEvent{};
            enterEvent.response_type = XCB_CLIENT_MESSAGE;
            enterEvent.format = 32;
            enterEvent.window = target;
            enterEvent.type = xdndEnter;
            memcpy(enterEvent.data.data32, enter, sizeof(enter));
            xcb_client_message_event_t positionEvent{};
            positionEvent.response_type = XCB_CLIENT_MESSAGE;
            positionEvent.format = 32;
            positionEvent.window = target;
            positionEvent.type = xdndPosition;
            memcpy(positionEvent.data.data32, position, sizeof(position));
            if (!sendEvent(source, target, XCB_EVENT_MASK_NO_EVENT, enterEvent) || !sendEvent(source, target, XCB_EVENT_MASK_NO_EVENT, positionEvent)) {
                return failure(scenario, "could not start the parked hover session");
            }
            pump(*platform, 100'000);
            bool accepted = false;
            while (xcb_generic_event_t* generic = xcb_poll_for_event(source.connection)) {
                if ((generic->response_type & 0x7f) == XCB_CLIENT_MESSAGE) {
                    const xcb_client_message_event_t& message = *(const xcb_client_message_event_t*)(generic);
                    accepted = accepted || (message.type == xdndStatus && (message.data.data32[1] & 1) != 0 && message.data.data32[4] == xdndActionCopy);
                }
                free(generic);
            }
            if (drop.overs != 1 || !accepted) {
                return failure(scenario, "the hover session was not parked after an accepted position");
            }

            owner.reset();
            if (!source.sync()) {
                return failure(scenario, "platform pool teardown corrupted the X connection");
            }
            xcb_destroy_window(source.connection, sourceWindow);
            xcb_flush(source.connection);
            return true;
        }

        using Scenario = bool (*)();

        bool runScenario(const char* name, Scenario scenario) {
            const bool success = scenario();
            fprintf(stderr, "%s: %s\n", name, success ? "PASS" : "FAIL");
            return success;
        }
    }
}

int main() {
    unsetenv("WAYLAND_DISPLAY");
    unsetenv("WAYLAND_SOCKET");
    if (getenv("DISPLAY") == nullptr || getenv("DISPLAY")[0] == 0) {
        fprintf(stderr, "X11 integration tests require DISPLAY\n");
        return 1;
    }

    using namespace plt::test;
    bool success = true;
    success = runScenario("window, input, EWMH, render handles", windowInputAndProtocols) && success;
    success = runScenario("incoming direct and INCR selections", incomingSelections) && success;
    success = runScenario("parked clipboard pool teardown", parkedClipboardTeardown) && success;
    success = runScenario("outgoing direct, metadata, and INCR selection", outgoingSelection) && success;
    success = runScenario("XDND transfer", xdndTransfer) && success;
    success = runScenario("parked XDND window teardown", parkedXdndWindowTeardown) && success;
    success = runScenario("parked XDND hover pool teardown", parkedXdndHoverPoolTeardown) && success;
    return success ? 0 : 1;
}
