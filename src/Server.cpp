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
    // GET /databases
    //
    // Возвращает список всех загруженных .bin файлов.

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
}


void Server::setupTableRoutes(crow::SimpleApp& app)
{
    // GET /databases/<database>/tables
    //
    // Возвращает список таблиц выбранной базы.

    CROW_ROUTE(app, "/databases/<string>/tables")
        ([this](const std::string& databaseName)
    {
        auto databaseIt = databases.find(databaseName);

        if (databaseIt == databases.end())
            return crow::response(404, "Database not found");

        const Database& database = databaseIt->second;

        crow::json::wvalue result;

        int index = 0;

        for (const Table& table : database.getTables())
        {
            result["tables"][index]["name"] = table.getName();

            ++index;
        }

        return crow::response(result);
    });


    // GET /databases/<database>/tables/<table>
    //
    // Возвращает структуру выбранной таблицы:
    // имя + список колонок.

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


    // GET /databases/<database>/tables/<table>/rows
    //
    // Возвращает все записи таблицы.

    CROW_ROUTE(app, "/databases/<string>/tables/<string>/rows")
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

    // Проверка работы сервера
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