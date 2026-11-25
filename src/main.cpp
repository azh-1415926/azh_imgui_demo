// ============ src/main.cpp ============
#include "App.h"
#include <cstdio>

int main() {
    App app;
    if (!app.Init()) {
        std::fprintf(stderr, "App init failed\n");
        return 1;
    }
    app.Run();
    app.Shutdown();
    return 0;
}