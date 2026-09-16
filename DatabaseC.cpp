#include "DatabaseC.h"
#include "src/BinaryFile.h"
#include "src/Database.h"

using namespace std;

int main()
{
    Database db;

    if (db.load("database.bin"))
    {
        Table* table = db.getTable("Students");
    }
    for (auto& t : db.getTables()) {
        for (auto r : t.getRecords()) {
			for (auto& v : r.values) {
				cout << r.to_string(v) << " ";
			}
			cout << endl;
        }
    }
}
