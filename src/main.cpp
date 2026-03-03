#include "Application.h"
#include "Logger.h"

int main(int argc, char** argv) {
    logger::init();

    core::Application app;

    if (!app.setup()) {
        return -1;
    }
    app.loop();

    if (!app.cleanup()) {
        return -1;
    }

    return 0;
}
