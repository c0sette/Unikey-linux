#ifndef TELEX_H
#define TELEX_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_WORD_LEN 32

typedef struct {
    uint32_t chars[MAX_WORD_LEN];  // UTF-32 codepoints
    int len;
} Word;

// Initialize telex engine
void telex_init(void);

// Process a key press, returns true if word was modified
bool telex_process(Word *word, char key);

// Reset current word
void telex_reset(Word *word);

// Normalize tone position after adding a vowel
void telex_normalize_tone(Word *word);

// Check if character is a vowel
bool telex_is_vowel(uint32_t ch);

// Convert UTF-32 word to UTF-8 string
int word_to_utf8(const Word *word, char *buf, int buf_size);

#endif
