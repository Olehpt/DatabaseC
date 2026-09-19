#include "Server.h"
#include "Database.h"
#include <filesystem>


void Server::run()
{
    namespace fs = std::filesystem;

    for (const auto& entry : fs::directory_iterator(fs::current_path()))
    {
        if (!entry.is_regular_file())
            continue;

        if (entry.path().extension() != ".bin")
            continue;

        std::string filename = entry.path().string();
        std::string databaseName = entry.path().stem().string();

        Database db;

        if (db.load(filename))
        {
            databases.emplace(databaseName, std::move(db));

            std::cout << "Loaded database: "
                << databaseName
                << " (" << filename << ")\n";
        }
        else
        {
            std::cerr << "Failed to load database: "
                << filename << '\n';
        }
    }

    std::cout << "Loaded " << databases.size()
        << " database(s)\n";

    crow::SimpleApp app;

    CROW_ROUTE(app, "/")
        ([]()
    {
        return "Server is running";
    });

    app.port(8080).run();
}