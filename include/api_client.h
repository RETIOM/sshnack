#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <curl/curl.h>

typedef struct {
    int  slot_id;
    int  item_id;
    int  price_gr;
    int  stock_qty;
    char name[64];
} product_t;

typedef enum {
    API_OK = 0,
    API_ERR_NETWORK,
    API_ERR_HTTP,
    API_ERR_PARSE
} api_status_t;

typedef struct {
    char  base_url[256];
    int   user_id;
    char  admin_token[128];
    CURL *curl;
} api_client_t;

api_status_t api_client_init(api_client_t *c, const char *base_url, int user_id, const char *admin_token);
void         api_client_cleanup(api_client_t *c);

/* customer calls — Authorization: Bearer <user_id> */
api_status_t api_get_stock(api_client_t *c, product_t **out, int *count); /* caller frees *out */
api_status_t api_get_balance(api_client_t *c, int *balance_gr);
api_status_t api_deposit(api_client_t *c, int amount_gr);
api_status_t api_purchase(api_client_t *c, int slot_id);
api_status_t api_refund(api_client_t *c, int *refund_gr);

/* admin calls — Authorization: Bearer <admin_token> */
api_status_t api_restock(api_client_t *c, int slot_id, int qty);
api_status_t api_set_price(api_client_t *c, int item_id, int price_gr);

#endif /* API_CLIENT_H */
