#include <xpresspp/xpresspp.h>
#include "src/exports.h"
#include <iostream>

using namespace std;

int main() {
    cout << "[Xpress++] Starting Modular Class-Based Backend..." << endl;

    xp::App app;

    // Connect Database & Run Migrations
    Database::connect(app);

    // Register all Middleware & Routes
    ApiRoutes::setup(app);

    // Start listening on Port 8082
    const int PORT = 8082;

    cout << "[Xpress++] Server is listening on http://localhost:" << PORT << endl;
    app.listen(PORT);

    return 0;
}
