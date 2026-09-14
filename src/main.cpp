#include "common.h"
#include "sprite_application.h"

#include <moth/graphics/platform/glfw/glfw_platform.h>

int main() {
    moth::gfx::platform::glfw::Platform platform;
    if (!platform.Startup()) {
        return 1;
    }
    SpriteApplication app(platform);
    app.Init();
    app.Run();
    platform.Shutdown();
    return 0;
}
