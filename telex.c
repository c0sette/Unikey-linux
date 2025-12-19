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

// ============================================================================
// CONSONANT DEFINITIONS FOR CVC EXTRACTION
// ============================================================================

// Reserved for future use: Check if character is a consonant
// static bool is_consonant(uint32_t ch) { ... }

// Reserved for future use: Check if character is đ/Đ
// static bool is_d_stroke(uint32_t ch) { ... }

void telex_init(void) {}

void telex_reset(Word *word) {
    word->len = 0;
    word->cancelled_tone = 0;
    word->history_len = 0;
}

// Record a transformation in history
static void record_transform(Word *word, TransformType type, int pos,
                            uint32_t old_ch, uint32_t new_ch, char key) {
    if (word->history_len >= MAX_HISTORY) return;
    Transformation *t = &word->history[word->history_len++];
    t->type = type;
    t->target_pos = pos;
    t->old_char = old_ch;
    t->new_char = new_ch;
    t->key = key;
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

// Get the base (toneless) form of a vowel
static inline uint32_t get_base_vowel(uint32_t ch) {
    int row = find_vowel_row(ch);
    if (row < 0) return ch;
    return vowel_table[row][0];
}

// ============================================================================
// CVC EXTRACTION (from bamboo-core)
// ============================================================================

bool telex_extract_cvc(const Word *word, CVCInfo *cvc) {
    memset(cvc, 0, sizeof(CVCInfo));
    cvc->fc_start = cvc->fc_end = -1;
    cvc->vo_start = cvc->vo_end = -1;
    cvc->lc_start = cvc->lc_end = -1;

    if (word->len == 0) return false;

    int i = 0;

    // Find first consonant cluster
    while (i < word->len && !is_vowel(word->chars[i])) {
        if (cvc->fc_start < 0) cvc->fc_start = i;
        cvc->fc_end = i;
        cvc->has_fc = true;
        i++;
    }

    // Find vowel cluster
    while (i < word->len && is_vowel(word->chars[i])) {
        if (cvc->vo_start < 0) cvc->vo_start = i;
        cvc->vo_end = i;
        cvc->has_vo = true;
        i++;
    }

    // Find last consonant cluster
    while (i < word->len) {
        if (cvc->lc_start < 0) cvc->lc_start = i;
        cvc->lc_end = i;
        cvc->has_lc = true;
        i++;
    }

    // Special case: "gi" and "qu" are considered first consonants
    // gi + vowel -> gi is consonant
    // qu + vowel -> qu is consonant
    if (cvc->has_fc && cvc->has_vo) {
        int fc_len = cvc->fc_end - cvc->fc_start + 1;
        uint32_t first = word->chars[cvc->fc_start];
        uint32_t second = (fc_len >= 1 && cvc->vo_start >= 0) ? word->chars[cvc->vo_start] : 0;

        // "g" + "i" + more vowels -> "gi" is consonant
        if ((first == 'g' || first == 'G') && (second == 'i' || second == 'I')) {
            int vo_len = cvc->vo_end - cvc->vo_start + 1;
            if (vo_len > 1) {
                cvc->fc_end = cvc->vo_start;
                cvc->vo_start++;
            }
        }
        // "q" + "u" -> "qu" is consonant
        if ((first == 'q' || first == 'Q') && (second == 'u' || second == 'U')) {
            int vo_len = cvc->vo_end - cvc->vo_start + 1;
            if (vo_len > 1) {
                cvc->fc_end = cvc->vo_start;
                cvc->vo_start++;
            }
        }
    }

    return cvc->has_vo;  // At minimum, need a vowel for valid syllable
}

// ============================================================================
// TONE VALIDATION (c/p/t/ch only sắc/nặng)
// ============================================================================

// Check if last consonant restricts tones to only sắc (1) and nặng (5)
static bool has_restricted_ending(const Word *word, const CVCInfo *cvc) {
    if (!cvc->has_lc) return false;

    int lc_len = cvc->lc_end - cvc->lc_start + 1;
    uint32_t c1 = word->chars[cvc->lc_start];
    uint32_t c2 = (lc_len >= 2) ? word->chars[cvc->lc_start + 1] : 0;

    // Convert to lowercase for comparison
    if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
    if (c2 >= 'A' && c2 <= 'Z') c2 += 32;

    // Single consonants: c, k, p, t
    if (lc_len == 1) {
        if (c1 == 'c' || c1 == 'k' || c1 == 'p' || c1 == 't') {
            return true;
        }
    }
    // Double consonant: ch
    if (lc_len == 2 && c1 == 'c' && c2 == 'h') {
        return true;
    }

    return false;
}

bool telex_is_valid_tone(const Word *word, int tone) {
    // Tones: 0=none, 1=sắc, 2=huyền, 3=hỏi, 4=ngã, 5=nặng
    // Restricted endings only allow: 0 (none), 1 (sắc), 5 (nặng)
    if (tone == 0 || tone == 1 || tone == 5) return true;

    CVCInfo cvc;
    if (!telex_extract_cvc(word, &cvc)) return true;

    if (has_restricted_ending(word, &cvc)) {
        return false;  // huyền(2), hỏi(3), ngã(4) not allowed
    }
    return true;
}

// ============================================================================
// SPELL CHECKING (simplified from bamboo-core)
// ============================================================================

// Valid first consonant patterns
static const char* valid_first_consonants[] = {
    "b", "c", "ch", "d", "g", "gh", "gi", "h", "k", "kh",
    "l", "m", "n", "ng", "ngh", "nh", "p", "ph", "qu", "r",
    "s", "t", "th", "tr", "v", "x", NULL
};

// Valid last consonant patterns
static const char* valid_last_consonants[] = {
    "c", "ch", "m", "n", "ng", "nh", "p", "t", NULL
};

// Helper: convert word segment to lowercase string
static int segment_to_str(const Word *word, int start, int end, char *buf, int bufsize) {
    int pos = 0;
    for (int i = start; i <= end && pos < bufsize - 4; i++) {
        uint32_t ch = word->chars[i];
        // Get base form for vowels (remove tones/marks for matching)
        if (is_vowel(ch)) {
            int row = find_vowel_row(ch);
            if (row >= 0) {
                int base = get_base_type(row);
                // Map to simple vowel
                switch (base) {
                    case BASE_A: case BASE_AW: case BASE_AA: ch = 'a'; break;
                    case BASE_E: case BASE_EE: ch = 'e'; break;
                    case BASE_I: ch = 'i'; break;
                    case BASE_O: case BASE_OO: case BASE_OW: ch = 'o'; break;
                    case BASE_U: case BASE_UW: ch = 'u'; break;
                    case BASE_Y: ch = 'y'; break;
                }
            }
        }
        // Convert to lowercase
        if (ch >= 'A' && ch <= 'Z') ch += 32;
        if (ch == 0x0110) ch = 0x0111;  // Đ -> đ

        // UTF-8 encode
        if (ch < 0x80) {
            buf[pos++] = (char)ch;
        } else if (ch < 0x800) {
            buf[pos++] = (char)(0xC0 | (ch >> 6));
            buf[pos++] = (char)(0x80 | (ch & 0x3F));
        } else {
            buf[pos++] = (char)(0xE0 | (ch >> 12));
            buf[pos++] = (char)(0x80 | ((ch >> 6) & 0x3F));
            buf[pos++] = (char)(0x80 | (ch & 0x3F));
        }
    }
    buf[pos] = '\0';
    return pos;
}

bool telex_is_valid_syllable(const Word *word) {
    if (word->len == 0) return true;

    CVCInfo cvc;
    if (!telex_extract_cvc(word, &cvc)) {
        // No vowel - could be incomplete, allow it
        return true;
    }

    char buf[32];

    // Validate first consonant if present
    if (cvc.has_fc) {
        segment_to_str(word, cvc.fc_start, cvc.fc_end, buf, sizeof(buf));
        bool valid = false;
        for (int i = 0; valid_first_consonants[i]; i++) {
            if (strcmp(buf, valid_first_consonants[i]) == 0) {
                valid = true;
                break;
            }
        }
        // Also allow đ
        if (!valid && (buf[0] == (char)0xc4 || strcmp(buf, "d") == 0)) {
            valid = true;  // đ encoded as UTF-8 or 'd'
        }
        if (!valid && strlen(buf) > 0) {
            return false;
        }
    }

    // Validate last consonant if present
    if (cvc.has_lc) {
        segment_to_str(word, cvc.lc_start, cvc.lc_end, buf, sizeof(buf));
        bool valid = false;
        for (int i = 0; valid_last_consonants[i]; i++) {
            if (strcmp(buf, valid_last_consonants[i]) == 0) {
                valid = true;
                break;
            }
        }
        if (!valid && strlen(buf) > 0) {
            return false;
        }
    }

    // Validate tone for restricted endings
    for (int i = 0; i < word->len; i++) {
        int tone = get_tone(word->chars[i]);
        if (tone > 0 && !telex_is_valid_tone(word, tone)) {
            return false;
        }
    }

    return true;
}

// ============================================================================
// VOWEL POSITION FINDING
// ============================================================================

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

// ============================================================================
// TONE APPLICATION
// ============================================================================

// Apply tone (freedom typing: works from any position)
// Returns: 0 = no change, 1 = tone applied, 2 = tone removed (double press)
static int apply_tone_ex(Word *word, int tone, char key) {
    int pos = find_tone_position(word);
    if (pos < 0) return 0;

    int row = find_vowel_row(word->chars[pos]);
    if (row < 0) return 0;

    int current = get_tone(word->chars[pos]);

    // Validate tone for restricted endings (c/p/t/ch)
    if (tone > 0 && !telex_is_valid_tone(word, tone)) {
        return 0;  // Invalid tone for this ending
    }

    if (tone == 0) {
        if (current == 0) return 0;
        uint32_t old = word->chars[pos];
        word->chars[pos] = vowel_table[row][0];
        record_transform(word, TRANS_TONE, pos, old, word->chars[pos], key);
        return 1;
    }

    // Double press same tone key: remove tone and return special code
    if (current == tone) {
        uint32_t old = word->chars[pos];
        word->chars[pos] = vowel_table[row][0];
        record_transform(word, TRANS_UNDO, pos, old, word->chars[pos], key);
        return 2;  // Signal: tone removed, add the key char
    }

    uint32_t old = word->chars[pos];
    word->chars[pos] = vowel_table[row][tone];
    record_transform(word, TRANS_TONE, pos, old, word->chars[pos], key);
    return 1;
}

// ============================================================================
// VOWEL MARK HANDLERS
// ============================================================================

// Handle 'w' key with UOW shortcut (from bamboo-core)
static bool handle_w(Word *word) {
    // UOW shortcut: uo + w -> ươ, uO + w -> ưƠ
    if (word->len >= 2) {
        int last = word->len - 1;
        int prev = word->len - 2;
        uint32_t c1 = word->chars[prev];
        uint32_t c2 = word->chars[last];

        int r1 = find_vowel_row(c1);
        int r2 = find_vowel_row(c2);

        if (r1 >= 0 && r2 >= 0) {
            int b1 = get_base_type(r1);
            int b2 = get_base_type(r2);
            bool u1 = is_upper_row(r1);
            bool u2 = is_upper_row(r2);
            int t1 = get_tone(c1);
            int t2 = get_tone(c2);

            // u + o -> ư + ơ (UOW shortcut)
            if ((b1 == BASE_U) && (b2 == BASE_O)) {
                uint32_t old1 = word->chars[prev];
                uint32_t old2 = word->chars[last];
                word->chars[prev] = get_vowel(BASE_UW, u1, t1);
                word->chars[last] = get_vowel(BASE_OW, u2, t2);
                record_transform(word, TRANS_MARK, prev, old1, word->chars[prev], 'w');
                record_transform(word, TRANS_MARK, last, old2, word->chars[last], 'w');
                normalize_tone_position(word);
                return true;
            }

            // ư + o -> ư + ơ
            if ((b1 == BASE_UW) && (b2 == BASE_O)) {
                uint32_t old = word->chars[last];
                word->chars[last] = get_vowel(BASE_OW, u2, t2);
                record_transform(word, TRANS_MARK, last, old, word->chars[last], 'w');
                normalize_tone_position(word);
                return true;
            }

            // u + ơ -> ư + ơ
            if ((b1 == BASE_U) && (b2 == BASE_OW)) {
                uint32_t old = word->chars[prev];
                word->chars[prev] = get_vowel(BASE_UW, u1, t1);
                record_transform(word, TRANS_MARK, prev, old, word->chars[prev], 'w');
                normalize_tone_position(word);
                return true;
            }

            // ươ + w -> undo back to uo
            if ((b1 == BASE_UW) && (b2 == BASE_OW)) {
                uint32_t old1 = word->chars[prev];
                uint32_t old2 = word->chars[last];
                word->chars[prev] = get_vowel(BASE_U, u1, t1);
                word->chars[last] = get_vowel(BASE_O, u2, t2);
                record_transform(word, TRANS_UNDO, prev, old1, word->chars[prev], 'w');
                record_transform(word, TRANS_UNDO, last, old2, word->chars[last], 'w');
                normalize_tone_position(word);
                return true;
            }
        }
    }

    // Standard w handling: toggle ă/ư/ơ on single vowels
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
            uint32_t old = word->chars[i];
            word->chars[i] = get_vowel(new_base, upper, tone);
            record_transform(word, TRANS_MARK, i, old, word->chars[i], 'w');
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
        uint32_t old = word->chars[word->len - 1];
        word->chars[word->len - 1] = get_vowel(new_base, upper, tone);
        record_transform(word, TRANS_MARK, word->len - 1, old, word->chars[word->len - 1], key);
        normalize_tone_position(word);
        return true;
    }
    return false;
}

// Handle d -> đ
static bool handle_d(Word *word) {
    for (int i = word->len - 1; i >= 0; i--) {
        uint32_t ch = word->chars[i];
        uint32_t new_ch = 0;

        if (ch == 'd') new_ch = 0x0111;
        else if (ch == 'D') new_ch = 0x0110;
        else if (ch == 0x0111) new_ch = 'd';
        else if (ch == 0x0110) new_ch = 'D';

        if (new_ch) {
            record_transform(word, TRANS_D_STROKE, i, ch, new_ch, 'd');
            word->chars[i] = new_ch;
            return true;
        }
    }
    return false;
}

// ============================================================================
// MAIN PROCESS FUNCTION
// ============================================================================

// Returns: 0 = no change, 1 = transformed, 2 = undo (double press, add key char)
int telex_process(Word *word, char key) {
    if (word->len >= MAX_WORD_LEN - 1) return 0;

    char k = tolower(key);
    int result;
    int tone_for_key = 0;

    // Map key to tone
    switch (k) {
        case 's': tone_for_key = 1; break;
        case 'f': tone_for_key = 2; break;
        case 'r': tone_for_key = 3; break;
        case 'x': tone_for_key = 4; break;
        case 'j': tone_for_key = 5; break;
    }

    // If this tone was previously cancelled, don't apply it again - just add the char
    if (tone_for_key > 0 && word->cancelled_tone == tone_for_key) {
        return 0;  // Signal: no transformation, keyboard.c will add char normally
    }

    // Tone marks (freedom typing: apply to correct position automatically)
    switch (k) {
        case 's': result = apply_tone_ex(word, 1, key); break;
        case 'f': result = apply_tone_ex(word, 2, key); break;
        case 'r': result = apply_tone_ex(word, 3, key); break;
        case 'x': result = apply_tone_ex(word, 4, key); break;
        case 'j': result = apply_tone_ex(word, 5, key); break;
        case 'z': result = apply_tone_ex(word, 0, key); break;
        default: result = 0; break;
    }

    if (result == 2) {
        // Double press: mark this tone as cancelled
        word->cancelled_tone = tone_for_key;
        return 2;
    } else if (result == 1) {
        // Tone applied: clear any previous cancellation
        word->cancelled_tone = 0;
        return 1;
    }

    // Vowel modifications
    switch (k) {
        case 'a': case 'e': case 'o':
            if (handle_double_vowel(word, key)) return 1;
            break;
        case 'w':
            if (handle_w(word)) return 1;
            break;
        case 'd':
            if (handle_d(word)) return 1;
            break;
    }

    // No transformation happened
    return 0;
}

// ============================================================================
// PUBLIC WRAPPERS
// ============================================================================

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
