#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "keyboard.h"

static void print_usage(const char *prog) {
    printf("UniKey - Vietnamese Input Method for Linux/Wayland\n");
    printf("Usage: %s [options]\n", prog);
    printf("Options:\n");
    printf("  -h, --help    Show this help\n");
    printf("\n");
    printf("Requires root or membership in 'input' group.\n");
    printf("Toggle: Ctrl+Space\n");
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        if (argv[1][0] == '-' && (argv[1][1] == 'h' || argv[1][1] == '-')) {
            print_usage(argv[0]);
            return 0;
        }
    }

    if (geteuid() != 0) {
        // Check if user is in input group
        if (access("/dev/input/event0", R_OK) != 0) {
            fprintf(stderr, "Error: Need root or input group membership\n");
            fprintf(stderr, "Run: sudo usermod -aG input $USER\n");
            fprintf(stderr, "Then log out and back in.\n");
            return 1;
        }
    }

    if (keyboard_init() < 0) {
        fprintf(stderr, "Failed to initialize keyboard\n");
        return 1;
    }

    keyboard_run();
    keyboard_cleanup();

    printf("UniKey exited.\n");
    return 0;
}
