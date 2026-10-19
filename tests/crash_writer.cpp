#include "seat/database.hpp"
#include <cstdlib>
// Test-only executable: simulate loss of process before any commit/destructor.
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    seat::db::Connection db(argv[1]);
    seat::db::Transaction transaction(db);
    db.exec("UPDATE slots SET occupied=occupied+1");
    std::_Exit(0);
}
