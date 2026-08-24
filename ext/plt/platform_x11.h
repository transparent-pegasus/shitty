#pragma once

namespace stl {
    class ObjPool;
}

namespace plt {
    struct Platform;

    Platform* createX11Platform(stl::ObjPool& owner);
}
