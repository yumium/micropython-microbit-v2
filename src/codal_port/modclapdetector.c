/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Damien P. George
 * Copyright (c) 2016 Joe Glancy
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "py/runtime.h"
#include "py/mphal.h"
#include "modmicrobit.h"
#include "tflite/main_functions.h"

// #include "tensorflow/lite/micro/all_ops_resolver.h"
// #include "tensorflow/lite/micro/micro_error_reporter.h"
// #include "tensorflow/lite/micro/micro_interpreter.h"
// #include "tensorflow/lite/schema/schema_generated.h"
// #include "tensorflow/lite/version.h"

// #include "../codal/libraries/codal-microbit-v2/inc/compat/mbed.h"

// #include "tflite/TensorFlowLite.h"
// #include "tflite/model.h"
// #include "tflite/main_functions.h"
// #include "tflite/hello_world.h"

// #include "tflite/mbed.h"


#define GET_PIXEL(x, y) microbit_hal_display_get_pixel(x, y)
#define SET_PIXEL(x, y, v) microbit_hal_display_set_pixel(x, y, v)
// Update based on browser to micro:bit pipeline
#define DEFAULT_MODEL_NAME "model.cpp"
// Should match input size for the ML model
#define EVENT_HISTORY_SIZE (75)

// So we know where to take events from in the circular array buffer
static uint8_t sound_event_history_index_start = 0;
// Index at which new events should be added
static uint8_t sound_event_history_index_end = 0;
// Indicates if the sound history array is full
static bool array_filled = false;
// Circular array buffer for sound events
static bool sound_event_history_array[EVENT_HISTORY_SIZE];

// Indicates if the ClapDetector is currently active and listening
static volatile bool listening = false;
// True if the ClapDetector found a match since last check
// Reset to false by was_detected function
static volatile bool detected = false;

STATIC void microphone_init(void) {
    microbit_hal_microphone_init();
    // mp_printf(&mp_plat_print, "Starting TFLite setup.");
    // setup();
    // mp_printf(&mp_plat_print, "Finished TFLite setup.");
}

void microbit_hal_clap_detector_callback(void) {
    if (listening) {
        // Initialize microphone
        microphone_init();
        // Work out the sound level.
        int sound = microbit_hal_microphone_get_level();
        bool ev = (sound >= 10);
        // For debugging
        mp_printf(&mp_plat_print, "%d -> %s\n", sound, ev ? "true" : "false");

        // Add sound event to the history.
        sound_event_history_array[sound_event_history_index_end++] = ev;
        
        if (array_filled) {
            // TODO: call clap detector ML module
            sound_event_history_index_start++;
        } else if (sound_event_history_index_start == sound_event_history_index_end) {
            array_filled = true;
        }
    }
}

STATIC void clapdetector_output_char(char c) {
    MP_PLAT_PRINT_STRN((char *)&c, 1);
}

// NOTE: string has to be plain ASCII
STATIC void clapdetector_print_rle(const char *s) {
    /* RLE encoding format (2 characters, [0] (first) and [1] (second)):
     * [0] the amount of times to output the specified character (max 127),
     *     bitwise or'ed with 0x80 (to set the last bit, ie: bit 7)
     * [1] the character to output
     */
    char reps;    // how many times to output the specified character
    while (*s != '\0') {
        if ((*s & 0x80) == 0x80) {
            // seventh bit set
            reps = *s ^ 0x80;
            s++;
            if (*s == '\0') {
                return;
            }
            while (reps--) {
                clapdetector_output_char(*s);
            }
        }
        else {
            clapdetector_output_char(*s);
        }
        s++;
    }
}

/* uncomment the following line if you want to build the clapdetector module with
 * the full comic instead of the micro one
 */
//#define clapdetector_COMIC_LARGE

STATIC void clapdetector(uint8_t interval_ms) {
    /* move all of the LEDs upwards (we can move them in other directions in the
     * future).
     * first, output the traditional XKCD comic (either in full or micro:size :)
     */
    /* micro comic (157 bytes):
    +-xkcd.com/353---------------------------------------------------+
    |                                                                |
    |                                                    \0/         |
    |                                                  /   \         |
    |        You're clapping!             Machine Learning!  /|      |
    |            Why?                                      \ \       |
    |            /                                                   |
    |          0                                                     |
    |         /|\                                                    |
    |          |                                                     |
    |-----____/_\______________________________----------------------|
    |                                                                |
    +----------------------------------------------------------------+
    */
    static const char *clapdetector_comic_str =
    "+-xkcd.com/353\xb3-+\n"
    "|\xc0 |\n"
    "|\xb4 \\0/\x89 |\n"
    "|\xb2 /\x83 \\\x89 |\n"
    "|\x88 You're clapping!\x90 Machine Learning!  /|\x88 |\n"
    "|\x8c Why?\xa6 \\ \\\x87 |\n"
    "|\x8c /\xb3 |\n"
    "|\x8a 0\xb5 |\n"
    "|\x89 /|\\\xb4 |\n"
    "|\x8a |\xb5 |\n"
    "|\x85-\x84_/_\\\x9e_\x96-|\n"
    "|\xc0 |\n"
    "+\xc0-+\n";

    clapdetector_print_rle(clapdetector_comic_str);

    for (uint8_t iteration = 0; iteration < 5; iteration++) {
        mp_hal_delay_ms(interval_ms);
        bool wait = false;
        for (uint8_t row = 1; row < 5 - iteration; row++) {
            for (uint8_t col = 0; col < 5; col++) {
                // move this bit down if possible
                uint8_t val = GET_PIXEL(col, row);
                if (val) {
                    // this is why the row for loop starts at one
                    if (!GET_PIXEL(col, row - 1)) {
                        SET_PIXEL(col, row, 0);
                        SET_PIXEL(col, row - 1, val);
                        wait = true;
                    }
                } // we don't care if the LED is off
            }
        }

        if (!wait) {
            continue;
        }
    }
}

STATIC mp_obj_t clapdetector__init__(void) {
    clapdetector(200);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(clapdetector___init___obj, clapdetector__init__);

mp_obj_t clapdetector_is_listening(void) {
    return mp_obj_new_bool(listening);
}
MP_DEFINE_CONST_FUN_OBJ_0(clapdetector_is_listening_obj, clapdetector_is_listening);

STATIC mp_obj_t clapdetector_listen(void) {
    microphone_init();
    listening = true;
    mp_printf(&mp_plat_print, "Clapdetector listening.\n");
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(clapdetector_listen_obj, clapdetector_listen);

STATIC mp_obj_t clapdetector_stop(void) {
    listening = false;
    mp_printf(&mp_plat_print, "Clapdetector stopped.\n");
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(clapdetector_stop_obj, clapdetector_stop);

/* STATIC mp_obj_t clapdetector_use_model(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_model, MP_ARG_INT, {.u_int = DEFAULT_MODEL_NAME} },
    };

    // Parse the args.
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    mp_obj_t input = args[0].u_obj;

    // Check that input is a string
    if (MP_OBJ_IS_STR(input)) {

    }

    mp_printf(&mp_plat_print, "Clapdetector model loaded.\n");
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(clapdetector_use_model_obj, clapdetector_use_model); */

// Returns detected, which is true if a pattern match occurred since last check,
// and resets detected to false
STATIC mp_obj_t clapdetector_was_detected(void) {
    if (detected) {
        detected = false;
        return mp_obj_new_bool(true);
    }
    else return mp_obj_new_bool(detected);
}
MP_DEFINE_CONST_FUN_OBJ_0(clapdetector_was_detected_obj, clapdetector_was_detected);

STATIC const mp_rom_map_elem_t clapdetector_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_clapdetector) },
    { MP_ROM_QSTR(MP_QSTR___init__), MP_ROM_PTR(&clapdetector___init___obj) },
    { MP_ROM_QSTR(MP_QSTR_is_listening), MP_ROM_PTR(&clapdetector_is_listening_obj) },
    { MP_ROM_QSTR(MP_QSTR_listen), MP_ROM_PTR(&clapdetector_listen_obj) },
    { MP_ROM_QSTR(MP_QSTR_stop), MP_ROM_PTR(&clapdetector_stop_obj) },
    // { MP_ROM_QSTR(MP_QSTR_use_model), MP_ROM_PTR(&clapdetector_use_model_obj) },
    { MP_ROM_QSTR(MP_QSTR_was_detected), MP_ROM_PTR(&clapdetector_was_detected_obj) },
};
STATIC MP_DEFINE_CONST_DICT(clapdetector_module_globals, clapdetector_module_globals_table);

const mp_obj_module_t clapdetector_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&clapdetector_module_globals,
};
