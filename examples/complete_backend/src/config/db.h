#pragma once
#include <xpresspp/xpresspp.h>
#include "../models/db.h"
#include <iostream>

class Database {
    public:
    // Configures the connection pool using a database URL and schedules table initialization
    static void connect(xp::App& app) {
        // Can connect to any local or cloud instance using a clean connection string URL!
        std::string db_url = "postgresql://postgres:password@127.0.0.1:5450/xpresspp";
        
        app.database(db_url, 3); // URL + pool size
        std::cout << "[DB Config] Database pool initialized from connection URL." << std::endl;

        app.onStart([]() async {
            try {
                // Auto-syncing: checks the Model schema and auto-creates all tables if not exists
                await SchemaSync::syncAll();
                std::cout << "[DB Migration] ORM synced all database tables successfully." << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[DB Migration Error] Auto-sync failed: " << e.what() << std::endl;
            }
        });
    }
};
