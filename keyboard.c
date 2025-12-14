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
#include <sys/time.h>
#include <linux/input.h>
#include <libevdev/libevdev.h>
static struct libevdev *dev = NULL;
static int fd = -1;
static volatile sig_atomic_t running = 1;
static bool vietnamese_mode = true;
static Word current_word;
static struct timeval last_key_time;

// Key state tracking
static bool ctrl_pressed = false;
static bool shift_pressed = false;

// Timeout for word reset (milliseconds)
#define WORD_TIMEOUT_MS 250

static void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

// Minimum score: keyboard should have most letter keys + enter + space
#define MIN_KEYBOARD_SCORE 20

// Find best keyboard device
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

    if (best_score < MIN_KEYBOARD_SCORE) {
        return NULL;
    }

    char *result = strdup(best_path);
    if (!result) {
        perror("strdup");
    }
    return result;
}

// Optimized wtype: combine backspaces and text into single call
static void wtype_replace(int backspace_count, const char *text) {
    // Build command: wtype -k BackSpace ... -- "text"
    char *args[128];
    int idx = 0;
    args[idx++] = "wtype";

    // Add backspaces
    for (int i = 0; i < backspace_count && idx < 100; i++) {
        args[idx++] = "-k";
        args[idx++] = "BackSpace";
    }

    // Add text if provided
    if (text && *text) {
        args[idx++] = "--";
        args[idx++] = (char*)text;
    }

    args[idx] = NULL;

    pid_t pid = fork();
    if (pid == 0) {
        // Silence stderr
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) { dup2(devnull, STDERR_FILENO); close(devnull); }
        execvp("wtype", args);
        _exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }
}

// Fast keycode to ASCII lookup
#define KEYCODE_MAP_SIZE 64
_Static_assert(KEY_Z < KEYCODE_MAP_SIZE, "Keycode map size too small for KEY_Z");

static char keycode_to_ascii(int code, bool shift) {
    static const char map[KEYCODE_MAP_SIZE] = {
        [KEY_A] = 'a', [KEY_B] = 'b', [KEY_C] = 'c', [KEY_D] = 'd',
        [KEY_E] = 'e', [KEY_F] = 'f', [KEY_G] = 'g', [KEY_H] = 'h',
        [KEY_I] = 'i', [KEY_J] = 'j', [KEY_K] = 'k', [KEY_L] = 'l',
        [KEY_M] = 'm', [KEY_N] = 'n', [KEY_O] = 'o', [KEY_P] = 'p',
        [KEY_Q] = 'q', [KEY_R] = 'r', [KEY_S] = 's', [KEY_T] = 't',
        [KEY_U] = 'u', [KEY_V] = 'v', [KEY_W] = 'w', [KEY_X] = 'x',
        [KEY_Y] = 'y', [KEY_Z] = 'z',
    };

    if (code < 0 || code >= KEYCODE_MAP_SIZE) return 0;
    char c = map[code];
    return (c && shift) ? (c - 'a' + 'A') : c;
}

// Check if key breaks word
static inline bool is_word_break(int code) {
    return code == KEY_SPACE || code == KEY_ENTER || code == KEY_TAB ||
           code == KEY_ESC || code == KEY_LEFT || code == KEY_RIGHT ||
           code == KEY_UP || code == KEY_DOWN || code == KEY_HOME ||
           code == KEY_END || code == KEY_DELETE || code == KEY_BACKSPACE;
}

// Telex transformation keys
static inline bool is_telex_key(int code) {
    return code == KEY_S || code == KEY_F || code == KEY_R ||
           code == KEY_X || code == KEY_J || code == KEY_Z ||
           code == KEY_A || code == KEY_E || code == KEY_O ||
           code == KEY_W || code == KEY_D;
}

// Check if word timed out (user paused typing)
static inline bool check_word_timeout(const struct timeval *now) {
    if (last_key_time.tv_sec == 0) return false;
    long diff_ms = (now->tv_sec - last_key_time.tv_sec) * 1000 +
                   (now->tv_usec - last_key_time.tv_usec) / 1000;
    return diff_ms > WORD_TIMEOUT_MS;
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
        perror("libevdev_new_from_fd");
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
    fflush(stdout);
}

bool keyboard_is_vietnamese(void) {
    return vietnamese_mode;
}

void keyboard_run(void) {
    struct input_event ev;
    struct timeval now;

    while (running != 0) {
        int rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL | LIBEVDEV_READ_FLAG_BLOCKING, &ev);
        if (rc < 0) {
            if (rc == -EAGAIN) {
                usleep(500);
                continue;
            }
            break;
        }

        if (ev.type != EV_KEY) continue;

        // Get current time for timeout check
        gettimeofday(&now, NULL);

        // Reset word if user paused typing
        if (current_word.len > 0 && check_word_timeout(&now)) {
            telex_reset(&current_word);
        }

        // Track modifiers
        if (ev.code == KEY_LEFTCTRL || ev.code == KEY_RIGHTCTRL) {
            ctrl_pressed = (ev.value != 0);
            continue;
        }
        if (ev.code == KEY_LEFTSHIFT || ev.code == KEY_RIGHTSHIFT) {
            shift_pressed = (ev.value != 0);
            continue;
        }

        // Only key press
        if (ev.value != 1) continue;

        // Toggle
        if (ev.code == KEY_SPACE && ctrl_pressed) {
            keyboard_toggle_vietnamese();
            continue;
        }

        // English mode - ignore
        if (!vietnamese_mode) continue;

        // Backspace - sync buffer with actual deletion
        if (ev.code == KEY_BACKSPACE) {
            if (current_word.len > 0) {
                current_word.len--;
            }
            // If buffer empty, ensure clean state
            if (current_word.len == 0) {
                telex_reset(&current_word);
            }
            continue;
        }

        // Word break
        if (is_word_break(ev.code)) {
            telex_reset(&current_word);
            continue;
        }

        // Get character
        char c = keycode_to_ascii(ev.code, shift_pressed);
        if (!c) {
            telex_reset(&current_word);
            continue;
        }

        // Update last key time for timeout tracking
        last_key_time = now;

        // Process Telex
        if (is_telex_key(ev.code) && current_word.len > 0) {
            int old_len = current_word.len;
            Word backup = current_word;

            int result = telex_process(&current_word, c);

            if (result == 1) {
                // Normal transformation
                char utf8_buf[MAX_WORD_LEN * 4 + 1];
                word_to_utf8(&current_word, utf8_buf, sizeof(utf8_buf));

                // Single wtype call: backspaces + new text
                wtype_replace(old_len + 1, utf8_buf);
                continue;
            } else if (result == 2) {
                // Double press: undo tone and add the key char
                // Add the key to buffer
                if (current_word.len < MAX_WORD_LEN - 1) {
                    current_word.chars[current_word.len++] = (uint32_t)c;
                }
                char utf8_buf[MAX_WORD_LEN * 4 + 1];
                word_to_utf8(&current_word, utf8_buf, sizeof(utf8_buf));

                // Replace: old word + pressed key -> new word with key
                wtype_replace(old_len + 1, utf8_buf);
                continue;
            }
            current_word = backup;
        }

        // Add to buffer
        if (current_word.len < MAX_WORD_LEN - 1) {
            current_word.chars[current_word.len++] = (uint32_t)c;
            // Smart tone: move tone to correct position after adding vowel
            if (telex_is_vowel(c)) {
                telex_normalize_tone(&current_word);
            }
        }
    }
}
