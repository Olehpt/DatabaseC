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

public:
    void run();
};