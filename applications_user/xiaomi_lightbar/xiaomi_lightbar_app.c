#include "nrf24_driver.h"

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <string.h>

#define XIAOMI_LIGHTBAR_PACKET_SIZE 17U
#define XIAOMI_LIGHTBAR_CHANNEL_COUNT 5U

typedef enum {
    XiaomiCmdPowerToggle = 0x0101,
    XiaomiCmdBrightnessUp = 0x0102,
    XiaomiCmdBrightnessDown = 0x0103,
    XiaomiCmdWarmer = 0x0201,
    XiaomiCmdColder = 0x0202,
} XiaomiCmd;

typedef struct {
    FuriMessageQueue* input_queue;
    uint8_t remote_id[3];
    uint8_t packet_counter;
    uint8_t hop_index;
} XiaomiLightbarApp;

static const uint8_t xiaomi_preamble[8] = {0x53, 0x39, 0x14, 0xDD, 0x1C, 0x49, 0x34, 0x12};
static const uint8_t xiaomi_channels[XIAOMI_LIGHTBAR_CHANNEL_COUNT] = {6, 12, 24, 38, 62};

static uint16_t xiaomi_crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for(size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for(uint8_t bit = 0; bit < 8; bit++) {
            if(crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

static void xiaomi_build_packet(XiaomiLightbarApp* app, XiaomiCmd cmd, uint8_t packet[XIAOMI_LIGHTBAR_PACKET_SIZE]) {
    memset(packet, 0, XIAOMI_LIGHTBAR_PACKET_SIZE);
    memcpy(packet, xiaomi_preamble, sizeof(xiaomi_preamble));
    memcpy(packet + 8, app->remote_id, sizeof(app->remote_id));

    packet[11] = (uint8_t)(cmd >> 8);
    packet[12] = (uint8_t)(cmd & 0xFF);
    packet[13] = app->packet_counter++;
    packet[14] = 0x00;

    uint16_t crc = xiaomi_crc16(packet, 15);
    packet[15] = (uint8_t)(crc >> 8);
    packet[16] = (uint8_t)(crc & 0xFF);
}

static bool xiaomi_send_command(XiaomiLightbarApp* app, XiaomiCmd cmd) {
    uint8_t packet[XIAOMI_LIGHTBAR_PACKET_SIZE];
    xiaomi_build_packet(app, cmd, packet);

    const uint8_t channel = xiaomi_channels[app->hop_index];
    app->hop_index = (app->hop_index + 1) % XIAOMI_LIGHTBAR_CHANNEL_COUNT;

    if(!nrf24_set_channel(channel)) return false;
    return nrf24_send_raw(packet, sizeof(packet));
}

static void xiaomi_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "Xiaomi Lightbar");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 27, "OK: Power");
    canvas_draw_str(canvas, 2, 39, "Up/Down: Brightness");
    canvas_draw_str(canvas, 2, 51, "Left/Right: Temp");
}

static void xiaomi_input_callback(InputEvent* input_event, void* context) {
    XiaomiLightbarApp* app = context;
    furi_check(app);
    furi_message_queue_put(app->input_queue, input_event, FuriWaitForever);
}

int32_t xiaomi_lightbar_app(void* p) {
    UNUSED(p);

    XiaomiLightbarApp app = {
        .input_queue = furi_message_queue_alloc(8, sizeof(InputEvent)),
        .remote_id = {0xD1, 0xA5, 0x7C},
        .packet_counter = 0,
        .hop_index = 0,
    };

    if(!app.input_queue) return -1;

    Nrf24Config config = {.rf_channel = xiaomi_channels[0]};
    const bool radio_ok = nrf24_begin(&config);

    Gui* gui = furi_record_open(RECORD_GUI);
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, xiaomi_draw_callback, &app);
    view_port_input_callback_set(view_port, xiaomi_input_callback, &app);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    bool running = true;
    while(running) {
        InputEvent event;
        if(furi_message_queue_get(app.input_queue, &event, FuriWaitForever) != FuriStatusOk) {
            continue;
        }
        if(event.type != InputTypeShort) continue;

        switch(event.key) {
        case InputKeyOk:
            if(radio_ok) xiaomi_send_command(&app, XiaomiCmdPowerToggle);
            break;
        case InputKeyUp:
            if(radio_ok) xiaomi_send_command(&app, XiaomiCmdBrightnessUp);
            break;
        case InputKeyDown:
            if(radio_ok) xiaomi_send_command(&app, XiaomiCmdBrightnessDown);
            break;
        case InputKeyLeft:
            if(radio_ok) xiaomi_send_command(&app, XiaomiCmdWarmer);
            break;
        case InputKeyRight:
            if(radio_ok) xiaomi_send_command(&app, XiaomiCmdColder);
            break;
        case InputKeyBack:
            running = false;
            break;
        default:
            break;
        }
    }

    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    nrf24_end();
    furi_message_queue_free(app.input_queue);
    return 0;
}
