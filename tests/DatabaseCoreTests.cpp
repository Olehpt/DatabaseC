#include "Database.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <limits>

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
int main() {
    try {
        Database db;
        Table table("items");
        table.addColumn({"id", DataType::Integer});
        table.addColumn({"price", DataType::Real});
        table.addColumn({"code", DataType::Char});
        table.addColumn({"name", DataType::String});
        table.addRecord({{std::int32_t{INT32_MIN}, 1.25, 'X', std::string("hello\0world", 11)}});
        table.addRecord({{std::int32_t{INT32_MAX}, -0.5, '\0', std::string{}}});
        db.addTable(table);
        std::string bytes;
        check(db.exportBinary(bytes), "export");
        Database restored;
        check(restored.importBinary(bytes), "import");
        check(restored.getTable("items")->getRecords()[0].values == table.getRecords()[0].values, "four types roundtrip");
        check(restored.getTable("items")->getRecords()[1].values == table.getRecords()[1].values, "boundary roundtrip");
        std::string again;
        check(restored.exportBinary(again) && again == bytes, "deterministic export");
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            check(!restored.importBinary(bytes.substr(0, i)), "truncated import rejected");
            check(restored.exportBinary(again) && again == bytes, "failed import preserves data");
        }
        check(!restored.importBinary(bytes + "junk"), "trailing bytes rejected");
        auto invalid = bytes; invalid[4] = 2;
        check(!restored.importBinary(invalid), "unknown version rejected");
        // Legacy files begin at the table count, without magic/version.
        check(restored.importBinary(bytes.substr(8)), "legacy compatibility");
        invalid = bytes;
        const std::size_t firstType = 12 + 4 + 5 + 4 + 4 + 2;
        invalid[firstType] = 9;
        check(!restored.importBinary(invalid), "unknown type rejected");
        invalid = bytes; invalid[12] = static_cast<char>(0xff); invalid[13] = static_cast<char>(0xff);
        check(!restored.importBinary(invalid), "oversized string length rejected");
        auto* t = restored.getTable("items");
        t->addColumn({"extra", DataType::String}, std::string("default"));
        check(std::get<std::string>(t->getRecords()[0].values.back()) == "default", "column backfill");
        t->renameColumn("extra", "note");
        t->removeColumn("price");
        auto record = t->getRecords()[0]; record.values[0] = std::int32_t{7};
        t->updateRecord(0, record);
        check(std::get<std::int32_t>(t->getRecords()[0].values[0]) == 7, "update");
        bool rejected = false;
        record.values[0] = std::string("wrong");
        try { t->updateRecord(0, record); } catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "model rejects wrong type");
        t->removeRecord(1); t->rename("renamed");
        check(restored.getTable("renamed")->getRecords().size() == 1, "delete and rename");
        Table complexTable("waves");
        complexTable.addColumn({"z", DataType::Complex});
        complexTable.addRecord({{Complex{1.5, -2.5}}});
        Database complexDb; complexDb.addTable(complexTable);
        std::string complexBytes;
        check(complexDb.exportBinary(complexBytes), "complex export");
        Database complexLoaded;
        check(complexLoaded.importBinary(complexBytes), "complex import");
        check(std::get<Complex>(complexLoaded.getTable("waves")->getRecords()[0].values[0]) == Complex{1.5, -2.5}, "complex roundtrip");
        const unsigned char expectedComplex[] = {0, 0, 0, 0, 0, 0, 0xf8, 0x3f, 0, 0, 0, 0, 0, 0, 4, 0xc0};
        for (std::size_t i = 0; i < 16; ++i)
            check(static_cast<unsigned char>(complexBytes[complexBytes.size() - 16 + i]) == expectedComplex[i], "complex little-endian layout");
        check(!complexLoaded.importBinary(complexBytes.substr(0, complexBytes.size() - 8)), "missing imaginary part rejected");
        auto nonfinite = complexBytes;
        for (std::size_t i = nonfinite.size() - 8; i < nonfinite.size(); ++i) nonfinite[i] = 0;
        nonfinite[nonfinite.size() - 2] = static_cast<char>(0xf0);
        nonfinite.back() = 0x7f;
        check(!complexLoaded.importBinary(nonfinite), "infinite imaginary component rejected on import");
        check(complexLoaded.exportBinary(again) && again == complexBytes, "complex failed import preserves database");
        for (const auto& bad : {Complex{std::numeric_limits<double>::infinity(), 0},
                              Complex{0, std::numeric_limits<double>::quiet_NaN()}}) {
            rejected = false;
            try { complexTable.addRecord({{bad}}); } catch (const std::invalid_argument&) { rejected = true; }
            check(rejected, "nonfinite complex rejected by model");
        }
        complexTable.addColumn({"zero", DataType::Complex});
        check(std::get<Complex>(complexTable.getRecords()[0].values[1]) == Complex{}, "complex zero default");
        complexTable.addColumn({"preset", DataType::Complex}, Complex{-3, 4});
        check(std::get<Complex>(complexTable.getRecords()[0].values[2]) == Complex{-3, 4}, "complex explicit default");
        auto complexRecord = complexTable.getRecords()[0]; complexRecord.values[0] = Complex{0, 7};
        complexTable.updateRecord(0, complexRecord);
        check(std::get<Complex>(complexTable.getRecords()[0].values[0]) == Complex{0, 7}, "complex update");
        check(complexRecord.to_string(Complex{1.5, -2.5}) == "1.500000-2.500000i", "complex text conversion");
        restored.addTable(complexTable);
        const auto file = std::filesystem::current_path() / "core-test-roundtrip.bin";
        check(restored.save(file.string()), "save");
        Database loaded; check(loaded.load(file.string()), "load");
        check(loaded.exportBinary(again), "loaded export");
        check(restored.exportBinary(bytes) && bytes == again, "disk roundtrip");
        // A failed replacement must preserve the previously committed file.
        std::filesystem::create_directory(file.string() + ".tmp");
        check(!db.save(file.string()), "save failure");
        check(loaded.load(file.string()) && loaded.exportBinary(again) && again == bytes, "save failure preserves disk");
        std::filesystem::remove(file.string() + ".tmp");
        std::filesystem::remove(file);
        std::cout << "Core tests passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
