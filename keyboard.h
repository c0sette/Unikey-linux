#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdbool.h>

// Initialize keyboard capture (requires root or input group)
int keyboard_init(void);

// Cleanup
void keyboard_cleanup(void);

// Start main loop
void keyboard_run(void);

// Toggle Vietnamese mode
void keyboard_toggle_vietnamese(void);

// Check if Vietnamese mode is active
bool keyboard_is_vietnamese(void);

#endif
