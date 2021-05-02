/* TODO:
 * Get model.call_predict(arr) working
 */

#include "py/runtime.h"
#include "py/mphal.h"
#include "modmicrobit.h"

#define SAMPLE_SIZE (40) // Measured in ms
#define NUMBER_OF_SAMPLES (75)

// Copied from microphone module
#define SOUND_EVENT_NONE (0)
#define SOUND_EVENT_LOUD (1)
#define SOUND_EVENT_QUIET (2)

typedef struct _microbit_clap_detector_obj_t {
    mp_obj_base_t base;
} microbit_clap_detector_obj_t;

static uint32_t start_time = 0;
static bool active = false, detected = false;

// A circular array containing the last NUMBER_OF_SAMPLES samples. Overwrite in a circle so the samples are
// the most recent set, without having to shuffle things around every time an event happens.
static uint8_t samples_index = 0;
static bool samples_array[NUMBER_OF_SAMPLES];

// The most recently detected event, and the sample timestamp it was detected at
static uint8_t current_event = SOUND_EVENT_NONE;
static uint32_t current_sample_timestamp = 0;

// The timestamp at which the model was last run, to avoid running the model repeatedly on the same input
static uint32_t last_predicted_timestamp = 0;

// Stand in until ML is working
bool call_predict(bool array[NUMBER_OF_SAMPLES]) {
    return array[0];
}

void microbit_hal_clap_detector_level_callback(int value) { // type of utime may need adjusting
    if (active) {
        // Use event code from microphone module
        uint8_t ev = SOUND_EVENT_NONE;
        if (value == MICROBIT_HAL_MICROPHONE_LEVEL_THRESHOLD_LOW) {
            ev = SOUND_EVENT_QUIET;
        } else if (value == MICROBIT_HAL_MICROPHONE_LEVEL_THRESHOLD_HIGH) {
            ev = SOUND_EVENT_LOUD;
        }

        // Get the number of SAMPLE_SIZE blocks that have passed since start, if this is more than last time this ran,
        // we need to fill in the array for the samples that have passed, with the event *last* detected
        uint32_t sample_timestamp = (mp_hal_ticks_ms() - start_time) / SAMPLE_SIZE;
        uint32_t to_fill = sample_timestamp - current_sample_timestamp;

        bool to_write = 0;
        if (current_event == SOUND_EVENT_LOUD) {to_write = 1;}

        // Fill in to_fill number of entries in the array with to_write, ensuring to wrap around when reaching the end    
        for (uint32_t i = 0; i < to_fill; i++) {
            samples_array[samples_index] = to_write;
            samples_index = (samples_index + 1) % NUMBER_OF_SAMPLES;
        }
    
        // Set number of samples handled and event for next time
        current_sample_timestamp = sample_timestamp;
        current_event = ev;
    }
}

void microbit_hal_utime_interrupt_callback() {
    if (active && (current_sample_timestamp != last_predicted_timestamp)) {
        // Use this interrupt to run the model, and save the result in detected
        bool flat_array[NUMBER_OF_SAMPLES];
    
        // Shift the array so flat_array[0] is the start, to pass to the model
        for (uint8_t i = 0; i < NUMBER_OF_SAMPLES; i++) {
            flat_array[i] = samples_array[(i + samples_index) % NUMBER_OF_SAMPLES];
        }
        detected = call_predict(flat_array) | detected;
        last_predicted_timestamp = current_sample_timestamp;
    }
}

STATIC mp_obj_t microbit_clap_detector_listen(mp_obj_t self_in) {
    (void)self_in;
    active = true;
    microbit_hal_microphone_init();
    start_time = mp_hal_ticks_ms();
    // Any other setup goes here
    return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_1(microbit_clap_detector_listen_obj, microbit_clap_detector_listen);

STATIC mp_obj_t microbit_clap_detector_was_detected(mp_obj_t self_in) {
    (void)self_in;
    bool temp = detected;
    detected = false;
    return mp_obj_new_bool(temp);
}

STATIC MP_DEFINE_CONST_FUN_OBJ_1(microbit_clap_detector_was_detected_obj, microbit_clap_detector_was_detected);

// Expose the samples_array in python for debugging purposes

STATIC mp_obj_t microbit_clap_detector_debug(mp_obj_t self_in) {
    (void)self_in;
    mp_obj_t values[NUMBER_OF_SAMPLES];
    for (uint8_t i = 0; i < NUMBER_OF_SAMPLES; i++) {
        values[i] = mp_obj_new_bool(samples_array[i]);
    }
    return mp_obj_new_tuple((size_t) NUMBER_OF_SAMPLES, values);
}

STATIC MP_DEFINE_CONST_FUN_OBJ_1(microbit_clap_detector_debug_obj, microbit_clap_detector_debug);

STATIC const mp_rom_map_elem_t microbit_clap_detector_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_listen), MP_ROM_PTR(&microbit_clap_detector_listen_obj) },
    { MP_ROM_QSTR(MP_QSTR_was_detected), MP_ROM_PTR(&microbit_clap_detector_was_detected_obj) },
    { MP_ROM_QSTR(MP_QSTR_debug), MP_ROM_PTR(&microbit_clap_detector_debug_obj) },
};

STATIC MP_DEFINE_CONST_DICT(microbit_clap_detector_locals_dict, microbit_clap_detector_locals_dict_table);

const mp_obj_type_t microbit_clap_detector_type = {
    { &mp_type_type },
    .name = MP_QSTR_MicroBitClap_detector,
    .locals_dict = (mp_obj_dict_t *)&microbit_clap_detector_locals_dict,
};

const microbit_clap_detector_obj_t microbit_clap_detector_obj = {
    { &microbit_clap_detector_type },
};
