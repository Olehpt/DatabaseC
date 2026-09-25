#include "Server.h"

#include <filesystem>
#include <iostream>
#include <type_traits>
#include <variant>

#include <limits>

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

    auto parseDataType =
        [](const std::string& typeName, DataType& type) -> bool
    {
        if (typeName == "integer")
        {
            type = DataType::Integer;
            return true;
        }

        if (typeName == "real")
        {
            type = DataType::Real;
            return true;
        }

        if (typeName == "char")
        {
            type = DataType::Char;
            return true;
        }

        if (typeName == "string")
        {
            type = DataType::String;
            return true;
        }

        return false;
    };

    auto parseValue =
        [](const crow::json::rvalue& jsonValue,
            DataType type,
            Value& value) -> bool
    {
        using JsonType = crow::json::type;
        using NumberType = crow::json::num_type;

        if (jsonValue.t() != JsonType::Number &&
            jsonValue.t() != JsonType::String)
        {
            return false;
        }

        switch (type)
        {
        case DataType::Integer:
        {
            if (jsonValue.t() != JsonType::Number)
                return false;

            if (jsonValue.nt() == NumberType::Signed_integer)
            {
                const auto number = jsonValue.i();

                if (number <
                    std::numeric_limits<std::int32_t>::min() ||
                    number >
                    std::numeric_limits<std::int32_t>::max())
                {
                    return false;
                }

                value = static_cast<std::int32_t>(number);
                return true;
            }

            if (jsonValue.nt() == NumberType::Unsigned_integer)
            {
                const auto number = jsonValue.u();

                if (number >
                    static_cast<std::uint64_t>(
                        std::numeric_limits<std::int32_t>::max()))
                {
                    return false;
                }

                value = static_cast<std::int32_t>(number);
                return true;
            }

            return false;
        }

        case DataType::Real:
        {
            if (jsonValue.t() != JsonType::Number)
                return false;

            value = jsonValue.d();
            return true;
        }

        case DataType::Char:
        {
            if (jsonValue.t() != JsonType::String)
                return false;

            const std::string str =
                static_cast<std::string>(jsonValue.s());

            if (str.size() != 1)
                return false;

            value = str[0];
            return true;
        }

        case DataType::String:
        {
            if (jsonValue.t() != JsonType::String)
                return false;

            value =
                static_cast<std::string>(jsonValue.s());

            return true;
        }
        }

        return false;
    };

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
        CROW_ROUTE(
            app,
            "/databases/<string>/tables/<string>/columns"
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

            result["columns"] =
                crow::json::wvalue::list();

            int index = 0;

            for (const Column& column : table->getColumns())
            {
                result["columns"][index]["name"] =
                    column.name;

                result["columns"][index]["type"] =
                    dataTypeToString(column.type);

                ++index;
            }

            return crow::response(result);
        });


        CROW_ROUTE(
            app,
            "/databases/<string>/tables/<string>/columns"
        )
            .methods(crow::HTTPMethod::Post)
            ([this, parseDataType](
                const crow::request& req,
                const std::string& databaseName,
                const std::string& tableName)
        {
            auto databaseIt = databases.find(databaseName);

            if (databaseIt == databases.end())
                return crow::response(404, "Database not found");

            auto body = crow::json::load(req.body);

            if (!body ||
                body.t() != crow::json::type::Object)
            {
                return crow::response(
                    400,
                    "Invalid JSON"
                );
            }

            if (!body.has("name") ||
                !body.has("type"))
            {
                return crow::response(
                    400,
                    "Column name and type are required"
                );
            }

            if (body["name"].t() != crow::json::type::String ||
                body["type"].t() != crow::json::type::String)
            {
                return crow::response(
                    400,
                    "Invalid column data"
                );
            }

            const std::string columnName =
                static_cast<std::string>(body["name"].s());

            const std::string typeName =
                static_cast<std::string>(body["type"].s());

            if (columnName.empty())
                return crow::response(
                    400,
                    "Column name cannot be empty"
                );

            DataType columnType;

            if (!parseDataType(typeName, columnType))
            {
                return crow::response(
                    400,
                    "Invalid column type"
                );
            }

            /*
             * Work on a copy so that if saving fails,
             * the in-memory database remains unchanged.
             */
            Database updatedDatabase =
                databaseIt->second;

            Table* table =
                updatedDatabase.getTable(tableName);

            if (table == nullptr)
                return crow::response(404, "Table not found");

            for (const Column& column : table->getColumns())
            {
                if (column.name == columnName)
                {
                    return crow::response(
                        409,
                        "Column already exists"
                    );
                }
            }

            /*
             * Table::addColumn() does not add a corresponding
             * value to existing records. Therefore we don't
             * allow schema expansion after records already exist.
             */
            if (!table->getRecords().empty())
            {
                return crow::response(
                    409,
                    "Cannot add column to a table with records"
                );
            }

            Column column;
            column.name = columnName;
            column.type = columnType;

            table->addColumn(column);

            const std::string filename =
                databaseName + ".bin";

            if (!updatedDatabase.save(filename))
            {
                return crow::response(
                    500,
                    "Failed to save database"
                );
            }

            databaseIt->second =
                std::move(updatedDatabase);

            crow::json::wvalue result;

            result["message"] =
                "Column created";

            result["database"] =
                databaseName;

            result["table"] =
                tableName;

            result["column"] =
                columnName;

            return crow::response(
                201,
                result
            );
        });


        CROW_ROUTE(
            app,
            "/databases/<string>/tables/<string>/columns/<string>"
        )
            .methods(crow::HTTPMethod::Delete)
            ([this](
                const std::string& databaseName,
                const std::string& tableName,
                const std::string& columnName)
        {
            auto databaseIt =
                databases.find(databaseName);

            if (databaseIt == databases.end())
                return crow::response(
                    404,
                    "Database not found"
                );

            Database updatedDatabase =
                databaseIt->second;

            Table* table =
                updatedDatabase.getTable(tableName);

            if (table == nullptr)
                return crow::response(
                    404,
                    "Table not found"
                );

            bool columnFound = false;

            for (const Column& column : table->getColumns())
            {
                if (column.name == columnName)
                {
                    columnFound = true;
                    break;
                }
            }

            if (!columnFound)
            {
                return crow::response(
                    404,
                    "Column not found"
                );
            }

            table->removeColumn(columnName);

            const std::string filename =
                databaseName + ".bin";

            if (!updatedDatabase.save(filename))
            {
                return crow::response(
                    500,
                    "Failed to save database"
                );
            }

            databaseIt->second =
                std::move(updatedDatabase);

            crow::json::wvalue result;

            result["message"] =
                "Column deleted";

            result["database"] =
                databaseName;

            result["table"] =
                tableName;

            result["column"] =
                columnName;

            return crow::response(
                200,
                result
            );
        });


        CROW_ROUTE(
            app,
            "/databases/<string>/tables/<string>/rows"
        )
            .methods(crow::HTTPMethod::Post)
            ([this, parseValue](
                const crow::request& req,
                const std::string& databaseName,
                const std::string& tableName)
        {
            auto databaseIt =
                databases.find(databaseName);

            if (databaseIt == databases.end())
                return crow::response(
                    404,
                    "Database not found"
                );

            auto body = crow::json::load(req.body);

            if (!body ||
                body.t() != crow::json::type::Object)
            {
                return crow::response(
                    400,
                    "Invalid JSON"
                );
            }

            if (!body.has("values") ||
                body["values"].t() !=
                crow::json::type::List)
            {
                return crow::response(
                    400,
                    "Values array is required"
                );
            }

            Database updatedDatabase =
                databaseIt->second;

            Table* table =
                updatedDatabase.getTable(tableName);

            if (table == nullptr)
                return crow::response(
                    404,
                    "Table not found"
                );

            const auto& jsonValues =
                body["values"];

            if (jsonValues.size() !=
                table->getColumns().size())
            {
                return crow::response(
                    400,
                    "Invalid number of values"
                );
            }

            Record record;

            for (std::size_t i = 0;
                i < table->getColumns().size();
                ++i)
            {
                Value value;

                if (!parseValue(
                    jsonValues[i],
                    table->getColumns()[i].type,
                    value))
                {
                    return crow::response(
                        400,
                        "Invalid value for column: " +
                        table->getColumns()[i].name
                    );
                }

                record.values.push_back(
                    std::move(value)
                );
            }

            const std::size_t rowIndex =
                table->getRecords().size();

            table->addRecord(record);

            const std::string filename =
                databaseName + ".bin";

            if (!updatedDatabase.save(filename))
            {
                return crow::response(
                    500,
                    "Failed to save database"
                );
            }

            databaseIt->second =
                std::move(updatedDatabase);

            crow::json::wvalue result;

            result["message"] =
                "Row created";

            result["database"] =
                databaseName;

            result["table"] =
                tableName;

            result["index"] =
                rowIndex;

            return crow::response(
                201,
                result
            );
        });


        CROW_ROUTE(
            app,
            "/databases/<string>/tables/<string>/rows/<int>"
        )
            .methods(crow::HTTPMethod::Put)
            ([this, parseValue](
                const crow::request& req,
                const std::string& databaseName,
                const std::string& tableName,
                int rowIndex)
        {
            if (rowIndex < 0)
                return crow::response(
                    400,
                    "Invalid row index"
                );

            auto databaseIt =
                databases.find(databaseName);

            if (databaseIt == databases.end())
                return crow::response(
                    404,
                    "Database not found"
                );

            auto body =
                crow::json::load(req.body);

            if (!body ||
                body.t() != crow::json::type::Object)
            {
                return crow::response(
                    400,
                    "Invalid JSON"
                );
            }

            if (!body.has("values") ||
                body["values"].t() !=
                crow::json::type::List)
            {
                return crow::response(
                    400,
                    "Values array is required"
                );
            }

            Database updatedDatabase =
                databaseIt->second;

            Table* table =
                updatedDatabase.getTable(tableName);

            if (table == nullptr)
                return crow::response(
                    404,
                    "Table not found"
                );

            const std::size_t index =
                static_cast<std::size_t>(rowIndex);

            if (index >= table->getRecords().size())
            {
                return crow::response(
                    404,
                    "Row not found"
                );
            }

            const auto& jsonValues =
                body["values"];

            if (jsonValues.size() !=
                table->getColumns().size())
            {
                return crow::response(
                    400,
                    "Invalid number of values"
                );
            }

            Record record;

            for (std::size_t i = 0;
                i < table->getColumns().size();
                ++i)
            {
                Value value;

                if (!parseValue(
                    jsonValues[i],
                    table->getColumns()[i].type,
                    value))
                {
                    return crow::response(
                        400,
                        "Invalid value for column: " +
                        table->getColumns()[i].name
                    );
                }

                record.values.push_back(
                    std::move(value)
                );
            }

            table->updateRecord(index, record);

            const std::string filename =
                databaseName + ".bin";

            if (!updatedDatabase.save(filename))
            {
                return crow::response(
                    500,
                    "Failed to save database"
                );
            }

            databaseIt->second =
                std::move(updatedDatabase);

            crow::json::wvalue result;

            result["message"] =
                "Row updated";

            result["database"] =
                databaseName;

            result["table"] =
                tableName;

            result["index"] =
                index;

            return crow::response(
                200,
                result
            );
        });


        CROW_ROUTE(
            app,
            "/databases/<string>/tables/<string>/rows/<int>"
        )
            .methods(crow::HTTPMethod::Delete)
            ([this](
                const std::string& databaseName,
                const std::string& tableName,
                int rowIndex)
        {
            if (rowIndex < 0)
                return crow::response(
                    400,
                    "Invalid row index"
                );

            auto databaseIt =
                databases.find(databaseName);

            if (databaseIt == databases.end())
                return crow::response(
                    404,
                    "Database not found"
                );

            Database updatedDatabase =
                databaseIt->second;

            Table* table =
                updatedDatabase.getTable(tableName);

            if (table == nullptr)
                return crow::response(
                    404,
                    "Table not found"
                );

            const std::size_t index =
                static_cast<std::size_t>(rowIndex);

            if (index >= table->getRecords().size())
            {
                return crow::response(
                    404,
                    "Row not found"
                );
            }

            table->removeRecord(index);

            const std::string filename =
                databaseName + ".bin";

            if (!updatedDatabase.save(filename))
            {
                return crow::response(
                    500,
                    "Failed to save database"
                );
            }

            databaseIt->second =
                std::move(updatedDatabase);

            crow::json::wvalue result;

            result["message"] =
                "Row deleted";

            result["database"] =
                databaseName;

            result["table"] =
                tableName;

            result["index"] =
                index;

            return crow::response(
                200,
                result
            );
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