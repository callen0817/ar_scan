#include "application.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        av::app::Application app(argc, argv);
        if (!app.initialize()) {
            std::cerr << "[FATAL] Failed to initialize AV Scan Application." << std::endl;
            return 1;
        }
        return app.run();
    } catch (const std::exception& e) {
        std::cerr << "[FATAL] Unhandled exception: " << e.what() << std::endl;
        return 1;
    }
}
