#include "Database.h"
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace {
constexpr std::uint32_t magic = 0x31434244;
constexpr std::size_t maxFileSize = 64 * 1024 * 1024;
}
void Database::addTable(const Table& table) {
    if (!table.valid() || getTable(table.getName()) || tables.size() >= 10000) throw std::invalid_argument("Invalid or duplicate table");
    tables.push_back(table);
}
Table* Database::getTable(const std::string& name) {
    for (auto& table : tables) if (table.getName() == name) return &table;
    return nullptr;
}
const std::vector<Table>& Database::getTables() const { return tables; }
void Database::removeTable(const std::string& name) {
    for (auto it = tables.begin(); it != tables.end(); ++it)
        if (it->getName() == name) { tables.erase(it); return; }
}
bool Database::exportBinary(std::string& bytes) const {
    BinaryFile file;
    file.openMemory();
    if (tables.size() > 10000 || !file.writeUInt32(magic) || !file.writeUInt32(1) ||
        !file.writeUInt32(static_cast<std::uint32_t>(tables.size()))) return false;
    std::set<std::string> names;
    std::size_t rows = 0, cells = 0;
    for (const auto& table : tables) {
        rows += table.getRecords().size();
        cells += table.getRecords().size() * table.getColumns().size();
        if (rows > 1000000 || cells > 1000000) return false;
        if (!names.insert(table.getName()).second || !table.save(file)) return false;
    }
    bytes = file.bytes();
    return bytes.size() <= maxFileSize;
}
bool Database::importBinary(const std::string& bytes) {
    if (bytes.size() > maxFileSize) return false;
    BinaryFile file;
    file.openMemory(bytes);
    std::uint32_t count;
    if (!file.readUInt32(count)) return false;
    if (count == magic) {
        std::uint32_t version;
        if (!file.readUInt32(version) || version != 1 || !file.readUInt32(count)) return false;
    }
    if (count > 10000 || count > file.remaining() / 12) return false;
    Database candidate;
    std::size_t rows = 0, cells = 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        Table table;
        if (!table.load(file) || candidate.getTable(table.getName())) return false;
        rows += table.getRecords().size();
        cells += table.getRecords().size() * table.getColumns().size();
        if (rows > 1000000 || cells > 1000000) return false;
        candidate.tables.push_back(std::move(table));
    }
    if (file.remaining() != 0) return false;
    tables.swap(candidate.tables);
    return true;
}
bool Database::save(const std::string& filename) const {
    std::string bytes;
    if (!exportBinary(bytes)) return false;
    const auto target = std::filesystem::path(filename);
    const auto temporary = std::filesystem::path(filename + ".tmp");
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) return false;
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        output.flush();
        output.close();
        if (!output) { std::error_code ec; std::filesystem::remove(temporary, ec); return false; }
    }
    bool replaced;
#ifdef _WIN32
    replaced = MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::error_code ec;
    std::filesystem::rename(temporary, target, ec);
    replaced = !ec;
#endif
    if (!replaced) { std::error_code ec; std::filesystem::remove(temporary, ec); }
    return replaced;
}
bool Database::load(const std::string& filename) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(filename, ec);
    if (ec || size > maxFileSize) return false;
    std::ifstream input(filename, std::ios::binary);
    if (!input) return false;
    std::string bytes(static_cast<std::size_t>(size), '\0');
    if (!input.read(bytes.data(), static_cast<std::streamsize>(size))) return false;
    return importBinary(bytes);
}
