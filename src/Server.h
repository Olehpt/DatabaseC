#pragma once

#include "../external/crow_all.h"
#include "boost/asio.hpp"

#include <map>
#include <string>

#include "Database.h"

class Server
{
private:
    std::map<std::string, Database> databases;

    void loadDatabases();

    void setupRoutes(crow::SimpleApp& app);
    void setupDatabaseRoutes(crow::SimpleApp& app);
    void setupTableRoutes(crow::SimpleApp& app);

    static std::string dataTypeToString(DataType type);
    static crow::json::wvalue valueToJson(const Value& value);

public:
    void run();
};