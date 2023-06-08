/* main.c - Application main entry point */

/*
 * Copyright (c) 2017 Intel Corporation
 * Additional Copyright (c) 2018 Espressif Systems (Shanghai) PTE LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_generic_model_api.h"

#include "board.h"
#include "ble_mesh_example_init.h"
#include "ble_mesh_example_nvs.h"

#include "coap.h"

#define BUFSIZE 40

#define TAG "EXAMPLE"

#define CID_ESP 0x02E5

#define MSG_SEND_TTL 3
#define MSG_SEND_REL false
#define MSG_TIMEOUT 4000
#define MSG_ROLE ROLE_NODE

#define ESP_BLE_MESH_VND_MODEL_ID_CLIENT 0x0000
#define ESP_BLE_MESH_VND_MODEL_ID_SERVER 0x0001

#define OP_REQ ESP_BLE_MESH_MODEL_OP_3(0x01, CID_ESP)
#define OP_RES ESP_BLE_MESH_MODEL_OP_3(0x02, CID_ESP)

static coap_optlist_t *optlist = NULL;
uint16_t tx_mid = 0;

static uint8_t dev_uuid[16] = {0xdd, 0xdd};

static struct example_info_store
{
    uint16_t net_idx; /* NetKey Index */
    uint16_t app_idx; /* AppKey Index */
    uint8_t onoff;    /* Remote OnOff */
    uint16_t tid;     /* Message TID */
} __attribute__((packed)) store = {
    .net_idx = ESP_BLE_MESH_KEY_UNUSED,
    .app_idx = ESP_BLE_MESH_KEY_UNUSED,
    .onoff = 0x0,
    .tid = 0x0,
};

static nvs_handle_t NVS_HANDLE;
static const char *NVS_KEY = "onoff_client";

static esp_ble_mesh_cfg_srv_t config_server = {
    .relay = ESP_BLE_MESH_RELAY_ENABLED,
    .beacon = ESP_BLE_MESH_BEACON_DISABLED,
#if defined(CONFIG_BLE_MESH_FRIEND)
    .friend_state = ESP_BLE_MESH_FRIEND_ENABLED,
#else
    .friend_state = ESP_BLE_MESH_FRIEND_NOT_SUPPORTED,
#endif
#if defined(CONFIG_BLE_MESH_GATT_PROXY_SERVER)
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_ENABLED,
#else
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_NOT_SUPPORTED,
#endif
    .default_ttl = 7,
    /* 3 transmissions with 20ms interval */
    .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20),
};

static const esp_ble_mesh_client_op_pair_t vnd_op_pair[] = {};

static esp_ble_mesh_client_t vendor_client = {
    .op_pair_size = ARRAY_SIZE(vnd_op_pair),
    .op_pair = vnd_op_pair,
};

static esp_ble_mesh_model_op_t vnd_op[] = {
    ESP_BLE_MESH_MODEL_OP(OP_REQ, 2),
    ESP_BLE_MESH_MODEL_OP(OP_RES, 2),
    ESP_BLE_MESH_MODEL_OP_END,
};

static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
};

static esp_ble_mesh_model_t vnd_models[] = {
    ESP_BLE_MESH_VENDOR_MODEL(CID_ESP, ESP_BLE_MESH_VND_MODEL_ID_CLIENT,
                              vnd_op, NULL, &vendor_client),
};

static esp_ble_mesh_elem_t elements[] = {
    ESP_BLE_MESH_ELEMENT(0, root_models, vnd_models),
};

static esp_ble_mesh_comp_t composition = {
    .cid = CID_ESP,
    .elements = elements,
    .element_count = ARRAY_SIZE(elements),
};

/* Disable OOB security for SILabs Android app */
static esp_ble_mesh_prov_t provision = {
    .uuid = dev_uuid,
#if 0
    .output_size = 4,
    .output_actions = ESP_BLE_MESH_DISPLAY_NUMBER,
    .input_actions = ESP_BLE_MESH_PUSH,
    .input_size = 4,
#else
    .output_size = 0,
    .output_actions = 0,
#endif
};

static void mesh_example_info_store(void)
{
    ble_mesh_nvs_store(NVS_HANDLE, NVS_KEY, &store, sizeof(store));
}

static void mesh_example_info_restore(void)
{
    esp_err_t err = ESP_OK;
    bool exist = false;

    err = ble_mesh_nvs_restore(NVS_HANDLE, NVS_KEY, &store, sizeof(store), &exist);
    if (err != ESP_OK)
    {
        return;
    }

    if (exist)
    {
        ESP_LOGI(TAG, "Restore, net_idx 0x%04x, app_idx 0x%04x, onoff %u, tid 0x%04x",
                 store.net_idx, store.app_idx, store.onoff, store.tid);
    }
}

uint16_t myaddr = 0x0001;

static void prov_complete(uint16_t net_idx, uint16_t addr, uint8_t flags, uint32_t iv_index)
{
    ESP_LOGI(TAG, "net_idx: 0x%04x, addr: 0x%04x", net_idx, addr);
    ESP_LOGI(TAG, "flags: 0x%02x, iv_index: 0x%08x", flags, iv_index);
    myaddr = addr;
    store.net_idx = net_idx;
    /* mesh_example_info_store() shall not be invoked here, because if the device
     * is restarted and goes into a provisioned state, then the following events
     * will come:
     * 1st: ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT
     * 2nd: ESP_BLE_MESH_PROV_REGISTER_COMP_EVT
     * So the store.net_idx will be updated here, and if we store the mesh example
     * info here, the wrong app_idx (initialized with 0xFFFF) will be stored in nvs
     * just before restoring it.
     */
}

static void example_ble_mesh_provisioning_cb(esp_ble_mesh_prov_cb_event_t event,
                                             esp_ble_mesh_prov_cb_param_t *param)
{
    switch (event)
    {
    case ESP_BLE_MESH_PROV_REGISTER_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_PROV_REGISTER_COMP_EVT, err_code %d", param->prov_register_comp.err_code);
        mesh_example_info_restore(); /* Restore proper mesh example info */
        break;
    case ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT, err_code %d", param->node_prov_enable_comp.err_code);
        break;
    case ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT, bearer %s",
                 param->node_prov_link_open.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
        break;
    case ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT, bearer %s",
                 param->node_prov_link_close.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
        break;
    case ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT");
        prov_complete(param->node_prov_complete.net_idx, param->node_prov_complete.addr,
                      param->node_prov_complete.flags, param->node_prov_complete.iv_index);
        break;
    case ESP_BLE_MESH_NODE_PROV_RESET_EVT:
        break;
    case ESP_BLE_MESH_NODE_SET_UNPROV_DEV_NAME_COMP_EVT:
        ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_SET_UNPROV_DEV_NAME_COMP_EVT, err_code %d", param->node_set_unprov_dev_name_comp.err_code);
        break;
    default:
        break;
    }
}

static void example_ble_mesh_config_server_cb(esp_ble_mesh_cfg_server_cb_event_t event,
                                              esp_ble_mesh_cfg_server_cb_param_t *param)
{
    if (event == ESP_BLE_MESH_CFG_SERVER_STATE_CHANGE_EVT)
    {
        switch (param->ctx.recv_op)
        {
        case ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD:
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD");
            ESP_LOGI(TAG, "net_idx 0x%04x, app_idx 0x%04x",
                     param->value.state_change.appkey_add.net_idx,
                     param->value.state_change.appkey_add.app_idx);
            ESP_LOG_BUFFER_HEX("AppKey", param->value.state_change.appkey_add.app_key, 16);
            break;
        case ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND:
            ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND");
            ESP_LOGI(TAG, "elem_addr 0x%04x, app_idx 0x%04x, cid 0x%04x, mod_id 0x%04x",
                     param->value.state_change.mod_app_bind.element_addr,
                     param->value.state_change.mod_app_bind.app_idx,
                     param->value.state_change.mod_app_bind.company_id,
                     param->value.state_change.mod_app_bind.model_id);
            if (param->value.state_change.mod_app_bind.company_id == CID_ESP &&
                param->value.state_change.mod_app_bind.model_id == ESP_BLE_MESH_VND_MODEL_ID_CLIENT)
            {
                store.app_idx = param->value.state_change.mod_app_bind.app_idx;
                mesh_example_info_store(); /* Store proper mesh example info */
            }
            break;
        default:
            break;
        }
    }
}

uint8_t action = 0;
uint16_t count = 0;
uint8_t retry = 0;

esp_ble_mesh_msg_ctx_t ctx = {0};
uint32_t opcode;
esp_err_t err;
static coap_uri_t uri;
const char *server_uri = "coap://localhost/h";

unsigned char _buf[BUFSIZE];
unsigned char *buf;
size_t buflen;
int res;
coap_pdu_t *request = NULL;
esp_timer_handle_t timer;

void btn_click_a()
{
    if (action == 2 || action == 0)
    {
        // 比佢入
        if (action == 0)
        {
            count = 0;
        }
    }
    else
    {
        ESP_LOGE(TAG, "wait action complete!!!");
        return;
    }
    action = 2;

    coap_set_log_level(CONFIG_COAP_LOG_DEFAULT_LEVEL);

    optlist = NULL;

    if (coap_split_uri((const uint8_t *)server_uri, strlen(server_uri), &uri) == -1)
    {
        ESP_LOGE(TAG, "CoAP server uri error");
        return;
    }

    if (uri.path.length)
    {
        buflen = BUFSIZE;
        buf = _buf;
        res = coap_split_path(uri.path.s, uri.path.length, buf, &buflen);

        while (res--)
        {
            coap_insert_optlist(&optlist,
                                coap_new_optlist(COAP_OPTION_URI_PATH,
                                                 coap_opt_length(buf),
                                                 coap_opt_value(buf)));

            buf += coap_opt_size(buf);
        }
    }

    if (uri.query.length)
    {
        buflen = BUFSIZE;
        buf = _buf;
        res = coap_split_query(uri.query.s, uri.query.length, buf, &buflen);

        while (res--)
        {
            coap_insert_optlist(&optlist,
                                coap_new_optlist(COAP_OPTION_URI_QUERY,
                                                 coap_opt_length(buf),
                                                 coap_opt_value(buf)));

            buf += coap_opt_size(buf);
        }
    }

    if (tx_mid == 0)
    {
        prng((unsigned char *)&tx_mid, sizeof(tx_mid));
    }

    request = coap_pdu_init(0, 0, 0, 1152 - 4);
    request->type = COAP_MESSAGE_CON;
    request->tid = ++tx_mid;
    request->code = COAP_REQUEST_GET;

    // uint8_t token[1];
    // prng(token, 1);
    uint8_t token[4];
    count++;
    token[0] = (uint8_t)(myaddr >> 8);
    token[1] = (uint8_t)(myaddr);
    token[2] = (uint8_t)(count >> 8);
    token[3] = (uint8_t)(count);
    coap_add_token(request, 4, token);

    coap_add_optlist_pdu(request, &optlist);

    // uint8_t payload[8];
    // payload[0] = (uint8_t)(count >> 8);
    // payload[1] = (uint8_t)(count);
    // coap_add_data(request, 2, payload);

    coap_pdu_encode_header(request, COAP_PROTO_UDP);

    bool need_rsp = false;

    // 準備 msg
    ctx.net_idx = store.net_idx;
    ctx.app_idx = store.app_idx;
    ctx.addr = 0xffff;
    ctx.send_ttl = MSG_SEND_TTL;
    ctx.send_rel = MSG_SEND_REL;
    opcode = OP_REQ;

    ESP_LOGI("[!]", ",req,%d,", count);

    err = esp_ble_mesh_client_model_send_msg(vendor_client.model, &ctx, opcode,
                                             request->used_size + request->hdr_size, request->token - request->hdr_size,
                                             MSG_TIMEOUT, need_rsp, MSG_ROLE);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to send vendor message 0x%06x", opcode);
    }

    if (optlist)
    {
        coap_delete_optlist(optlist);
        optlist = NULL;
    }
    if (request)
    {
        coap_delete_pdu(request);
        request = NULL;
    }
    coap_cleanup();

    // ESP_LOGI("coap", "send\n\n");

    esp_timer_stop(timer);
    ESP_ERROR_CHECK(esp_timer_start_once(timer, 3000000));
}

static void timer_callback(void *arg)
{
    ESP_LOGI(TAG, "\n\n!!!!!!!!!!!!!! timeout timer called !!!!!!!!!!!!!\n\n");
    if (action == 2)
    {
        count--;
        retry++;
        btn_click_a();
    }
}

static void ble_mesh_custom_model_cb(esp_ble_mesh_model_cb_event_t event,
                                     esp_ble_mesh_model_cb_param_t *param)
{
    switch (event)
    {
    case ESP_BLE_MESH_MODEL_OPERATION_EVT:
        ESP_LOGI(TAG, "Recv 0x%06x, tid 0x%04x", param->model_operation.opcode, store.tid);
        break;
    case ESP_BLE_MESH_MODEL_SEND_COMP_EVT:
        if (param->model_send_comp.err_code)
        {
            ESP_LOGE(TAG, "Failed to send message 0x%06x", param->model_send_comp.opcode);
            break;
        }
        ESP_LOGI(TAG, "Send 0x%06x", param->model_send_comp.opcode);
        break;
    case ESP_BLE_MESH_CLIENT_MODEL_RECV_PUBLISH_MSG_EVT:;
        ESP_LOGI(TAG, "Receive publish message 0x%06x", param->client_recv_publish_msg.opcode);
        ESP_LOG_BUFFER_HEX("recv data", param->client_recv_publish_msg.msg, param->client_recv_publish_msg.length);

        if (param->client_recv_publish_msg.opcode == OP_RES)
        {
            if (action == 2)
            {
                uint8_t *msg = param->client_recv_publish_msg.msg;
                uint8_t count2[2];
                count2[1] = *(msg + 6);
                count2[0] = *(msg + 7);
                uint16_t *c = count2;
                if (count == *c)
                {
                    ESP_LOGI("[!]", ",res,%d,%d,%d,%d,",
                             *c,
                             param->client_recv_publish_msg.length,
                             param->client_recv_publish_msg.ctx->recv_ttl,
                             param->client_recv_publish_msg.ctx->recv_rssi);
                    esp_timer_stop(timer);
                    vTaskDelay(200 / portTICK_PERIOD_MS);
                    retry = 0;
                    btn_click_a();
                }
            }
        }

        break;
    case ESP_BLE_MESH_CLIENT_MODEL_SEND_TIMEOUT_EVT:
        ESP_LOGW(TAG, "Client message 0x%06x timeout", param->client_send_timeout.opcode);
        break;
    default:
        break;
    }
}

static esp_err_t ble_mesh_init(void)
{
    esp_err_t err = ESP_OK;

    esp_ble_mesh_register_prov_callback(example_ble_mesh_provisioning_cb);
    esp_ble_mesh_register_custom_model_callback(ble_mesh_custom_model_cb);
    esp_ble_mesh_register_config_server_callback(example_ble_mesh_config_server_cb);

    err = esp_ble_mesh_init(&provision, &composition);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize mesh stack (err %d)", err);
        return err;
    }

    err = esp_ble_mesh_client_model_init(&vnd_models[0]);
    if (err)
    {
        ESP_LOGE(TAG, "Failed to initialize vendor client");
        return err;
    }

    err = esp_ble_mesh_node_prov_enable(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to enable mesh node (err %d)", err);
        return err;
    }

    ESP_LOGI(TAG, "BLE Mesh Node initialized");

    return err;
}

void app_main(void)
{
    esp_err_t err;

    ESP_LOGI(TAG, "Initializing...");

    board_init();

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    err = bluetooth_init();
    if (err)
    {
        ESP_LOGE(TAG, "esp32_bluetooth_init failed (err %d)", err);
        return;
    }

    /* Open nvs namespace for storing/restoring mesh example info */
    err = ble_mesh_nvs_open(&NVS_HANDLE);
    if (err)
    {
        return;
    }

    ble_mesh_get_dev_uuid(dev_uuid);

    /* Initialize the Bluetooth Mesh Subsystem */
    err = ble_mesh_init();
    if (err)
    {
        ESP_LOGE(TAG, "Bluetooth mesh init failed (err %d)", err);
    }

    const esp_timer_create_args_t timer_args = {
        .callback = &timer_callback,
        /* name is optional, but may help identify the timer when debugging */
        .name = "periodic"};
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
}
