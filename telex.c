#include "telex.h"
#include <string.h>
#include <ctype.h>

// Vietnamese vowels with tones
// Rows: base vowel variants, Columns: tones (0=none, 1=sắc, 2=huyền, 3=hỏi, 4=ngã, 5=nặng)
static const uint32_t vowel_table[][6] = {
    {'a', 0x00E1, 0x00E0, 0x1EA3, 0x00E3, 0x1EA1},  // 0: a
    {'A', 0x00C1, 0x00C0, 0x1EA2, 0x00C3, 0x1EA0},  // 1: A
    {0x0103, 0x1EAF, 0x1EB1, 0x1EB3, 0x1EB5, 0x1EB7},  // 2: ă
    {0x0102, 0x1EAE, 0x1EB0, 0x1EB2, 0x1EB4, 0x1EB6},  // 3: Ă
    {0x00E2, 0x1EA5, 0x1EA7, 0x1EA9, 0x1EAB, 0x1EAD},  // 4: â
    {0x00C2, 0x1EA4, 0x1EA6, 0x1EA8, 0x1EAA, 0x1EAC},  // 5: Â
    {'e', 0x00E9, 0x00E8, 0x1EBB, 0x1EBD, 0x1EB9},  // 6: e
    {'E', 0x00C9, 0x00C8, 0x1EBA, 0x1EBC, 0x1EB8},  // 7: E
    {0x00EA, 0x1EBF, 0x1EC1, 0x1EC3, 0x1EC5, 0x1EC7},  // 8: ê
    {0x00CA, 0x1EBE, 0x1EC0, 0x1EC2, 0x1EC4, 0x1EC6},  // 9: Ê
    {'i', 0x00ED, 0x00EC, 0x1EC9, 0x0129, 0x1ECB},  // 10: i
    {'I', 0x00CD, 0x00CC, 0x1EC8, 0x0128, 0x1ECA},  // 11: I
    {'o', 0x00F3, 0x00F2, 0x1ECF, 0x00F5, 0x1ECD},  // 12: o
    {'O', 0x00D3, 0x00D2, 0x1ECE, 0x00D5, 0x1ECC},  // 13: O
    {0x00F4, 0x1ED1, 0x1ED3, 0x1ED5, 0x1ED7, 0x1ED9},  // 14: ô
    {0x00D4, 0x1ED0, 0x1ED2, 0x1ED4, 0x1ED6, 0x1ED8},  // 15: Ô
    {0x01A1, 0x1EDB, 0x1EDD, 0x1EDF, 0x1EE1, 0x1EE3},  // 16: ơ
    {0x01A0, 0x1EDA, 0x1EDC, 0x1EDE, 0x1EE0, 0x1EE2},  // 17: Ơ
    {'u', 0x00FA, 0x00F9, 0x1EE7, 0x0169, 0x1EE5},  // 18: u
    {'U', 0x00DA, 0x00D9, 0x1EE6, 0x0168, 0x1EE4},  // 19: U
    {0x01B0, 0x1EE9, 0x1EEB, 0x1EED, 0x1EEF, 0x1EF1},  // 20: ư
    {0x01AF, 0x1EE8, 0x1EEA, 0x1EEC, 0x1EEE, 0x1EF0},  // 21: Ư
    {'y', 0x00FD, 0x1EF3, 0x1EF7, 0x1EF9, 0x1EF5},  // 22: y
    {'Y', 0x00DD, 0x1EF2, 0x1EF6, 0x1EF8, 0x1EF4},  // 23: Y
};

#define VOWEL_ROWS 24
#define BASE_A  0
#define BASE_AW 2
#define BASE_AA 4
#define BASE_E  6
#define BASE_EE 8
#define BASE_I  10
#define BASE_O  12
#define BASE_OO 14
#define BASE_OW 16
#define BASE_U  18
#define BASE_UW 20
#define BASE_Y  22

void telex_init(void) {}

void telex_reset(Word *word) {
    word->len = 0;
}

// Fast vowel lookup using cache
static int find_vowel_row(uint32_t ch) {
    // Quick ASCII check
    if (ch < 128) {
        switch (ch) {
            case 'a': return 0; case 'A': return 1;
            case 'e': return 6; case 'E': return 7;
            case 'i': return 10; case 'I': return 11;
            case 'o': return 12; case 'O': return 13;
            case 'u': return 18; case 'U': return 19;
            case 'y': return 22; case 'Y': return 23;
            default: return -1;
        }
    }
    // Full table search for Unicode
    for (int i = 0; i < VOWEL_ROWS; i++) {
        for (int j = 0; j < 6; j++) {
            if (vowel_table[i][j] == ch) return i;
        }
    }
    return -1;
}

static inline int get_tone(uint32_t ch) {
    int row = find_vowel_row(ch);
    if (row < 0) return 0;
    for (int j = 0; j < 6; j++) {
        if (vowel_table[row][j] == ch) return j;
    }
    return 0;
}

static inline bool is_vowel(uint32_t ch) {
    return find_vowel_row(ch) >= 0;
}

static inline int get_base_type(int row) {
    return (row / 2) * 2;
}

static inline bool is_upper_row(int row) {
    return (row % 2) == 1;
}

static inline uint32_t get_vowel(int base, bool upper, int tone) {
    int row = base + (upper ? 1 : 0);
    return (row >= 0 && row < VOWEL_ROWS) ? vowel_table[row][tone] : 0;
}

// Find all vowel positions
static int find_vowel_positions(const Word *word, int *pos, int max) {
    int count = 0;
    for (int i = 0; i < word->len && count < max; i++) {
        if (is_vowel(word->chars[i])) pos[count++] = i;
    }
    return count;
}

// Check for final consonant
static bool has_final_consonant(const Word *word, int after_pos) {
    for (int i = after_pos + 1; i < word->len; i++) {
        if (!is_vowel(word->chars[i])) return true;
    }
    return false;
}

// Smart tone position (Vietnamese rules + freedom typing)
static int find_tone_position(const Word *word) {
    int pos[MAX_WORD_LEN];
    int count = find_vowel_positions(word, pos, MAX_WORD_LEN);

    if (count == 0) return -1;
    if (count == 1) return pos[0];

    // Find vowel cluster
    int cs = pos[0], ce = pos[0];
    for (int i = 1; i < count; i++) {
        if (pos[i] == ce + 1) ce = pos[i];
    }
    int clen = ce - cs + 1;

    // Priority: ơ, ê get the tone
    for (int i = cs; i <= ce; i++) {
        int row = find_vowel_row(word->chars[i]);
        if (row >= 0) {
            int base = get_base_type(row);
            if (base == BASE_OW || base == BASE_EE) return i;
        }
    }

    bool final_cons = has_final_consonant(word, ce);

    if (clen == 2) {
        int r1 = find_vowel_row(word->chars[cs]);
        int r2 = find_vowel_row(word->chars[ce]);
        if (r1 >= 0 && r2 >= 0) {
            int b1 = get_base_type(r1), b2 = get_base_type(r2);
            // oa, oe, uy patterns -> second vowel
            if ((b1 == BASE_O || b1 == BASE_OO || b1 == BASE_OW) &&
                (b2 == BASE_A || b2 == BASE_AW || b2 == BASE_AA || b2 == BASE_E || b2 == BASE_EE))
                return ce;
            if ((b1 == BASE_U || b1 == BASE_UW) &&
                (b2 == BASE_Y || b2 == BASE_E || b2 == BASE_EE || b2 == BASE_OW || b2 == BASE_A))
                return ce;
        }
        return final_cons ? ce : cs;
    }

    if (clen >= 3) return cs + 1;
    return cs;
}

// Move existing tone to correct position (smart tone movement)
static void normalize_tone_position(Word *word) {
    int correct_pos = find_tone_position(word);
    if (correct_pos < 0) return;

    // Find current tone and its position
    int current_tone = 0;
    int current_pos = -1;

    for (int i = 0; i < word->len; i++) {
        int t = get_tone(word->chars[i]);
        if (t > 0) {
            current_tone = t;
            current_pos = i;
            break;
        }
    }

    // Move tone if needed
    if (current_tone > 0 && current_pos != correct_pos && current_pos >= 0) {
        // Remove tone from current position
        int row = find_vowel_row(word->chars[current_pos]);
        if (row >= 0) {
            word->chars[current_pos] = vowel_table[row][0];
        }

        // Add tone to correct position
        row = find_vowel_row(word->chars[correct_pos]);
        if (row >= 0) {
            word->chars[correct_pos] = vowel_table[row][current_tone];
        }
    }
}

// Apply tone (freedom typing: works from any position)
static bool apply_tone(Word *word, int tone) {
    int pos = find_tone_position(word);
    if (pos < 0) return false;

    int row = find_vowel_row(word->chars[pos]);
    if (row < 0) return false;

    int current = get_tone(word->chars[pos]);

    if (tone == 0) {
        if (current == 0) return false;
        word->chars[pos] = vowel_table[row][0];
        return true;
    }

    word->chars[pos] = vowel_table[row][current == tone ? 0 : tone];
    return true;
}

// Handle 'w' key
static bool handle_w(Word *word) {
    for (int i = word->len - 1; i >= 0; i--) {
        int row = find_vowel_row(word->chars[i]);
        if (row < 0) continue;

        int base = get_base_type(row);
        bool upper = is_upper_row(row);
        int tone = get_tone(word->chars[i]);
        int new_base = -1;

        if (base == BASE_A) new_base = BASE_AW;
        else if (base == BASE_AW) new_base = BASE_A;
        else if (base == BASE_O) new_base = BASE_OW;
        else if (base == BASE_OW) new_base = BASE_O;
        else if (base == BASE_U) new_base = BASE_UW;
        else if (base == BASE_UW) new_base = BASE_U;

        if (new_base >= 0) {
            word->chars[i] = get_vowel(new_base, upper, tone);
            normalize_tone_position(word);
            return true;
        }
    }
    return false;
}

// Handle aa, ee, oo
static bool handle_double_vowel(Word *word, char key) {
    if (word->len == 0) return false;

    uint32_t last = word->chars[word->len - 1];
    int row = find_vowel_row(last);
    if (row < 0) return false;

    int base = get_base_type(row);
    bool upper = is_upper_row(row);
    int tone = get_tone(last);
    char k = tolower(key);
    int new_base = -1;

    if (k == 'a' && base == BASE_A) new_base = BASE_AA;
    else if (k == 'a' && base == BASE_AA) new_base = BASE_A;
    else if (k == 'e' && base == BASE_E) new_base = BASE_EE;
    else if (k == 'e' && base == BASE_EE) new_base = BASE_E;
    else if (k == 'o' && base == BASE_O) new_base = BASE_OO;
    else if (k == 'o' && base == BASE_OO) new_base = BASE_O;

    if (new_base >= 0) {
        word->chars[word->len - 1] = get_vowel(new_base, upper, tone);
        normalize_tone_position(word);
        return true;
    }
    return false;
}

// Handle d -> đ
static bool handle_d(Word *word) {
    for (int i = word->len - 1; i >= 0; i--) {
        uint32_t ch = word->chars[i];
        if (ch == 'd') { word->chars[i] = 0x0111; return true; }
        if (ch == 'D') { word->chars[i] = 0x0110; return true; }
        if (ch == 0x0111) { word->chars[i] = 'd'; return true; }
        if (ch == 0x0110) { word->chars[i] = 'D'; return true; }
    }
    return false;
}

bool telex_process(Word *word, char key) {
    if (word->len >= MAX_WORD_LEN - 1) return false;

    char k = tolower(key);

    // Tone marks (freedom typing: apply to correct position automatically)
    switch (k) {
        case 's': if (apply_tone(word, 1)) return true; break;
        case 'f': if (apply_tone(word, 2)) return true; break;
        case 'r': if (apply_tone(word, 3)) return true; break;
        case 'x': if (apply_tone(word, 4)) return true; break;
        case 'j': if (apply_tone(word, 5)) return true; break;
        case 'z': if (apply_tone(word, 0)) return true; break;
    }

    // Vowel modifications
    switch (k) {
        case 'a': case 'e': case 'o':
            if (handle_double_vowel(word, key)) return true;
            break;
        case 'w':
            if (handle_w(word)) return true;
            break;
        case 'd':
            if (handle_d(word)) return true;
            break;
    }

    // No transformation happened
    return false;
}

// Public wrappers
void telex_normalize_tone(Word *word) {
    normalize_tone_position(word);
}

bool telex_is_vowel(uint32_t ch) {
    return is_vowel(ch);
}

int word_to_utf8(const Word *word, char *buf, int buf_size) {
    int pos = 0;
    for (int i = 0; i < word->len && pos < buf_size - 4; i++) {
        uint32_t cp = word->chars[i];
        if (cp < 0x80) {
            buf[pos++] = (char)cp;
        } else if (cp < 0x800) {
            buf[pos++] = (char)(0xC0 | (cp >> 6));
            buf[pos++] = (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            buf[pos++] = (char)(0xE0 | (cp >> 12));
            buf[pos++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            buf[pos++] = (char)(0x80 | (cp & 0x3F));
        } else {
            buf[pos++] = (char)(0xF0 | (cp >> 18));
            buf[pos++] = (char)(0x80 | ((cp >> 12) & 0x3F));
            buf[pos++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            buf[pos++] = (char)(0x80 | (cp & 0x3F));
        }
    }
    buf[pos] = '\0';
    return pos;
}
