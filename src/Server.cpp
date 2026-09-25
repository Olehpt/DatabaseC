#include "Server.h"

#include <filesystem>
#include <iostream>
#include <type_traits>
#include <variant>

void Server::loadDatabases()
{
    namespace fs = std::filesystem;

    for (const auto& entry : fs::directory_iterator(fs::current_path()))
    {
        if (!entry.is_regular_file())
            continue;

        if (entry.path().extension() != ".bin")
            continue;

        const std::string filename = entry.path().string();
        const std::string databaseName = entry.path().stem().string();

        Database db;

        if (db.load(filename))
        {
            databases.emplace(databaseName, std::move(db));

            std::cout << "Loaded database: "
                << databaseName
                << '\n';
        }
        else
        {
            std::cerr << "Failed to load database: "
                << filename
                << '\n';
        }
    }

    std::cout << "Loaded "
        << databases.size()
        << " database(s)\n";
}


std::string Server::dataTypeToString(DataType type)
{
    switch (type)
    {
    case DataType::Integer:
        return "integer";

    case DataType::Real:
        return "real";

    case DataType::Char:
        return "char";

    case DataType::String:
        return "string";
    }

    return "unknown";
}


crow::json::wvalue Server::valueToJson(const Value& value)
{
    return std::visit(
        [](const auto& arg) -> crow::json::wvalue
    {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, std::string>)
        {
            return arg;
        }
        else if constexpr (std::is_same_v<T, char>)
        {
            return std::string(1, arg);
        }
        else
        {
            return arg;
        }
    },
        value
    );
}


void Server::setupDatabaseRoutes(crow::SimpleApp& app)
{
    CROW_ROUTE(app, "/databases")
        ([this]()
    {
        crow::json::wvalue result;

        int index = 0;

        for (const auto& [name, database] : databases)
        {
            result["databases"][index++] = name;
        }

        return crow::response(result);
    });

    CROW_ROUTE(app, "/databases/<string>")
        .methods(crow::HTTPMethod::Post)
        ([this](const std::string& databaseName)
    {
        if (databases.find(databaseName) != databases.end())
        {
            return crow::response(409, "Database already exists");
        }

        Database database;

        const std::string filename = databaseName + ".bin";

        if (!database.save(filename))
        {
            return crow::response(500, "Failed to create database file");
        }

        databases.emplace(databaseName, std::move(database));

        crow::json::wvalue result;
        result["message"] = "Database created";
        result["name"] = databaseName;

        return crow::response(201, result);
    });

    CROW_ROUTE(app, "/databases/<string>")
        .methods(crow::HTTPMethod::Delete)
        ([this](const std::string& databaseName)
    {
        auto databaseIt = databases.find(databaseName);

        if (databaseIt == databases.end())
        {
            return crow::response(404, "Database not found");
        }

        const std::string filename = databaseName + ".bin";

        std::error_code error;

        if (!std::filesystem::remove(filename, error))
        {
            if (error)
            {
                return crow::response(
                    500,
                    "Failed to delete database file"
                );
            }

            return crow::response(
                500,
                "Database file not found"
            );
        }

        databases.erase(databaseIt);

        crow::json::wvalue result;
        result["message"] = "Database deleted";
        result["name"] = databaseName;

        return crow::response(200, result);
    });


}


void Server::setupTableRoutes(crow::SimpleApp& app)
{

    CROW_ROUTE(app, "/databases/<string>/tables")
        ([this](const std::string& databaseName)
    {
        auto databaseIt = databases.find(databaseName);

        if (databaseIt == databases.end())
            return crow::response(404, "Database not found");

        const Database& database = databaseIt->second;

        crow::json::wvalue result;

        result["tables"] = crow::json::wvalue::list();

        int index = 0;

        for (const Table& table : database.getTables())
        {
            result["tables"][index]["name"] = table.getName();
            ++index;
        }

        return crow::response(result);
    });

    CROW_ROUTE(app, "/databases/<string>/tables/<string>")
        ([this](
            const std::string& databaseName,
            const std::string& tableName)
    {
        auto databaseIt = databases.find(databaseName);

        if (databaseIt == databases.end())
            return crow::response(404, "Database not found");

        Database& database = databaseIt->second;

        Table* table = database.getTable(tableName);

        if (table == nullptr)
            return crow::response(404, "Table not found");

        crow::json::wvalue result;

        result["name"] = table->getName();

        // Обязательно создаём пустой список,
        // даже если колонок пока нет.
        result["columns"] = crow::json::wvalue::list();

        int index = 0;

        for (const Column& column : table->getColumns())
        {
            result["columns"][index]["name"] = column.name;
            result["columns"][index]["type"] =
                dataTypeToString(column.type);

            ++index;
        }

        return crow::response(result);
    });
    
       CROW_ROUTE(
            app,
            "/databases/<string>/tables/<string>"
        )
            .methods(crow::HTTPMethod::Post)
            ([this](
                const std::string& databaseName,
                const std::string& tableName)
        {
            auto databaseIt = databases.find(databaseName);

            if (databaseIt == databases.end())
            {
                return crow::response(404, "Database not found");
            }

            Database& database = databaseIt->second;
            if (database.getTable(tableName) != nullptr)
            {
                return crow::response(409, "Table already exists");
            }

            Table table(tableName);

            database.addTable(table);
            const std::string filename = databaseName + ".bin";

            if (!database.save(filename))
            {
                return crow::response(
                    500,
                    "Failed to save database"
                );
            }

            crow::json::wvalue result;

            result["message"] = "Table created";
            result["database"] = databaseName;
            result["table"] = tableName;

            return crow::response(201, result);
        });

        CROW_ROUTE(
            app,
            "/databases/<string>/tables/<string>"
        )
            .methods(crow::HTTPMethod::Delete)
            ([this](
                const std::string& databaseName,
                const std::string& tableName)
        {
            auto databaseIt = databases.find(databaseName);

            if (databaseIt == databases.end())
            {
                return crow::response(404, "Database not found");
            }

            Database& database = databaseIt->second;
            if (database.getTable(tableName) == nullptr)
            {
                return crow::response(404, "Table not found");
            }
            database.removeTable(tableName);
            const std::string filename = databaseName + ".bin";

            if (!database.save(filename))
            {
                return crow::response(
                    500,
                    "Failed to save database"
                );
            }

            crow::json::wvalue result;

            result["message"] = "Table deleted";
            result["database"] = databaseName;
            result["table"] = tableName;

            return crow::response(200, result);
        });


        CROW_ROUTE(
            app,
            "/databases/<string>/tables/<string>/rows"
        )
            ([this](
                const std::string& databaseName,
                const std::string& tableName)
        {
            auto databaseIt = databases.find(databaseName);

            if (databaseIt == databases.end())
                return crow::response(404, "Database not found");

            Database& database = databaseIt->second;

            Table* table = database.getTable(tableName);

            if (table == nullptr)
                return crow::response(404, "Table not found");

            crow::json::wvalue result;

            // Даже пустая таблица должна вернуть rows: [].
            result["rows"] = crow::json::wvalue::list();

            int rowIndex = 0;

            for (const Record& record : table->getRecords())
            {
                int valueIndex = 0;

                for (const Value& value : record.values)
                {
                    result["rows"][rowIndex]["values"][valueIndex] =
                        valueToJson(value);

                    ++valueIndex;
                }

                ++rowIndex;
            }

            return crow::response(result);
        });
}


void Server::setupRoutes(crow::SimpleApp& app)
{
    setupDatabaseRoutes(app);
    setupTableRoutes(app);

    CROW_ROUTE(app, "/")
        ([]()
    {
        return "Server is running";
    });
}


void Server::run()
{
    loadDatabases();

    crow::SimpleApp app;

    setupRoutes(app);

    app.port(18080).run();
}