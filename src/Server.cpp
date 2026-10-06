#include "Server.h"
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <type_traits>
namespace {
using Method = crow::HTTPMethod;
using JsonType = crow::json::type;
bool validName(const std::string& name) { return !name.empty() && name.size() <= 1024 && name.find('\0') == std::string::npos; }
bool validDatabaseName(const std::string& name) {
    if (!validName(name) || name.size() > 128 || name.back() == '.' || name.back() == ' ') return false;
    for (unsigned char c : name)
        if (c < 32 || c == 127 || std::string("/\\:<>\"|?*").find(c) != std::string::npos) return false;
    auto stem = name.substr(0, name.find('.'));
    std::transform(stem.begin(), stem.end(), stem.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    if (stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL") return false;
    if (stem.size() == 4 && (stem.substr(0, 3) == "COM" || stem.substr(0, 3) == "LPT") && stem[3] >= '0' && stem[3] <= '9') return false;
    return true;
}
std::string typeName(DataType type) {
    switch (type) {
    case DataType::Integer: return "integer";
    case DataType::Real: return "real";
    case DataType::Char: return "char";
    case DataType::String: return "string";
    case DataType::Complex: return "complex";
    }
    return "unknown";
}
bool parseType(const std::string& name, DataType& type) {
    for (unsigned i = 0; i <= static_cast<unsigned>(DataType::Complex); ++i) {
        type = static_cast<DataType>(i);
        if (typeName(type) == name) return true;
    }
    return false;
}
crow::json::wvalue valueJson(const Value& value) {
    return std::visit([](const auto& v) -> crow::json::wvalue {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, char>) return std::string(1, v);
        else if constexpr (std::is_same_v<T, Complex>) {
            crow::json::wvalue result;
            result["real"] = v.real(); result["imag"] = v.imag();
            return result;
        }
        else return v;
    }, value);
}
bool parseValue(const crow::json::rvalue& json, DataType type, Value& value) {
    if (type == DataType::Complex) {
        if (json.t() != JsonType::Object || json.size() != 2 || !json.has("real") || !json.has("imag") ||
            json["real"].t() != JsonType::Number || json["imag"].t() != JsonType::Number) return false;
        const double real = json["real"].d(), imag = json["imag"].d();
        if (!std::isfinite(real) || !std::isfinite(imag)) return false;
        value = Complex{real, imag}; return true;
    }
    if (type == DataType::Integer) {
        if (json.t() != JsonType::Number) return false;
        if (json.nt() == crow::json::num_type::Signed_integer) {
            auto n = json.i();
            if (n < INT32_MIN || n > INT32_MAX) return false;
            value = static_cast<std::int32_t>(n); return true;
        }
        if (json.nt() == crow::json::num_type::Unsigned_integer && json.u() <= INT32_MAX) {
            value = static_cast<std::int32_t>(json.u()); return true;
        }
        return false;
    }
    if (type == DataType::Real) {
        if (json.t() != JsonType::Number || !std::isfinite(json.d())) return false;
        value = json.d(); return true;
    }
    if (json.t() != JsonType::String) return false;
    std::string s = json.s();
    if (type == DataType::Char) {
        if (s.size() != 1) return false;
        value = s[0]; return true;
    }
    if (type != DataType::String || s.size() > 16 * 1024 * 1024) return false;
    value = std::move(s); return true;
}
crow::json::wvalue columnsJson(const Table& table) {
    crow::json::wvalue result;
    result["columns"] = crow::json::wvalue::list();
    std::size_t i = 0;
    for (const auto& column : table.getColumns()) {
        result["columns"][i]["name"] = column.name;
        result["columns"][i++]["type"] = typeName(column.type);
    }
    return result;
}
crow::json::wvalue rowJson(const Record& record, std::size_t index) {
    crow::json::wvalue row;
    row["index"] = index;
    row["values"] = crow::json::wvalue::list();
    for (std::size_t i = 0; i < record.values.size(); ++i) row["values"][i] = valueJson(record.values[i]);
    return row;
}
bool parseRecord(const crow::json::rvalue& body, const Table& table, Record& record) {
    if (!body || body.t() != JsonType::Object || !body.has("values") || body["values"].t() != JsonType::List ||
        body["values"].size() != table.getColumns().size()) return false;
    for (std::size_t i = 0; i < table.getColumns().size(); ++i) {
        Value value;
        if (!parseValue(body["values"][i], table.getColumns()[i].type, value)) return false;
        record.values.push_back(std::move(value));
    }
    return true;
}
crow::response message(int code, const std::string& text) {
    crow::json::wvalue result; result["message"] = text;
    return crow::response(code, result);
}
}
void Server::loadDatabases() {
    for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::current_path())) {
        if (!entry.is_regular_file() || entry.path().extension() != ".bin") continue;
        const auto name = entry.path().stem().string();
        if (!validDatabaseName(name)) continue;
        Database db;
        if (db.load(entry.path().string())) databases.emplace(name, std::move(db));
        else std::cerr << "Failed to load database: " << entry.path() << '\n';
    }
}
crow::response Server::withTable(const crow::request& req, const std::string& database,
    const std::string& tableName, const std::function<crow::response(Table&, Database&)>& action) {
    std::lock_guard lock(mutex);
    if (!validDatabaseName(database) || !validName(tableName)) return message(400, "Invalid name");
    auto it = databases.find(database);
    if (it == databases.end()) return message(404, "Database not found");
    Database candidate = it->second;
    auto* table = candidate.getTable(tableName);
    if (!table) return message(404, "Table not found");
    try {
        auto response = action(*table, candidate);
        if (req.method != Method::Get && response.code >= 200 && response.code < 300) {
            if (!candidate.save(database + ".bin")) return message(500, "Failed to save database");
            it->second = std::move(candidate);
            crow::json::wvalue result(crow::json::load(response.body));
            result["database"] = database;
            result["table"] = tableName;
            response = crow::response(response.code, result);
        }
        return response;
    } catch (const std::invalid_argument& e) { return message(400, e.what()); }
      catch (const std::out_of_range& e) { return message(400, e.what()); }
}
void Server::setupRoutes(crow::SimpleApp& app) {
    CROW_ROUTE(app, "/")([] { return "Server is running"; });
    CROW_ROUTE(app, "/databases")([this] {
        std::lock_guard lock(mutex);
        crow::json::wvalue result; result["databases"] = crow::json::wvalue::list();
        std::size_t i = 0;
        for (const auto& [name, db] : databases) result["databases"][i++] = name;
        return crow::response(result);
    });
    CROW_ROUTE(app, "/databases/<string>").methods(Method::Post, Method::Delete)
    ([this](const crow::request& req, const std::string& name) {
        std::lock_guard lock(mutex);
        if (!validDatabaseName(name)) return message(400, "Invalid database name");
        auto it = databases.find(name);
        if (req.method == Method::Post) {
            if (it != databases.end() || std::filesystem::exists(name + ".bin")) return message(409, "Database already exists");
            Database db;
            if (!db.save(name + ".bin")) return message(500, "Failed to create database file");
            databases.emplace(name, std::move(db));
            crow::json::wvalue result; result["message"] = "Database created"; result["name"] = name;
            return crow::response(201, result);
        }
        if (it == databases.end()) return message(404, "Database not found");
        std::error_code ec;
        std::filesystem::remove(name + ".bin", ec);
        if (ec) return message(500, "Failed to delete database file");
        databases.erase(it);
        crow::json::wvalue result; result["message"] = "Database deleted"; result["name"] = name;
        return crow::response(result);
    });
    CROW_ROUTE(app, "/databases/<string>/export")
    ([this](const std::string& name) {
        std::lock_guard lock(mutex);
        if (!validDatabaseName(name)) return message(400, "Invalid database name");
        auto it = databases.find(name);
        if (it == databases.end()) return message(404, "Database not found");
        std::string bytes;
        if (!it->second.exportBinary(bytes)) return message(500, "Failed to export database");
        crow::response response(200, std::move(bytes));
        response.set_header("Content-Type", "application/octet-stream");
        response.set_header("Content-Disposition", "attachment; filename=\"database.bin\"");
        return response;
    });
    CROW_ROUTE(app, "/databases/<string>/import").methods(Method::Post, Method::Put)
    ([this](const crow::request& req, const std::string& name) {
        std::lock_guard lock(mutex);
        if (!validDatabaseName(name)) return message(400, "Invalid database name");
        if (req.body.size() > 64 * 1024 * 1024) return message(413, "Database exceeds 64 MiB");
        auto it = databases.find(name);
        if (req.method == Method::Post && (it != databases.end() || std::filesystem::exists(name + ".bin"))) return message(409, "Database already exists");
        if (req.method == Method::Put && it == databases.end()) return message(404, "Database not found");
        Database candidate;
        if (!candidate.importBinary(req.body)) return message(400, "Invalid database binary file");
        if (!candidate.save(name + ".bin")) return message(500, "Failed to save imported database");
        databases.insert_or_assign(name, std::move(candidate));
        return message(req.method == Method::Post ? 201 : 200, "Database imported");
    });
    CROW_ROUTE(app, "/databases/<string>/tables")([this](const std::string& name) {
        std::lock_guard lock(mutex);
        if (!validDatabaseName(name)) return message(400, "Invalid database name");
        auto it = databases.find(name);
        if (it == databases.end()) return message(404, "Database not found");
        crow::json::wvalue result; result["tables"] = crow::json::wvalue::list();
        std::size_t i = 0;
        for (const auto& table : it->second.getTables()) result["tables"][i++]["name"] = table.getName();
        return crow::response(result);
    });
    CROW_ROUTE(app, "/databases/<string>/tables/<string>").methods(Method::Get, Method::Post, Method::Delete, Method::Put)
    ([this](const crow::request& req, const std::string& name, const std::string& tableName) {
        if (req.method == Method::Post) {
            std::lock_guard lock(mutex);
            if (!validDatabaseName(name) || !validName(tableName)) return message(400, "Invalid name");
            auto it = databases.find(name);
            if (it == databases.end()) return message(404, "Database not found");
            if (it->second.getTable(tableName)) return message(409, "Table already exists");
            Database candidate = it->second;
            if (candidate.getTables().size() >= 10000) return message(400, "Table limit reached");
            candidate.addTable(Table(tableName));
            if (!candidate.save(name + ".bin")) return message(500, "Failed to save database");
            it->second = std::move(candidate);
            crow::json::wvalue result; result["message"] = "Table created";
            result["database"] = name; result["table"] = tableName;
            return crow::response(201, result);
        }
        return withTable(req, name, tableName, [&](Table& table, Database& db) {
            if (req.method == Method::Get) {
                auto result = columnsJson(table); result["name"] = table.getName();
                result["row_count"] = table.getRecords().size(); return crow::response(result);
            }
            if (req.method == Method::Delete) { db.removeTable(tableName); return message(200, "Table deleted"); }
            auto body = crow::json::load(req.body);
            if (!body || body.t() != JsonType::Object || !body.has("name") || body["name"].t() != JsonType::String) return message(400, "Name is required");
            std::string newName = body["name"].s();
            if (!validName(newName)) return message(400, "Invalid table name");
            if (newName != tableName && db.getTable(newName)) return message(409, "Table already exists");
            table.rename(newName); return message(200, "Table renamed");
        });
    });
    CROW_ROUTE(app, "/databases/<string>/tables/<string>/columns").methods(Method::Get, Method::Post)
    ([this](const crow::request& req, const std::string& name, const std::string& tableName) {
        return withTable(req, name, tableName, [&](Table& table, Database&) {
            if (req.method == Method::Get) return crow::response(columnsJson(table));
            auto body = crow::json::load(req.body);
            if (!body || body.t() != JsonType::Object || !body.has("name") || !body.has("type") ||
                body["name"].t() != JsonType::String || body["type"].t() != JsonType::String) return message(400, "Column name and type are required");
            Column column; column.name = body["name"].s();
            if (!validName(column.name) || !parseType(body["type"].s(), column.type)) return message(400, "Invalid column");
            for (const auto& c : table.getColumns()) if (c.name == column.name) return message(409, "Column already exists");
            if (body.has("default")) {
                Value value;
                if (!parseValue(body["default"], column.type, value)) return message(400, "Invalid default value");
                table.addColumn(column, value);
            } else table.addColumn(column);
            crow::json::wvalue result; result["message"] = "Column created"; result["column"] = column.name;
            return crow::response(201, result);
        });
    });
    CROW_ROUTE(app, "/databases/<string>/tables/<string>/columns/<string>").methods(Method::Delete, Method::Put)
    ([this](const crow::request& req, const std::string& name, const std::string& tableName, const std::string& columnName) {
        return withTable(req, name, tableName, [&](Table& table, Database&) {
            auto found = std::find_if(table.getColumns().begin(), table.getColumns().end(), [&](const Column& c) { return c.name == columnName; });
            if (found == table.getColumns().end()) return message(404, "Column not found");
            if (req.method == Method::Delete) {
                table.removeColumn(columnName);
                crow::json::wvalue result; result["message"] = "Column deleted"; result["column"] = columnName;
                return crow::response(result);
            }
            auto body = crow::json::load(req.body);
            if (!body || body.t() != JsonType::Object || !body.has("name") || body["name"].t() != JsonType::String) return message(400, "Name is required");
            std::string newName = body["name"].s();
            if (!validName(newName)) return message(400, "Invalid column name");
            for (const auto& c : table.getColumns()) if (c.name == newName && c.name != columnName) return message(409, "Column already exists");
            table.renameColumn(columnName, newName); return message(200, "Column renamed");
        });
    });
    CROW_ROUTE(app, "/databases/<string>/tables/<string>/rows").methods(Method::Get, Method::Post, Method::Delete)
    ([this](const crow::request& req, const std::string& name, const std::string& tableName) {
        return withTable(req, name, tableName, [&](Table& table, Database&) {
            if (req.method == Method::Get) {
                crow::json::wvalue result; result["rows"] = crow::json::wvalue::list();
                for (std::size_t i = 0; i < table.getRecords().size(); ++i) result["rows"][i] = rowJson(table.getRecords()[i], i);
                return crow::response(result);
            }
            if (req.method == Method::Delete) {
                while (!table.getRecords().empty()) table.removeRecord(table.getRecords().size() - 1);
                return message(200, "Rows deleted");
            }
            Record record;
            if (!parseRecord(crow::json::load(req.body), table, record)) return message(400, "Invalid values: check count and column types");
            auto index = table.getRecords().size(); table.addRecord(record);
            auto result = rowJson(record, index); result["message"] = "Row created";
            return crow::response(201, result);
        });
    });
    CROW_ROUTE(app, "/databases/<string>/tables/<string>/rows/<int>").methods(Method::Get, Method::Put, Method::Patch, Method::Delete)
    ([this](const crow::request& req, const std::string& name, const std::string& tableName, int index) {
        if (index < 0) return message(400, "Invalid row index");
        return withTable(req, name, tableName, [&](Table& table, Database&) {
            if (static_cast<std::size_t>(index) >= table.getRecords().size()) return message(404, "Row not found");
            if (req.method == Method::Get) return crow::response(rowJson(table.getRecords()[index], index));
            if (req.method == Method::Delete) {
                table.removeRecord(index);
                crow::json::wvalue result; result["message"] = "Row deleted"; result["index"] = index;
                return crow::response(result);
            }
            auto body = crow::json::load(req.body);
            Record record;
            if (req.method == Method::Put) {
                if (!parseRecord(body, table, record)) return message(400, "Invalid values: check count and column types");
            } else {
                if (!body || body.t() != JsonType::Object || !body.has("values") || body["values"].t() != JsonType::Object || body["values"].size() == 0)
                    return message(400, "Values object keyed by column name is required");
                record = table.getRecords()[index];
                for (const auto& key : body["values"].keys()) {
                    auto c = std::find_if(table.getColumns().begin(), table.getColumns().end(), [&](const Column& column) { return column.name == key; });
                    if (c == table.getColumns().end()) return message(400, "Unknown column: " + key);
                    auto position = static_cast<std::size_t>(c - table.getColumns().begin());
                    if (!parseValue(body["values"][key], c->type, record.values[position])) return message(400, "Invalid value for column: " + key);
                }
            }
            table.updateRecord(index, record);
            auto result = rowJson(record, index); result["message"] = "Row updated";
            return crow::response(result);
        });
    });
}
void Server::run() {
    loadDatabases();
    crow::SimpleApp app;
    setupRoutes(app);
    app.port(18080).run();
}
