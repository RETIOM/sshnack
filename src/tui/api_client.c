#include "api_client.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    char  *data;
    size_t len;
} buf_t;

static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t add = size * nmemb;
    buf_t *b = (buf_t *)userdata;
    char *p = realloc(b->data, b->len + add + 1);
    if (!p) return 0;
    b->data = p;
    memcpy(b->data + b->len, ptr, add);
    b->len += add;
    b->data[b->len] = '\0';
    return add;
}

static api_status_t do_request(api_client_t *c, const char *method, const char *path,
                               const char *body, int use_admin,
                               buf_t *resp, long *http_status) {
    char url[512];
    snprintf(url, sizeof(url), "%s%s", c->base_url, path);

    resp->data = NULL;
    resp->len  = 0;

    curl_easy_reset(c->curl);
    curl_easy_setopt(c->curl, CURLOPT_URL, url);
    curl_easy_setopt(c->curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(c->curl, CURLOPT_WRITEDATA, resp);
    curl_easy_setopt(c->curl, CURLOPT_TIMEOUT, 5L);

    if (strcmp(method, "GET") == 0) {
        curl_easy_setopt(c->curl, CURLOPT_HTTPGET, 1L);
    } else if (strcmp(method, "POST") == 0) {
        curl_easy_setopt(c->curl, CURLOPT_POST, 1L);
        curl_easy_setopt(c->curl, CURLOPT_POSTFIELDS, body ? body : "");
    } else {
        curl_easy_setopt(c->curl, CURLOPT_CUSTOMREQUEST, method);
        if (body) curl_easy_setopt(c->curl, CURLOPT_POSTFIELDS, body);
    }

    struct curl_slist *hdrs = NULL;
    char auth[200];
    if (use_admin)
        snprintf(auth, sizeof(auth), "Authorization: Bearer %s", c->admin_token);
    else
        snprintf(auth, sizeof(auth), "Authorization: Bearer %d", c->user_id);
    hdrs = curl_slist_append(hdrs, auth);
    if (body) hdrs = curl_slist_append(hdrs, "Content-Type: application/json");
    curl_easy_setopt(c->curl, CURLOPT_HTTPHEADER, hdrs);

    CURLcode rc = curl_easy_perform(c->curl);
    curl_slist_free_all(hdrs);

    if (rc != CURLE_OK) {
        free(resp->data);
        resp->data = NULL;
        resp->len  = 0;
        return API_ERR_NETWORK;
    }

    long status = 0;
    curl_easy_getinfo(c->curl, CURLINFO_RESPONSE_CODE, &status);
    if (http_status) *http_status = status;
    return API_OK;
}

api_status_t api_client_init(api_client_t *c, const char *base_url, int user_id, const char *admin_token) {
    strncpy(c->base_url, base_url, sizeof(c->base_url) - 1);
    c->base_url[sizeof(c->base_url) - 1] = '\0';
    c->user_id = user_id;
    strncpy(c->admin_token, admin_token, sizeof(c->admin_token) - 1);
    c->admin_token[sizeof(c->admin_token) - 1] = '\0';
    c->curl = curl_easy_init();
    return c->curl ? API_OK : API_ERR_NETWORK;
}

void api_client_cleanup(api_client_t *c) {
    if (c->curl) {
        curl_easy_cleanup(c->curl);
        c->curl = NULL;
    }
}

api_status_t api_get_stock(api_client_t *c, product_t **out, int *count) {
    buf_t resp;
    long status;
    api_status_t st = do_request(c, "GET", "/stock", NULL, 0, &resp, &status);
    if (st != API_OK) return st;
    if (status >= 400) { free(resp.data); return API_ERR_HTTP; }

    cJSON *root = cJSON_Parse(resp.data ? resp.data : "");
    free(resp.data);
    if (!root || !cJSON_IsArray(root)) { cJSON_Delete(root); return API_ERR_PARSE; }

    int n = cJSON_GetArraySize(root);
    product_t *arr = calloc(n > 0 ? (size_t)n : 1, sizeof(product_t));
    if (!arr) { cJSON_Delete(root); return API_ERR_PARSE; }

    int i = 0;
    cJSON *el;
    cJSON_ArrayForEach(el, root) {
        cJSON *slot  = cJSON_GetObjectItem(el, "slot_id");
        cJSON *item  = cJSON_GetObjectItem(el, "item_id");
        cJSON *name  = cJSON_GetObjectItem(el, "name");
        cJSON *price = cJSON_GetObjectItem(el, "price_gr");
        cJSON *qty   = cJSON_GetObjectItem(el, "stock_qty");
        arr[i].slot_id   = cJSON_IsNumber(slot)  ? slot->valueint  : 0;
        arr[i].item_id   = cJSON_IsNumber(item)  ? item->valueint  : 0;
        arr[i].price_gr  = cJSON_IsNumber(price) ? price->valueint : 0;
        arr[i].stock_qty = cJSON_IsNumber(qty)   ? qty->valueint   : 0;
        if (cJSON_IsString(name) && name->valuestring) {
            strncpy(arr[i].name, name->valuestring, sizeof(arr[i].name) - 1);
            arr[i].name[sizeof(arr[i].name) - 1] = '\0';
        }
        i++;
    }
    cJSON_Delete(root);
    *out   = arr;
    *count = i;
    return API_OK;
}

api_status_t api_get_balance(api_client_t *c, int *balance_gr) {
    buf_t resp;
    long status;
    api_status_t st = do_request(c, "GET", "/balance", NULL, 0, &resp, &status);
    if (st != API_OK) return st;
    if (status >= 400) { free(resp.data); return API_ERR_HTTP; }

    cJSON *root = cJSON_Parse(resp.data ? resp.data : "");
    free(resp.data);
    if (!root) { cJSON_Delete(root); return API_ERR_PARSE; }
    cJSON *b = cJSON_GetObjectItem(root, "balance_gr");
    if (!cJSON_IsNumber(b)) { cJSON_Delete(root); return API_ERR_PARSE; }
    *balance_gr = b->valueint;
    cJSON_Delete(root);
    return API_OK;
}

api_status_t api_deposit(api_client_t *c, int amount_gr) {
    char body[64];
    snprintf(body, sizeof(body), "{\"amount_gr\":%d}", amount_gr);
    buf_t resp;
    long status;
    api_status_t st = do_request(c, "POST", "/balance", body, 0, &resp, &status);
    if (st != API_OK) return st;
    free(resp.data);
    return status >= 400 ? API_ERR_HTTP : API_OK;
}

api_status_t api_purchase(api_client_t *c, int slot_id) {
    char body[64];
    snprintf(body, sizeof(body), "{\"slot_id\":%d}", slot_id);
    buf_t resp;
    long status;
    api_status_t st = do_request(c, "POST", "/orders", body, 0, &resp, &status);
    if (st != API_OK) return st;
    free(resp.data);
    return status >= 400 ? API_ERR_HTTP : API_OK;
}

api_status_t api_refund(api_client_t *c, int *refund_gr) {
    buf_t resp;
    long status;
    api_status_t st = do_request(c, "DELETE", "/balance", NULL, 0, &resp, &status);
    if (st != API_OK) return st;
    if (status >= 400) { free(resp.data); return API_ERR_HTTP; }

    cJSON *root = cJSON_Parse(resp.data ? resp.data : "");
    free(resp.data);
    if (!root) return API_ERR_PARSE;
    cJSON *r = cJSON_GetObjectItem(root, "refund_gr");
    if (!cJSON_IsNumber(r)) { cJSON_Delete(root); return API_ERR_PARSE; }
    *refund_gr = r->valueint;
    cJSON_Delete(root);
    return API_OK;
}

api_status_t api_restock(api_client_t *c, int slot_id, int qty) {
    char path[64], body[64];
    snprintf(path, sizeof(path), "/stock/%d", slot_id);
    snprintf(body, sizeof(body), "{\"qty\":%d}", qty);
    buf_t resp;
    long status;
    api_status_t st = do_request(c, "PATCH", path, body, 1, &resp, &status);
    if (st != API_OK) return st;
    free(resp.data);
    return status >= 400 ? API_ERR_HTTP : API_OK;
}

api_status_t api_set_price(api_client_t *c, int item_id, int price_gr) {
    char path[64], body[64];
    snprintf(path, sizeof(path), "/items/%d", item_id);
    snprintf(body, sizeof(body), "{\"price_gr\":%d}", price_gr);
    buf_t resp;
    long status;
    api_status_t st = do_request(c, "PATCH", path, body, 1, &resp, &status);
    if (st != API_OK) return st;
    free(resp.data);
    return status >= 400 ? API_ERR_HTTP : API_OK;
}
