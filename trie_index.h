#ifndef TRIE_INDEX_H
#define TRIE_INDEX_H

#include<stdbool.h>

#define ALPHABET_SIZE 26
#define MAX_FILES_PER_WORD 20
#define MAX_FILENAME_LENGTH 256

typedef struct FileNode {
    char filename[MAX_FILENAME_LENGTH];
} FileNode;

typedef struct TrieNode {
    struct TrieNode* children[ALPHABET_SIZE];
    bool is_word_end;
    FileNode files[MAX_FILES_PER_WORD];
    int file_count;
} TrieNode;

void trie_insert_word(const char* word, const char* filename);
void search_word_in_trie(const char* word);
void free_trie(TrieNode* node);
void initialize_trie();

#endif /* TRIE_INDEX_H */