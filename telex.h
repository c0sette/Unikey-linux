#ifndef TELEX_H
#define TELEX_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_WORD_LEN 32
#define MAX_HISTORY 64

// Transformation types for history tracking
typedef enum {
    TRANS_APPEND,       // Added a character
    TRANS_TONE,         // Added/changed tone mark
    TRANS_MARK,         // Added/changed vowel mark (ă, â, ê, ô, ơ, ư)
    TRANS_D_STROKE,     // d -> đ
    TRANS_UNDO          // Undid a transformation
} TransformType;

// Single transformation record
typedef struct {
    TransformType type;
    int target_pos;     // Position affected
    uint32_t old_char;  // Character before transformation
    uint32_t new_char;  // Character after transformation
    char key;           // Key that triggered this
} Transformation;

// Word with transformation history
typedef struct {
    uint32_t chars[MAX_WORD_LEN];  // UTF-32 codepoints
    int len;
    int cancelled_tone;  // Tone that was cancelled (1-5), 0 = none

    // Transformation history for smart undo
    Transformation history[MAX_HISTORY];
    int history_len;
} Word;

// CVC (Consonant-Vowel-Consonant) info for spell checking
typedef struct {
    int fc_start, fc_end;   // First consonant range (inclusive)
    int vo_start, vo_end;   // Vowel cluster range
    int lc_start, lc_end;   // Last consonant range
    bool has_fc, has_vo, has_lc;
} CVCInfo;

// Initialize telex engine
void telex_init(void);

// Process a key press
// Returns: 0 = no change, 1 = transformed, 2 = undo (double press, add key char)
int telex_process(Word *word, char key);

// Reset current word
void telex_reset(Word *word);

// Normalize tone position after adding a vowel
void telex_normalize_tone(Word *word);

// Check if character is a vowel
bool telex_is_vowel(uint32_t ch);

// Check if current word forms a valid Vietnamese syllable
bool telex_is_valid_syllable(const Word *word);

// Extract CVC components from word
bool telex_extract_cvc(const Word *word, CVCInfo *cvc);

// Check if a tone is valid for the current word ending
bool telex_is_valid_tone(const Word *word, int tone);

// Convert UTF-32 word to UTF-8 string
int word_to_utf8(const Word *word, char *buf, int buf_size);

#endif
