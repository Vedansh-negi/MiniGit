/**
 * @file autocomplete.h
 * @brief Autocomplete suggestions system
 */

#ifndef AUTOCOMPLETE_H
#define AUTOCOMPLETE_H

#include "search_engine.h"
#include <stdbool.h>

/* Autocomplete algorithm types */
typedef enum {
    AC_ALGORITHM_PREFIX_MATCH,      // Prefix matching
    AC_ALGORITHM_FUZZY_MATCH,       // Fuzzy matching
    AC_ALGORITHM_CONTEXTUAL,        // Context-aware
    AC_ALGORITHM_ML_BASED,          // ML-based
    AC_ALGORITHM_HYBRID             // Combined methods
} autocomplete_algorithm_t;

/* Suggestion source types */
typedef enum {
    AC_SOURCE_QUERY_HISTORY,        // User history
    AC_SOURCE_POPULAR_QUERIES,      // Trending
    AC_SOURCE_DOCUMENT_TITLES,      // Document titles
    AC_SOURCE_CONTEXTUAL,           // Context-based
    AC_SOURCE_PERSONALIZED          // Personalized
} autocomplete_source_t;

/* Configuration */
typedef struct {
    autocomplete_algorithm_t algorithm;
    float min_score_threshold;
    int max_suggestions;
    bool enable_fuzzy_matching;
    bool enable_trending_boost;
    bool enable_personalization;
    float trending_weight;
    float history_weight;
    float popularity_weight;
} autocomplete_config_t;

/* Trie node for prefix matching */
typedef struct trie_node {
    struct trie_node *children[128]; // ASCII children
    char *suggestion;
    float score;
    int frequency;
    bool is_end_of_word;
    long last_used;
} trie_node_t;

/* Autocomplete system context */
typedef struct {
    trie_node_t *root;
    autocomplete_config_t config;
    int total_suggestions;
    long last_update;
} autocomplete_context_t;

/* Initialization and cleanup */
int init_autocomplete_system(void);
void cleanup_autocomplete_system(void);

/* Suggestion management */
int add_autocomplete_suggestion(const char *suggestion, float score, autocomplete_source_t source);
int update_suggestion_score(const char *suggestion, bool user_selected);
int clear_autocomplete_data(void);

/* Data loading and saving */
int load_suggestions_from_history(const char *history_file);
int load_trending_suggestions(const char *trending_file);
int save_autocomplete_data(const char *filename);
int load_autocomplete_data(const char *filename);

/* Suggestion retrieval */
int get_autocomplete_suggestions(const char *query, autocomplete_result_t *suggestions, int max_suggestions);

/* Algorithm-specific retrieval */
int get_prefix_suggestions(const char *prefix, autocomplete_result_t *suggestions, int max_suggestions);
int get_fuzzy_suggestions(const char *query, autocomplete_result_t *suggestions, int max_suggestions);
int get_contextual_suggestions(const char *query, const char *context, autocomplete_result_t *suggestions, int max_suggestions);

/* Utility functions */
int calculate_edit_distance(const char *str1, const char *str2);
bool is_suggestion_trending(const char *suggestion, long time_window);

/* Statistics and configuration */
void get_autocomplete_stats(int *total_suggestions, float *avg_score, float *cache_hit_rate);
int configure_autocomplete(const autocomplete_config_t *config);

#endif /* AUTOCOMPLETE_H */
