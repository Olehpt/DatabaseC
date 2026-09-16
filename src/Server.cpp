#include "Server.h"

void Server::run()
{
    crow::SimpleApp app;

    CROW_ROUTE(app, "/")
        ([]()
    {
        return "Server is running";
    });

    app.port(18080).run();
}