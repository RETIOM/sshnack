#include "auth.h"
#include <stdlib.h>
#include <string.h>

auth_t auth_parse(const char *bearer_token, const char *admin_token) {
    auth_t auth = { AUTH_NONE, 0 };

    if (!bearer_token || bearer_token[0] == '\0') return auth;

    if (strcmp(bearer_token, admin_token) == 0) {
        auth.role = AUTH_ADMIN;
        return auth;
    }

    char *end;
    long id = strtol(bearer_token, &end, 10);
    if (*end == '\0' && id > 0) {
        auth.role = AUTH_USER;
        auth.user_id = (int)id;
    }

    return auth;
}
