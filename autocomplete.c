/**
 * @file autocomplete.c
 * @brief Clean, file-based autocomplete system (NO preset suggestions)
 */

#include "autocomplete.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/* ================= GLOBAL CONTEXT ================= */

static autocomplete_context_t g_autocomplete_ctx = {0};

/* ================= HELPER FUNCTION DECLARATIONS ================= */

static trie_node_t* create_trie_node(void);
static void destroy_trie(trie_node_t *node);
static void insert_suggestion_into_trie(const char *suggestion, float score);
static int collect_suggestions_from_trie(trie_node_t *node,
                                         autocomplete_result_t *suggestions,
                                         int max_suggestions, int *count);
static int compare_suggestions(const void *a, const void *b);
static float calculate_suggestion_score(const char *suggestion,
                                        autocomplete_source_t source);

/* ================= INITIALIZATION ================= */

/**
 * @brief Initialize the autocomplete system (NO preset suggestions!)
 */
int init_autocomplete_system(void) {
    printf("Initializing autocomplete system...\n");

    g_autocomplete_ctx.root = create_trie_node();
    if (!g_autocomplete_ctx.root) {
        fprintf(stderr, "ERROR: Failed to create trie root!\n");
        return -1;
    }

    g_autocomplete_ctx.config.algorithm = AC_ALGORITHM_HYBRID;
    g_autocomplete_ctx.config.min_score_threshold = DEFAULT_SUGGESTION_THRESHOLD;
    g_autocomplete_ctx.config.max_suggestions = MAX_AUTOCOMPLETE_SUGGESTIONS;
    g_autocomplete_ctx.config.enable_fuzzy_matching = false; // disabled
    g_autocomplete_ctx.config.enable_trending_boost = false;
    g_autocomplete_ctx.config.enable_personalization = false;

    g_autocomplete_ctx.total_suggestions = 0;

    printf("Autocomplete system initialized (NO built-in suggestions)\n");
    return 0;
}

/**
 * @brief Cleanup resources
 */
void cleanup_autocomplete_system(void) {
    if (g_autocomplete_ctx.root) {
        destroy_trie(g_autocomplete_ctx.root);
        g_autocomplete_ctx.root = NULL;
    }
    g_autocomplete_ctx.total_suggestions = 0;
    printf("Autocomplete system cleanup completed.\n");
}

/* ================= INSERT SUGGESTIONS ================= */

/**
 * @brief Add suggestion from real file
 */
int add_autocomplete_suggestion(const char *suggestion, float score, autocomplete_source_t source) {
    if (!suggestion || strlen(suggestion) == 0)
        return -1;

    float final_score = score > 0 ? score : calculate_suggestion_score(suggestion, source);

    printf("ADDING TO AUTOCOMPLETE: '%s'\n", suggestion);

    insert_suggestion_into_trie(suggestion, final_score);
    g_autocomplete_ctx.total_suggestions++;

    return 0;
}

/* ================= SEARCH ================= */

/**
 * @brief Main API to get suggestions
 */
int get_autocomplete_suggestions(const char *query,
                                 autocomplete_result_t *suggestions,
                                 int max_suggestions) {

    if (!query || !suggestions) return 0;

    /* Normalize query */
    char normalized[MAX_QUERY_LENGTH];
    strncpy(normalized, query, MAX_QUERY_LENGTH - 1);
    normalized[MAX_QUERY_LENGTH - 1] = '\0';

    for (int i = 0; normalized[i]; i++)
        normalized[i] = tolower(normalized[i]);

    /* Move through trie to prefix */
    trie_node_t *current = g_autocomplete_ctx.root;
    for (int i = 0; normalized[i]; i++) {
        int c = (unsigned char)normalized[i];
        if (c >= 128 || !current->children[c])
            return 0; // prefix not found
        current = current->children[c];
    }

    /* Collect */
    int count = 0;
    collect_suggestions_from_trie(current, suggestions, max_suggestions, &count);

    /* Sort */
    qsort(suggestions, count, sizeof(autocomplete_result_t), compare_suggestions);

    return count;
}

/* ================= PREFIX COLLECTION ================= */

static int collect_suggestions_from_trie(trie_node_t *node,
                                         autocomplete_result_t *suggestions,
                                         int max_suggestions,
                                         int *count) {

    if (!node || *count >= max_suggestions)
        return 0;

    if (node->is_end_of_word && node->suggestion) {
        strncpy(suggestions[*count].suggestion, node->suggestion,
                MAX_SUGGESTION_LENGTH - 1);
        suggestions[*count].suggestion[MAX_SUGGESTION_LENGTH - 1] = '\0';

        suggestions[*count].score = node->score;
        suggestions[*count].frequency = node->frequency;
        suggestions[*count].last_used = node->last_used;
        suggestions[*count].is_trending = false;

        (*count)++;
    }

    for (int i = 0; i < 128 && *count < max_suggestions; i++) {
        if (node->children[i])
            collect_suggestions_from_trie(node->children[i],
                                          suggestions,
                                          max_suggestions, count);
    }

    return *count;
}

/* ================= TRIE INSERTION ================= */

/**
 * @brief Insert suggestion into trie (letters & numbers ONLY)
 */
static void insert_suggestion_into_trie(const char *suggestion, float score) {
    trie_node_t *current = g_autocomplete_ctx.root;

    for (int i = 0; suggestion[i]; i++) {
        int c = tolower(suggestion[i]);

        /* Skip non-alphanumeric characters */
        if (!isalnum(c))
            continue;

        if (!current->children[c])
            current->children[c] = create_trie_node();

        current = current->children[c];
    }

    current->is_end_of_word = true;

    if (current->suggestion)
        free(current->suggestion);

    current->suggestion = strdup(suggestion);
    current->score = score;
    current->frequency++;
    current->last_used = time(NULL);
}

/* ================= TRIE CREATION / DESTRUCTION ================= */

static trie_node_t* create_trie_node(void) {
    trie_node_t *node = (trie_node_t*)calloc(1, sizeof(trie_node_t));
    return node;
}

static void destroy_trie(trie_node_t *node) {
    if (!node) return;

    for (int i = 0; i < 128; i++)
        if (node->children[i])
            destroy_trie(node->children[i]);

    if (node->suggestion)
        free(node->suggestion);

    free(node);
}

/* ================= SCORING ================= */

static float calculate_suggestion_score(const char *suggestion,
                                        autocomplete_source_t source) {

    switch (source) {
        case AC_SOURCE_DOCUMENT_TITLES:
            return 0.6;
        default:
            return 0.5;
    }
}

/* ================= SORT ================= */

static int compare_suggestions(const void *a, const void *b) {
    const autocomplete_result_t *A = a;
    const autocomplete_result_t *B = b;

    if (A->score > B->score) return -1;
    if (A->score < B->score) return 1;
    return 0;
}
