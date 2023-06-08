#include <stdio.h>
#include "esp_log.h"
#include "iot_button.h"

#define TAG "BOARD"

extern void btn_click_a();

static void board_button_init(void)
{
    iot_button_set_evt_cb(iot_button_create(0, 0), BUTTON_CB_RELEASE, btn_click_a, "RELEASE");
}

void board_init(void)
{
    board_button_init();
}