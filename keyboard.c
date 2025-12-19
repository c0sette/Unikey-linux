#define _GNU_SOURCE
#include "keyboard.h"
#include "telex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <linux/input.h>
#include <libevdev/libevdev.h>

static struct libevdev *dev = NULL;
static int fd = -1;
static volatile sig_atomic_t running = 1;
static bool vietnamese_mode = true;
static Word current_word;

// Modifier state
static bool shift_pressed = false;
static bool ctrl_pressed = false;

static void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

// Find keyboard device
static char* find_keyboard(void) {
    char path[64];
    int best_score = 0;
    char best_path[64] = {0};

    for (int i = 0; i < 20; i++) {
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int test_fd = open(path, O_RDONLY | O_NONBLOCK);
        if (test_fd < 0) continue;

        struct libevdev *test_dev = NULL;
        if (libevdev_new_from_fd(test_fd, &test_dev) < 0) {
            close(test_fd);
            continue;
        }

        const char *name = libevdev_get_name(test_dev);
        bool skip = name && (strstr(name, "Mouse") || strstr(name, "mouse") ||
                            strstr(name, "Virtual") || strstr(name, "UniKey"));

        if (!skip) {
            int score = 0;
            for (int k = KEY_Q; k <= KEY_P; k++)
                if (libevdev_has_event_code(test_dev, EV_KEY, k)) score++;
            for (int k = KEY_A; k <= KEY_L; k++)
                if (libevdev_has_event_code(test_dev, EV_KEY, k)) score++;
            for (int k = KEY_Z; k <= KEY_M; k++)
                if (libevdev_has_event_code(test_dev, EV_KEY, k)) score++;
            if (libevdev_has_event_code(test_dev, EV_KEY, KEY_ENTER)) score += 5;
            if (libevdev_has_event_code(test_dev, EV_KEY, KEY_SPACE)) score += 5;

            if (score > best_score) {
                best_score = score;
                snprintf(best_path, sizeof(best_path), "%s", path);
            }
        }
        libevdev_free(test_dev);
        close(test_fd);
    }

    return (best_score >= 20) ? strdup(best_path) : NULL;
}

// Send backspaces + text via wtype (single call for efficiency)
static void wtype_replace(int bs_count, const char *text) {
    char *args[128];
    int idx = 0;
    args[idx++] = "wtype";

    for (int i = 0; i < bs_count && idx < 100; i++) {
        args[idx++] = "-k";
        args[idx++] = "BackSpace";
    }

    if (text && *text) {
        args[idx++] = "--";
        args[idx++] = (char*)text;
    }
    args[idx] = NULL;

    pid_t pid = fork();
    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, STDERR_FILENO); close(devnull); }
        execvp("wtype", args);
        _exit(1);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
    }
}

// Key to character
static char key_to_char(int code, bool shift) {
    static const char map[64] = {
        [KEY_A] = 'a', [KEY_B] = 'b', [KEY_C] = 'c', [KEY_D] = 'd',
        [KEY_E] = 'e', [KEY_F] = 'f', [KEY_G] = 'g', [KEY_H] = 'h',
        [KEY_I] = 'i', [KEY_J] = 'j', [KEY_K] = 'k', [KEY_L] = 'l',
        [KEY_M] = 'm', [KEY_N] = 'n', [KEY_O] = 'o', [KEY_P] = 'p',
        [KEY_Q] = 'q', [KEY_R] = 'r', [KEY_S] = 's', [KEY_T] = 't',
        [KEY_U] = 'u', [KEY_V] = 'v', [KEY_W] = 'w', [KEY_X] = 'x',
        [KEY_Y] = 'y', [KEY_Z] = 'z',
    };
    if (code < 0 || code >= 64) return 0;
    char c = map[code];
    return (c && shift) ? (c - 32) : c;
}

// Telex keys that can trigger transformation
static inline bool is_telex_key(int code) {
    return code == KEY_S || code == KEY_F || code == KEY_R ||
           code == KEY_X || code == KEY_J || code == KEY_Z ||
           code == KEY_A || code == KEY_E || code == KEY_O ||
           code == KEY_W || code == KEY_D;
}

// Keys that break word context
static inline bool is_word_break(int code) {
    return code == KEY_SPACE || code == KEY_ENTER || code == KEY_TAB ||
           code == KEY_ESC || code == KEY_LEFT || code == KEY_RIGHT ||
           code == KEY_UP || code == KEY_DOWN || code == KEY_HOME ||
           code == KEY_END || code == KEY_DELETE || code == KEY_PAGEUP ||
           code == KEY_PAGEDOWN;
}

// Punctuation/number keys
static inline bool is_punct_key(int code) {
    return (code >= KEY_1 && code <= KEY_0) ||
           code == KEY_MINUS || code == KEY_EQUAL ||
           code == KEY_LEFTBRACE || code == KEY_RIGHTBRACE ||
           code == KEY_SEMICOLON || code == KEY_APOSTROPHE ||
           code == KEY_GRAVE || code == KEY_BACKSLASH ||
           code == KEY_COMMA || code == KEY_DOT || code == KEY_SLASH;
}

int keyboard_init(void) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    telex_init();
    telex_reset(&current_word);

    char *devpath = find_keyboard();
    if (!devpath) {
        fprintf(stderr, "No keyboard found\n");
        return -1;
    }

    fd = open(devpath, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Cannot open %s: %s\n", devpath, strerror(errno));
        free(devpath);
        return -1;
    }
    printf("Keyboard: %s\n", devpath);
    free(devpath);

    if (libevdev_new_from_fd(fd, &dev) < 0) {
        close(fd);
        return -1;
    }

    printf("UniKey ready. Mode: %s | Toggle: Ctrl+Space\n",
           vietnamese_mode ? "VI" : "EN");
    return 0;
}

void keyboard_cleanup(void) {
    if (dev) libevdev_free(dev);
    if (fd >= 0) close(fd);
}

void keyboard_toggle_vietnamese(void) {
    vietnamese_mode = !vietnamese_mode;
    telex_reset(&current_word);
    printf("\rMode: %s      \n", vietnamese_mode ? "VI" : "EN");
}

bool keyboard_is_vietnamese(void) {
    return vietnamese_mode;
}

void keyboard_run(void) {
    struct input_event ev;

    while (running) {
        int rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL | LIBEVDEV_READ_FLAG_BLOCKING, &ev);
        if (rc < 0) {
            if (rc == -EAGAIN) { usleep(1000); continue; }
            break;
        }

        if (ev.type != EV_KEY) continue;

        // Track modifiers
        if (ev.code == KEY_LEFTSHIFT || ev.code == KEY_RIGHTSHIFT) {
            shift_pressed = (ev.value != 0);
            continue;
        }
        if (ev.code == KEY_LEFTCTRL || ev.code == KEY_RIGHTCTRL) {
            ctrl_pressed = (ev.value != 0);
            continue;
        }

        // Only key press (not release or repeat)
        if (ev.value != 1) continue;

        // Ctrl+Space toggle
        if (ev.code == KEY_SPACE && ctrl_pressed) {
            keyboard_toggle_vietnamese();
            continue;
        }
        // Skip if Ctrl held (shortcuts)
        if (ctrl_pressed) {
            telex_reset(&current_word);
            continue;
        }

        // English mode - just track buffer for sync
        if (!vietnamese_mode) {
            char c = key_to_char(ev.code, shift_pressed);
            if (c && current_word.len < MAX_WORD_LEN - 1) {
                current_word.chars[current_word.len++] = c;
            } else if (is_word_break(ev.code) || is_punct_key(ev.code)) {
                telex_reset(&current_word);
            } else if (ev.code == KEY_BACKSPACE && current_word.len > 0) {
                current_word.len--;
            }
            continue;
        }

        // Vietnamese mode

        // Backspace
        if (ev.code == KEY_BACKSPACE) {
            if (current_word.len > 0) current_word.len--;
            if (current_word.len == 0) telex_reset(&current_word);
            continue;
        }

        // Word break
        if (is_word_break(ev.code) || is_punct_key(ev.code)) {
            telex_reset(&current_word);
            continue;
        }

        // Get character
        char c = key_to_char(ev.code, shift_pressed);
        if (!c) {
            telex_reset(&current_word);
            continue;
        }

        // Try telex transformation
        if (is_telex_key(ev.code) && current_word.len > 0) {
            int old_len = current_word.len;
            Word backup = current_word;

            int result = telex_process(&current_word, c);

            if (result == 1) {
                // Transformation succeeded
                // Delete old text + the key just typed, then type new text
                char utf8[MAX_WORD_LEN * 4 + 1];
                word_to_utf8(&current_word, utf8, sizeof(utf8));
                wtype_replace(old_len + 1, utf8);
                continue;
            } else if (result == 2) {
                // Double press - undo and add the char
                if (current_word.len < MAX_WORD_LEN - 1) {
                    current_word.chars[current_word.len++] = c;
                }
                char utf8[MAX_WORD_LEN * 4 + 1];
                word_to_utf8(&current_word, utf8, sizeof(utf8));
                wtype_replace(old_len + 1, utf8);
                continue;
            }

            // No transformation, restore
            current_word = backup;
        }

        // Just add to buffer (original keystroke goes through naturally)
        if (current_word.len < MAX_WORD_LEN - 1) {
            current_word.chars[current_word.len++] = c;
        }
    }
}
