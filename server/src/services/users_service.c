#include "users_service.h"
#include "db.h"
#include <stdio.h>

int users_lookup(sqlite3 *db, const char *fingerprint) {
	sqlite3_stmt *stmt;
	int rc, user_id;

	rc = sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION;", NULL, NULL, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "users_lookup: begin transaction failed: %s\n", sqlite3_errmsg(db));
		return -1;
	}

	const char *upsert_sql =
		"INSERT INTO users (fingerprint) VALUES (?) "
		"ON CONFLICT(fingerprint) DO UPDATE SET fingerprint = excluded.fingerprint "
		"RETURNING user_id;";

	rc = sqlite3_prepare_v2(db, upsert_sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "users_lookup: prepare failed: %s\n", sqlite3_errmsg(db));
		sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
		return -1;
	}
	sqlite3_bind_text(stmt, 1, fingerprint, -1, SQLITE_STATIC);

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW) {
		fprintf(stderr, "users_lookup: upsert failed: %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
		return -1;
	}
	user_id = sqlite3_column_int(stmt, 0);
	sqlite3_finalize(stmt);

	rc = sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "users_lookup: commit failed: %s\n", sqlite3_errmsg(db));
		sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
		return -1;
	}
	return user_id;
}