// Handles db interactions
#ifndef DB_H
#define DB_H
#include <sqlite3.h>

sqlite3 *initDB();
void closeDB(sqlite3 *db);


#endif /* DB_H */