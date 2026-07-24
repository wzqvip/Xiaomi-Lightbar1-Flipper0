#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include "nrf24.h"
#include "xiaomi_protocol.h"

#define CONFIG_FILE_PATH EXT_PATH("apps_data/xiaomi_lightbar/config.bin")

typedef enum {
    ViewControl,
    ViewMenu,
    ViewEditID,
    ViewEditPA,
} AppView;

typedef struct {
    uint32_t remote_id;
    uint8_t counter;
    bool nrf_connected;
    Nrf24PaLevel pa_level;
    AppView current_view;

    // Menu state
    int8_t menu_index;

    // Edit ID state
    uint8_t edit_id_bytes[3];
    uint8_t edit_id_cursor; // 0, 1, 2

    // Edit PA state
    int8_t edit_pa_index;

    FuriMutex* mutex;
} AppState;

static void load_settings(AppState* state) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    
    if (storage_file_open(file, CONFIG_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint32_t loaded_id = 0;
        uint8_t loaded_pa = 0;
        
        if (storage_file_read(file, &loaded_id, sizeof(loaded_id)) == sizeof(loaded_id)) {
            state->remote_id = loaded_id;
        }
        if (storage_file_read(file, &loaded_pa, sizeof(loaded_pa)) == sizeof(loaded_pa)) {
            state->pa_level = (Nrf24PaLevel)loaded_pa;
            state->edit_pa_index = (int8_t)state->pa_level;
        }
    }
    
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void save_settings(AppState* state) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, EXT_PATH("apps_data"));
    storage_simply_mkdir(storage, EXT_PATH("apps_data/xiaomi_lightbar"));
    
    File* file = storage_file_alloc(storage);
    
    if (storage_file_open(file, CONFIG_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint32_t id_to_save = state->remote_id;
        uint8_t pa_to_save = (uint8_t)state->pa_level;
        
        storage_file_write(file, &id_to_save, sizeof(id_to_save));
        storage_file_write(file, &pa_to_save, sizeof(pa_to_save));
    }
    
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static const char* pa_level_to_str(Nrf24PaLevel level) {
    switch(level) {
        case NRF24_PA_MIN: return "MIN";
        case NRF24_PA_LOW: return "LOW";
        case NRF24_PA_HIGH: return "HIGH";
        case NRF24_PA_MAX: return "MAX";
        default: return "UNKNOWN";
    }
}

static void draw_control_view(Canvas* canvas, AppState* state) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "Xiaomi Lightbar Control");
    canvas_draw_line(canvas, 0, 15, 128, 15);

    canvas_set_font(canvas, FontSecondary);
    if (state->nrf_connected) {
        canvas_draw_str(canvas, 2, 26, "nRF24: CONNECTED");
    } else {
        canvas_draw_str(canvas, 2, 26, "nRF24: DISCONNECTED!");
    }

    char id_str[32];
    snprintf(id_str, sizeof(id_str), "Remote ID: 0x%06lX", state->remote_id);
    canvas_draw_str(canvas, 2, 37, id_str);

    char pa_str[32];
    snprintf(pa_str, sizeof(pa_str), "PA Level: %s", pa_level_to_str(state->pa_level));
    canvas_draw_str(canvas, 2, 48, pa_str);

    canvas_draw_str(canvas, 2, 59, "[BACK] Settings Menu");
}

static void draw_menu_view(Canvas* canvas, AppState* state) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "Settings Menu");
    canvas_draw_line(canvas, 0, 15, 128, 15);

    canvas_set_font(canvas, FontSecondary);
    const char* menu_items[] = {
        "Pair Lightbar (Send Reset)",
        "Set Remote ID",
        "Set PA Level",
        "Back to Remote",
        "Exit Application"
    };

    for (int i = 0; i < 5; i++) {
        int y = 26 + (i * 9);
        if (i == state->menu_index) {
            canvas_draw_str(canvas, 2, y, "> ");
            canvas_draw_str(canvas, 10, y, menu_items[i]);
        } else {
            canvas_draw_str(canvas, 10, y, menu_items[i]);
        }
    }
}

static void draw_edit_id_view(Canvas* canvas, AppState* state) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "Set Remote ID");
    canvas_draw_line(canvas, 0, 15, 128, 15);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 26, "Edit bytes (HEX):");

    for (int i = 0; i < 3; i++) {
        char byte_str[8];
        snprintf(byte_str, sizeof(byte_str), "%02X", state->edit_id_bytes[i]);
        int x = 20 + (i * 30);
        int y = 44;
        
        canvas_draw_str(canvas, x, y, byte_str);
        if (i == state->edit_id_cursor) {
            // Draw select underline
            canvas_draw_line(canvas, x - 2, y + 2, x + 14, y + 2);
            canvas_draw_line(canvas, x - 2, y + 3, x + 14, y + 3);
        }
    }

    canvas_draw_str(canvas, 2, 56, "Up/Dn: Val | Lf/Rt: Move");
    canvas_draw_str(canvas, 2, 64, "OK: Save | BACK: Cancel");
}

static void draw_edit_pa_view(Canvas* canvas, AppState* state) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "Set PA Level");
    canvas_draw_line(canvas, 0, 15, 128, 15);

    canvas_set_font(canvas, FontSecondary);
    const char* pa_options[] = { "MIN", "LOW", "HIGH", "MAX" };

    for (int i = 0; i < 4; i++) {
        int y = 26 + (i * 9);
        if (i == state->edit_pa_index) {
            canvas_draw_str(canvas, 2, y, "> ");
            canvas_draw_str(canvas, 10, y, pa_options[i]);
        } else {
            canvas_draw_str(canvas, 10, y, pa_options[i]);
        }
    }
}

static void draw_callback(Canvas* canvas, void* context) {
    AppState* state = context;
    furi_mutex_acquire(state->mutex, FuriWaitForever);

    canvas_clear(canvas);

    switch(state->current_view) {
        case ViewControl:
            draw_control_view(canvas, state);
            break;
        case ViewMenu:
            draw_menu_view(canvas, state);
            break;
        case ViewEditID:
            draw_edit_id_view(canvas, state);
            break;
        case ViewEditPA:
            draw_edit_pa_view(canvas, state);
            break;
    }

    furi_mutex_release(state->mutex);
}

static void input_callback(InputEvent* input_event, void* context) {
    FuriMessageQueue* event_queue = context;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

int32_t xiaomi_lightbar_app(void* p) {
    (void)p;

    AppState state;
    state.remote_id = 0x123456;
    state.counter = 0;
    state.nrf_connected = false;
    state.pa_level = NRF24_PA_MAX;
    state.current_view = ViewControl;
    state.menu_index = 0;
    state.edit_id_cursor = 0;
    state.edit_pa_index = 3; // MAX
    state.mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    load_settings(&state);

    // Initialize SPI and nRF24 module
    nrf24_init();
    nrf24_power_up();
    nrf24_set_data_rate(NRF24_DATA_RATE_2M);
    nrf24_set_pa_level(state.pa_level);
    nrf24_set_auto_ack(false);
    nrf24_set_dynamic_payloads(false);
    nrf24_set_dynamic_ack(true);
    nrf24_tx_mode();
    
    // Address width setup: 5 bytes
    nrf24_write_reg(SETUP_AW, 0x03); 
    
    // Tx address setup: 0x5555555555
    uint8_t addr[] = {0x55, 0x55, 0x55, 0x55, 0x55};
    nrf24_set_tx_address(addr, 5);

    state.nrf_connected = nrf24_check_connected();

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, &state);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    InputEvent event;
    bool running = true;

    while(running) {
        if(furi_message_queue_get(event_queue, &event, FuriWaitForever) == FuriStatusOk) {
            if(event.type == InputTypeShort || event.type == InputTypeLong || event.type == InputTypeRepeat) {
                uint32_t rid = 0;
                uint16_t cmd = 0;
                bool send = false;

                furi_mutex_acquire(state.mutex, FuriWaitForever);
                
                if (event.key == InputKeyBack && event.type == InputTypeLong) {
                    running = false;
                } else if (state.current_view == ViewControl) {
                    if (event.key == InputKeyBack && event.type == InputTypeShort) {
                        // Enter menu
                        state.menu_index = 0;
                        state.current_view = ViewMenu;
                    } else if (event.key == InputKeyOk) {
                        rid = state.remote_id;
                        cmd = CMD_ON_OFF;
                        send = true;
                    } else if (event.key == InputKeyUp) {
                        rid = state.remote_id;
                        cmd = cmd_stronger(1);
                        send = true;
                    } else if (event.key == InputKeyDown) {
                        rid = state.remote_id;
                        cmd = cmd_softer(1);
                        send = true;
                    } else if (event.key == InputKeyLeft) {
                        rid = state.remote_id;
                        cmd = cmd_warmer(1);
                        send = true;
                    } else if (event.key == InputKeyRight) {
                        rid = state.remote_id;
                        cmd = cmd_cooler(1);
                        send = true;
                    }
                } 
                else if (state.current_view == ViewMenu) {
                    if (event.key == InputKeyBack && event.type == InputTypeShort) {
                        state.current_view = ViewControl;
                    } else if (event.key == InputKeyUp) {
                        state.menu_index = (state.menu_index - 1 + 5) % 5;
                    } else if (event.key == InputKeyDown) {
                        state.menu_index = (state.menu_index + 1) % 5;
                    } else if (event.key == InputKeyOk) {
                        if (state.menu_index == 0) {
                            // Pair Lightbar (Send Reset)
                            rid = state.remote_id;
                            cmd = CMD_RESET;
                            send = true;
                            state.current_view = ViewControl;
                        } else if (state.menu_index == 1) {
                            // Edit Remote ID view setup
                            state.edit_id_bytes[0] = (state.remote_id >> 16) & 0xFF;
                            state.edit_id_bytes[1] = (state.remote_id >> 8) & 0xFF;
                            state.edit_id_bytes[2] = state.remote_id & 0xFF;
                            state.edit_id_cursor = 0;
                            state.current_view = ViewEditID;
                        } else if (state.menu_index == 2) {
                            // Edit PA level
                            state.edit_pa_index = (int8_t)state.pa_level;
                            state.current_view = ViewEditPA;
                        } else if (state.menu_index == 3) {
                            // Back to Remote control screen
                            state.current_view = ViewControl;
                        } else if (state.menu_index == 4) {
                            // Exit App
                            running = false;
                        }
                    }
                } 
                else if (state.current_view == ViewEditID) {
                    if (event.key == InputKeyBack && event.type == InputTypeShort) {
                        state.current_view = ViewMenu;
                    } else if (event.key == InputKeyLeft) {
                        state.edit_id_cursor = (state.edit_id_cursor - 1 + 3) % 3;
                    } else if (event.key == InputKeyRight) {
                        state.edit_id_cursor = (state.edit_id_cursor + 1) % 3;
                    } else if (event.key == InputKeyUp) {
                        state.edit_id_bytes[state.edit_id_cursor]++;
                    } else if (event.key == InputKeyDown) {
                        state.edit_id_bytes[state.edit_id_cursor]--;
                    } else if (event.key == InputKeyOk) {
                        // Save ID
                        state.remote_id = ((uint32_t)state.edit_id_bytes[0] << 16) | 
                                          ((uint32_t)state.edit_id_bytes[1] << 8) | 
                                          state.edit_id_bytes[2];
                        state.current_view = ViewMenu;
                        save_settings(&state);
                    }
                } 
                else if (state.current_view == ViewEditPA) {
                    if (event.key == InputKeyBack && event.type == InputTypeShort) {
                        state.current_view = ViewMenu;
                    } else if (event.key == InputKeyUp) {
                        state.edit_pa_index = (state.edit_pa_index - 1 + 4) % 4;
                    } else if (event.key == InputKeyDown) {
                        state.edit_pa_index = (state.edit_pa_index + 1) % 4;
                    } else if (event.key == InputKeyOk) {
                        state.pa_level = (Nrf24PaLevel)state.edit_pa_index;
                        nrf24_set_pa_level(state.pa_level);
                        state.current_view = ViewMenu;
                        save_settings(&state);
                    }
                }

                furi_mutex_release(state.mutex);

                if (send) {
                    uint8_t cnt = 0;
                    furi_mutex_acquire(state.mutex, FuriWaitForever);
                    cnt = state.counter;
                    furi_mutex_release(state.mutex);

                    xiaomi_send_command(rid, cmd, &cnt);

                    furi_mutex_acquire(state.mutex, FuriWaitForever);
                    state.counter = cnt;
                    furi_mutex_release(state.mutex);
                }

                view_port_update(view_port);
            }
        }
    }

    // Clean up
    nrf24_deinit();
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_mutex_free(state.mutex);
    furi_record_close(RECORD_GUI);

    return 0;
}
