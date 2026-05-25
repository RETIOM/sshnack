#ifndef UI_H
#define UI_H

#include "app.h"

void ui_init(void);
void ui_teardown(void);
void ui_render(const app_t *a);
int  ui_get_key(void);

#endif /* UI_H */
