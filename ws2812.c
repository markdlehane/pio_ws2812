/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/watchdog.h"
#include "ws2812.pio.h"

/**
 * NOTE:
 *  Take into consideration if your WS2812 is a RGB or RGBW variant.
 *
 *  If it is RGBW, you need to set IS_RGBW to true and provide 4 bytes per 
 *  pixel (Red, Green, Blue, White) and use urgbw_u32().
 *
 *  If it is RGB, set IS_RGBW to false and provide 3 bytes per pixel (Red,
 *  Green, Blue) and use rgb_u32().
 *
 *  When RGBW is used with rgb_u32(), the White channel will be ignored (off).
 *
 */
#define NUM_PIXELS  (100)
#define NUM_PERPAGE (4)
#define NUM_PAGES   (NUM_PIXELS / NUM_PERPAGE)
#define LED_PIN     (28)
#define MODE_PIN    (16)
#define MODE_MAX    (6)

// Operating data.
static volatile bool    led_pressed = false;            // Tracks when the button is pushed.
static volatile bool    button_interrupt = false;       // Toggled when the button is pressed and released.
static volatile int     led_pattern = 0;                // Which pattern is being displayed.
static const int        led_pattern_max = MODE_MAX;     // The maxmimum pattern code.

/**
 * @brief Format a RGBw value to a pixel.
 * @details The ws2812b has GRB encoded LEDS, check for the encoding of your
 * specific LED type.
 * 
 * @param r Red value.
 * @param g Green value.
 * @param b Blue value.
 * @return uint32_t Formatted RGB value.
 */
static inline uint32_t rgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t) (g) << 8) | ((uint32_t) (r) << 16) | (uint32_t) (b);
}

/**
 * @brief Write "array_size" elements to the WS2812b LED string.
 * 
 * @param pio PIO identifier.
 * @param sm State machine identifier.
 * @param array A pointer to an array of uint32_t values.
 * @param array_size The number of elements in the array.
 */
static void led_array_write(PIO pio, uint sm, uint32_t *array, size_t array_size) {
    for (size_t idx = 0; idx < array_size; idx++) {
        pio_sm_put_blocking(pio, sm, (*array++) << 8u);
    }
}

/**
 * @brief Set the led array to given colour.
 * 
 * @param array A pointer to an array of uint32_t values.
 * @param array_size The number of elements in the array.
 * @param colour RGB colour.
 */
static void led_array_set(uint32_t *array, size_t array_size, uint32_t colour) {
    for (size_t idx = 0; idx < array_size; idx++) {
        *array++ = colour;
    }
}

/**
 * @brief Get the interrupted state flag.
 * @details Returns the interrupt flag state. If the flag
 * is set, it is cleared and the set state is returned.
 * 
 * @return true An interrupt occurred.
 * @return false No interrupt event.
 */
static bool get_interrupted(void) {
    bool ret;

    ret = button_interrupt;
    if (ret) {
        button_interrupt = false;
    }
    return ret;
}

/**
 * @brief Called when an event changes the GPIO pin.
 * 
 * @param gpio GPIO pin where the event(s) occurred.
 * @param events The events that occurred.
 */
void gpio_callback(uint gpio, uint32_t events) {

    if (events & 0x04) {
        // Adjust the LED mode.
        if (led_pressed == false) {
            led_pressed = true;
        }
    }
    else if (events & 0x08) {
        if (led_pressed == true) {
            led_pattern++;
            if (led_pattern == led_pattern_max) {
                led_pattern = 0;
            }
            button_interrupt = true;
            led_pressed = false;
        }
    }
}

/**
 * @brief Set the LED array to off.
 * 
 * @param pio PIO handle.
 * @param sm State machine identifier.
 * @param array Pointer to the LED array buffer.
 * @param array_size The number of LEDs in the array buffer.
 */
static inline void clear_leds(PIO pio, int sm, uint32_t *array, size_t array_size) {
    led_array_set(array, array_size, 0);
    led_array_write(pio, sm, array, array_size);
}

/**
 * @brief Perform a 3 channel cross fade (red, green and blue).
 * 
 * @param pio PIO handle.
 * @param sm State machine id.
 * @param array A pointer to an LED array.
 * @param array_size The size of the LED array.
 * @param red Initial red channel value.
 * @param grn Initial grn channel value.
 * @param blu Initial blu channel value.
 * @param period Cycle period (in ms).
 * @param adj Adjustment rate.
 */
static void fade_three(PIO pio, int sm, uint32_t *array, size_t array_size, uint8_t red, uint8_t grn, uint8_t blu, uint16_t period, int adj) {
    
    // Compute the step for a full transition.
    uint16_t wait_ms = (((period*10)/256)+5)/10;
    uint16_t step_count = period / wait_ms;
        
    // Define the initial direction that each colour adjusts by.
    int d_red = (red > 127) ? -adj : adj;
    int d_grn = (grn > 127) ? -adj : adj;
    int d_blu = (blu > 127) ? -adj : adj;
    
    // Loop until the cycles are zero.
    uint16_t step_no = 0;
    while (1) {

        // Early exit if the mode button was pressed.
        if (get_interrupted()) {
            break;
        }

        // Draw the current RGB values - limit brightness to 0-31.
        uint32_t *ap = array;
        for (size_t idx = 0; idx < array_size; idx++) {
            ap[0] = rgb_u32(red>>3, 0, 0);
            ap[1] = rgb_u32(0, grn>>3, 0);
            ap[2] = rgb_u32(0, 0, blu>>3);
            ap += 3;
        }
        led_array_write(pio, sm, array, array_size);

        // Adjust the colours according to their directions.
        red += d_red;
        grn += d_grn;
        blu += d_blu;

        // Check for maximum and minimum.
        d_red = (red == 255) ? -1 : (red == 0) ? 1 : d_red;
        d_grn = (grn == 255) ? -1 : (grn == 0) ? 1 : d_grn;
        d_blu = (blu == 255) ? -1 : (blu == 0) ? 1 : d_blu;

        // Adjust the remaining cycles (if above zero).
        step_no++;
        if (step_no >= step_count) {
            step_no = 0;
        }

        // Wait for a moment while the LEDs display the selected colour.
        else {
            sleep_ms(wait_ms);
        }
    }
}

/**
 * @brief 
 * 
 * @param pio PIO handle.
 * @param sm State machine id.
 * @param array A pointer to an LED array.
 * @param array_size The size of the LED array.
 * @param period Cycle period (in ms).
 */
static void step_three(PIO pio, int sm, uint32_t *array, size_t array_size, uint16_t period) {
    
    // Compute the step for a full transition (divide by 2 because of the PIO delay).
    uint16_t wait_ms = (((period*10)/255)+5)/10;
    uint16_t step_count = period / (wait_ms << 1u);

    // Inner loop.
    int clr_index = 0;
    uint8_t clr_value = 0;
    while (1) {

        // Early exit if the mode button was pressed.
        if (get_interrupted()) {
            break;
        }        
        
        switch (clr_index) {
            default:
            case 0:
                led_array_set(array, array_size, rgb_u32(clr_value>>3, 0, 0));
                break;
            case 1:
                led_array_set(array, array_size, rgb_u32(0, clr_value>>3, 0));
                break;
            case 2:
                led_array_set(array, array_size, rgb_u32(0,0, clr_value>>3));
                break;
        }
        led_array_write(pio, sm, array, array_size);
        
        clr_value++;
        if (clr_value == 255) {
            clr_value = 0;
            clr_index++;
            if (clr_index > 2) {
                clr_index = 0;
            }
        }
        sleep_ms(wait_ms);
    }
}

/**
 * @brief Walk three colours along the string of LEDs.
 * 
 * @param pio PIO handle.
 * @param sm State machine id.
 * @param array A pointer to an LED array.
 * @param array_size The size of the LED array.
 * @param period The time between steps.
 */
static void walk_three(PIO pio, int sm, uint32_t *array, size_t array_size, uint16_t period) {

    // Establish the colours.
    uint32_t red = rgb_u32(255>>3, 0, 0);
    uint32_t grn = rgb_u32(0, 255>>3, 0);
    uint32_t blu = rgb_u32(0, 0, 255>>3);

    // Inner loop.
    uint8_t step = 0;
    uint32_t clrs[3];
    while (1) {

        // Early exit if the mode button was pressed.
        if (get_interrupted()) {
            break;
        }

        // Set the colours by step.
        if (step == 0) {
            clrs[0] = red;
            clrs[1] = grn;
            clrs[2] = blu;
            step = 1;
        }
        else if (step == 1) {
            clrs[2] = red;
            clrs[0] = grn;
            clrs[1] = blu;
            step = 2;
        }
        else {
            clrs[1] = red;
            clrs[2] = grn;
            clrs[0] = blu;
            step = 0;
        }
    
        // Update the colours.
        for (size_t i = 0; i < array_size; i++) {
            array[i] = clrs[i % 3];
        }

        // Write out the colours.
        led_array_write(pio, sm, array, array_size);
        sleep_ms(period);
    }
}

/**
 * @brief Walk three colours along the string of LEDs.
 * 
 * @param pio PIO handle.
 * @param sm State machine id.
 * @param array A pointer to an LED array.
 * @param array_size The size of the LED array.
 * @param period The time between steps.
 * @param bg_on True for colour background, false for black.
 */
static void chase_colour(PIO pio, int sm, uint32_t *array, size_t array_size, uint16_t period, bool bg_on) {
    
    // Clear the array.
    clear_leds(pio, sm, array, array_size);
    
    // Start the run with red, then green then blue.
    int clr_pos = 0;
    uint8_t clr_id = 'r';
    int clr_dir = 1;
    while (1) {
        
        // Early exit if the mode button was pressed.
        if (get_interrupted()) {
            break;
        }
        
        // Create a colour code from the colour id.
        uint32_t clr_fg, clr_bg;
        if (clr_id == 'r') {
            clr_fg = 0x0f0000u;
            clr_bg = bg_on ? 0x000201u : 0;
        }
        else if (clr_id == 'g') {
            clr_fg = 0x000f00u;
            clr_bg = bg_on ? 0x010002u : 0;
        }
        else {
            clr_fg = 0x00000fu;
            clr_bg = bg_on ? 0x020100u : 0;
        }

        // Clear the array, setting the indexed colour only.
        for (size_t idx = 0; idx < array_size; idx++) {
            if (idx == clr_pos) {
                array[idx] = clr_fg;
            }
            else {
                array[idx] = clr_bg;
            }
        }
        // Write the array to the LEDs.
        led_array_write(pio, sm, array, array_size);

        // Advance the position.
        if (clr_dir == 1) {
            clr_pos++;
            if (clr_pos == array_size) {
                clr_dir = -1;
                clr_pos--;
            }
        }
        else if (clr_dir == -1) {
            clr_pos--;
            if (clr_pos < 0) {
                clr_pos = 0;
                clr_dir = 1;
                switch (clr_id) {
                    default:
                    case 'r':
                        clr_id = 'g';
                        break;
                    case 'g':
                        clr_id = 'b';
                        break;
                    case 'b':
                        clr_id = 'r';
                        break;
                }
            }
        }
        sleep_ms(period);
    }
}

/**
 * @brief Profram entry point.
 * 
 * @return int 0.
 */
int main() {
    // Setup STDIO and tell the console what's going on.
    stdio_init_all();

    // Setup GPIO16 as a mode switch.
    gpio_init(MODE_PIN);
    gpio_set_irq_enabled_with_callback(MODE_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    
    // This will find a free pio and state machine for our program and load it for us
    // We use pio_claim_free_sm_and_add_program_for_gpio_range (for_gpio_range variant)
    // so we will get a PIO instance suitable for addressing gpios >= 32 if needed and 
    // supported by the hardware.
    PIO pio;
    uint sm;
    uint offset;
    printf("Setup WS2812b, using pin %d\n", LED_PIN);
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&ws2812_program, &pio, &sm, &offset, LED_PIN, 1, true);
    if (success == false) {
        printf("Failed to initialise PIO for program on pin %d\n", LED_PIN);
    }
    else {        
        // Allocate a buffer for the colour data.
        size_t array_size = sizeof(uint32_t) * NUM_PIXELS;
        uint32_t *led_array = calloc(sizeof(uint32_t), NUM_PIXELS);
        if (led_array == NULL) {
            puts("Failed to allocate a LED array!");
        }
        else {
            printf("Allocated %u x %u = %lu bytes for a LED array\n", sizeof(uint32_t), NUM_PIXELS, array_size);

            // Initialise the WS2812b LED array as RGB only.
            ws2812_program_init(pio, sm, offset, LED_PIN, 800000, false);
            clear_leds(pio, sm, led_array, NUM_PIXELS);
            sleep_ms(1000);

            // Endless loop.
            // clear_leds(pio, sm, led_array, NUM_PIXELS);
            while(1) {

                printf("led mode %d\n", led_pattern);
                switch(led_pattern) {
                    case 0:
                        // Tripple chaser with 100ms separation.
                        walk_three(pio, sm, led_array, NUM_PIXELS, 100);
                        break;
                    case 1:
                        // Slow fade over 3 seconds.
                        fade_three(pio, sm, led_array, NUM_PIXELS, 255, 0, 127, 3000, 1);
                        break;
                    case 2:
                        // Tripple chaser with 200ms separation.
                        walk_three(pio, sm, led_array, NUM_PIXELS, 200);
                        break;
                    case 3:
                        // Quick pulse with 3 second duration.
                        fade_three(pio, sm, led_array, NUM_PIXELS, 255, 0, 127, 3000, 2);
                        break;
                    case 4:
                        // Colour chaser.
                        chase_colour(pio, sm, led_array, NUM_PIXELS, 100, false);
                        break;
                    case 5:
                        // Colour chaser.
                        chase_colour(pio, sm, led_array, NUM_PIXELS, 100, true);
                        break;
                }
            }
            // free up resources.
            puts("Releasing LED array buffer");
            free(led_array);
        }
        // This will free resources and unload our program
        pio_remove_program_and_unclaim_sm(&ws2812_program, pio, sm, offset);
    }

    puts("Exiting and rebooting.");
    for (int tick = 10; tick > 0; tick--) {
        printf("Reboot in %d\n", tick);
    }    
    watchdog_reboot(0, 0, 1);
    return 0;
}

/* End. */
