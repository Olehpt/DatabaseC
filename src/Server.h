#pragma once
#include "../external/crow_all.h"
#include "Database.h"
#include <map>
#include <mutex>
#include <functional>
class Server {
    std::map<std::string, Database> databases;
    std::mutex mutex;
    void loadDatabases();
    void setupRoutes(crow::SimpleApp& app);
    crow::response withTable(const crow::request& req, const std::string& database,
        const std::string& table, const std::function<crow::response(Table&, Database&)>& action);
public:
    void run();
};
