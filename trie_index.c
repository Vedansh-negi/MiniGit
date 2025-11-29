#include "trie_index.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static TrieNode* root = NULL;

static TrieNode* create_node() {
    TrieNode* node = (TrieNode*)calloc(1, sizeof(TrieNode));
    node->is_word_end = false;
    node->file_count = 0;
    return node;
}

static int char_to_index(char c) {
    return c - 'a';
}

void initialize_trie() {
    if (!root) {
        root = create_node();
    }
}

void trie_insert_word(const char* word, const char* filename) {
    if (!root) initialize_trie();

    TrieNode* current = root;
    for (int i = 0; word[i]; i++) {
        char c = word[i];
        if (c < 'a' || c > 'z') continue;

        int idx = char_to_index(c);
        if (!current->children[idx]) {
            current->children[idx] = create_node();
        }
        current = current->children[idx];
    }
    current->is_word_end = true;

    // Add filename if not already present
    for (int i = 0; i < current->file_count; i++) {
        if (strcmp(current->files[i].filename, filename) == 0) return;
    }
    if (current->file_count < MAX_FILES_PER_WORD) {
        strncpy(current->files[current->file_count].filename, filename, MAX_FILENAME_LENGTH - 1);
        current->files[current->file_count].filename[MAX_FILENAME_LENGTH - 1] = '\0';
        current->file_count++;
    }
}

void search_word_in_trie(const char* word) {
    if (!root) {
        printf("Index is empty.\n");
        return;
    }
    TrieNode* current = root;
    for (int i = 0; word[i]; i++) {
        char c = word[i];
        if (c < 'a' || c > 'z') {
            printf("Invalid query word (only a-z allowed).\n");
            return;
        }
        int idx = char_to_index(c);
        if (!current->children[idx]) {
            printf("No results found for \"%s\"\n", word);
            return;
        }
        current = current->children[idx];
    }
    if (current->is_word_end && current->file_count > 0) {
        printf("Files containing \"%s\":\n", word);
        for (int i = 0; i < current->file_count; i++) {
            printf("  %s\n", current->files[i].filename);
        }
    } else {
        printf("No results found for \"%s\"\n", word);
    }
}

static void free_trie_node(TrieNode* node) {
    if (!node) return;
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        free_trie_node(node->children[i]);
    }
    free(node);
}

void free_trie(TrieNode* node) {
    free_trie_node(node);
    root = NULL;
}