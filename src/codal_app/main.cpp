/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Damien P. George
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

#include "main.h"
// #include "../../lib/codal/libraries/codal-microbit-v2/inc/compat/mbed.h"
// #include "TensorFlowLite.h"
// #include "tflite/model.h"
#include "tflite/main_functions.h"
#include "tflite/hello_world.h"

#define MICROPY_TIMER_EVENT (0x1001)
#define MICROPY_NOISE_EVENT (0x1002)

extern "C" void mp_main(void);
extern "C" void m_printf(...);
extern "C" void microbit_hal_timer_callback(void);
extern "C" void microbit_hal_gesture_callback(int);
extern "C" void microbit_hal_level_detector_callback(int);
extern "C" void microbit_hal_clap_detector_callback(void);
extern "C" void microbit_radio_irq_handler(void);

MicroBit uBit;

void timer_handler(Event evt) {
    microbit_hal_timer_callback();
}

void gesture_event_handler(Event evt) {
    microbit_hal_gesture_callback(evt.value);
}

void level_detector_event_handler(Event evt) {
    microbit_hal_level_detector_callback(evt.value);
}

void clap_detector_event_handler(Event evt) {
    microbit_hal_clap_detector_callback();
}

int main() {
    uBit.init();

    // Reconfigure the radio IRQ to our custom handler.
    // This must be done after uBit.init() in case BLE pairing mode is activated there.
    NVIC_SetVector(RADIO_IRQn, (uint32_t)microbit_radio_irq_handler);

    // As well as configuring a larger RX buffer, this needs to be called so it
    // calls Serial::initialiseRx, to set up interrupts.
    uBit.serial.setRxBufferSize(128);

    uBit.messageBus.listen(MICROPY_TIMER_EVENT, DEVICE_EVT_ANY, timer_handler, MESSAGE_BUS_LISTENER_IMMEDIATE);
    uBit.messageBus.listen(DEVICE_ID_SERIAL, CODAL_SERIAL_EVT_DELIM_MATCH, serial_interrupt_handler, MESSAGE_BUS_LISTENER_IMMEDIATE);
    uBit.messageBus.listen(DEVICE_ID_GESTURE, DEVICE_EVT_ANY, gesture_event_handler);
    uBit.messageBus.listen(DEVICE_ID_SYSTEM_LEVEL_DETECTOR, DEVICE_EVT_ANY, level_detector_event_handler);
    uBit.messageBus.listen(MICROPY_NOISE_EVENT, DEVICE_EVT_ANY, clap_detector_event_handler, MESSAGE_BUS_LISTENER_IMMEDIATE);

    // 6ms follows the micro:bit v1 value
    system_timer_event_every(6, MICROPY_TIMER_EVENT, 1);
    // 40ms follows the online python editor's 25 noise recordings per second
    system_timer_event_every(40, MICROPY_NOISE_EVENT, 1);

    uBit.display.setBrightness(255);

    // By default the speaker is enabled but no pin is selected.  The audio system will
    // select the correct pin when any audio related code is first executed.
    uBit.audio.setSpeakerEnabled(true);
    uBit.audio.setPinEnabled(false);

    setup();

    mp_main();
    return 0;
}
