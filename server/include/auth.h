#ifndef AUTH_H
#define AUTH_H

typedef enum { AUTH_NONE, AUTH_USER, AUTH_ADMIN } auth_role_t;

typedef struct {
    auth_role_t role;
    int         user_id;
} auth_t;

auth_t auth_parse(const char *bearer_token, const char *admin_token);

#endif /* AUTH_H */
