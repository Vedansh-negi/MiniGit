/**
 * @file ranking.c
 * @brief Implementation of search result ranking system
 * @author Autocomplete and Ranking Specialist
 * @version 1.0
 * 
 * This module implements intelligent ranking of search results using various
 * algorithms including TF-IDF, BM25, authority scoring, and machine learning.
 * Core specialization area (30% contribution focus).
 */

#include "ranking.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <ctype.h>

/* Global ranking context */
static ranking_config_t g_ranking_config = {0};
static ranking_stats_t g_ranking_stats = {0};
static bool g_ranking_initialized = false;

/* Internal helper functions */
static int compare_search_results(const void *a, const void *b);
static float calculate_combined_score(const char *query, const search_result_t *document, 
                                     const document_features_t *features, const query_context_t *context);
static int tokenize_query(const char *query, char tokens[][64], int max_tokens);
static float calculate_term_frequency(const char *term, const char *text);
static float calculate_inverse_document_frequency(const char *term, int total_documents);

/**
 * @brief Initialize the ranking system
 */
int init_ranking_system(void) {
    printf("Initializing ranking system...\n");
    
    if (g_ranking_initialized) {
        printf("Ranking system already initialized\n");
        return 0;
    }
    
    // Set default configuration
    g_ranking_config.algorithm = RANK_ALGORITHM_HYBRID;
    g_ranking_config.relevance_weight = 0.4;
    g_ranking_config.authority_weight = 0.25;
    g_ranking_config.freshness_weight = 0.15;
    g_ranking_config.popularity_weight = 0.15;
    g_ranking_config.personalization_weight = 0.05;
    g_ranking_config.enable_query_expansion = true;
    g_ranking_config.enable_semantic_matching = true;
    g_ranking_config.min_relevance_threshold = DEFAULT_RELEVANCE_THRESHOLD;
    
    // Initialize statistics
    memset(&g_ranking_stats, 0, sizeof(ranking_stats_t));
    g_ranking_stats.last_model_update = time(NULL);
    
    g_ranking_initialized = true;
    printf("Ranking system initialized successfully\n");
    return 0;
}

/**
 * @brief Cleanup ranking system resources
 */
void cleanup_ranking_system(void) {
    if (!g_ranking_initialized) {
        return;
    }
    
    // Reset all state
    memset(&g_ranking_config, 0, sizeof(ranking_config_t));
    memset(&g_ranking_stats, 0, sizeof(ranking_stats_t));
    g_ranking_initialized = false;
    
    printf("Ranking system cleanup completed\n");
}

/**
 * @brief Rank search results based on query and configuration
 */
int rank_search_results(const char *query, search_result_t *documents, 
                       int num_documents, const query_context_t *context) {
    if (!query || !documents || num_documents <= 0) {
        return -1;
    }
    
    if (!g_ranking_initialized) {
        fprintf(stderr, "Error: Ranking system not initialized\n");
        return -1;
    }
    
    clock_t start_time = clock();
    
    // Calculate relevance scores for all documents
    for (int i = 0; i < num_documents; i++) {
        document_features_t features = {0};
        
        // Extract features from document
        extract_document_features(&documents[i], query, &features);
        
        // Calculate combined score
        documents[i].relevance_score = calculate_combined_score(query, &documents[i], &features, context);
        
        // Apply minimum threshold
        if (documents[i].relevance_score < g_ranking_config.min_relevance_threshold) {
            documents[i].relevance_score = 0.0;
        }
    }
    
    // Sort documents by relevance score (descending)
    qsort(documents, num_documents, sizeof(search_result_t), compare_search_results);
    
    // Update statistics
    clock_t end_time = clock();
    double ranking_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC * 1000.0;
    
    g_ranking_stats.total_documents_ranked += num_documents;
    g_ranking_stats.queries_processed++;
    g_ranking_stats.avg_ranking_time = (g_ranking_stats.avg_ranking_time * (g_ranking_stats.queries_processed - 1) + ranking_time) / g_ranking_stats.queries_processed;
    
    return 0;
}

/**
 * @brief Calculate TF-IDF score
 */
float calculate_tf_idf_score(const char *query, const search_result_t *document, int total_documents) {
    if (!query || !document || total_documents <= 0) {
        return 0.0;
    }
    
    char tokens[32][64];
    int num_tokens = tokenize_query(query, tokens, 32);
    float total_score = 0.0;
    
    for (int i = 0; i < num_tokens; i++) {
        float tf = calculate_term_frequency(tokens[i], document->title) * 2.0 +  // Title has 2x weight
                  calculate_term_frequency(tokens[i], document->description);
        
        float idf = calculate_inverse_document_frequency(tokens[i], total_documents);
        
        total_score += tf * idf;
    }
    
    return total_score / num_tokens;
}

/**
 * @brief Calculate BM25 score
 */
float calculate_bm25_score(const char *query, const search_result_t *document, 
                          float avg_doc_length, int total_documents) {
    if (!query || !document) {
        return 0.0;
    }
    
    const float k1 = 1.2;  // Term frequency saturation parameter
    const float b = 0.75;  // Field length normalization parameter
    
    char tokens[32][64];
    int num_tokens = tokenize_query(query, tokens, 32);
    float total_score = 0.0;
    
    // Estimate document length (title + description)
    float doc_length = strlen(document->title) + strlen(document->description);
    
    for (int i = 0; i < num_tokens; i++) {
        float tf = calculate_term_frequency(tokens[i], document->title) +
                  calculate_term_frequency(tokens[i], document->description);
        
        float idf = calculate_inverse_document_frequency(tokens[i], total_documents);
        
        float numerator = tf * (k1 + 1);
        float denominator = tf + k1 * (1 - b + b * (doc_length / avg_doc_length));
        
        total_score += idf * (numerator / denominator);
    }
    
    return total_score;
}

/**
 * @brief Calculate authority score
 */
float calculate_authority_score(int document_id) {
    // Simplified PageRank-style scoring
    // In a real implementation, this would be based on link analysis
    float base_score = 0.5;
    
    // Simulate some variation based on document_id
    int hash = document_id % 100;
    if (hash < 10) {
        base_score = 0.9;  // High authority
    } else if (hash < 30) {
        base_score = 0.7;  // Medium authority
    } else if (hash < 70) {
        base_score = 0.5;  // Average authority
    } else {
        base_score = 0.3;  // Low authority
    }
    
    return base_score;
}

/**
 * @brief Calculate freshness score based on document age
 */
float calculate_freshness_score(long document_timestamp, long current_time) {
    if (document_timestamp <= 0 || current_time <= 0) {
        return 0.5; // Default score if timestamps are invalid
    }
    
    long age_seconds = current_time - document_timestamp;
    
    // Freshness decay function - exponential decay over time
    const long ONE_DAY = 24 * 60 * 60;
    const long ONE_WEEK = 7 * ONE_DAY;
    const long ONE_MONTH = 30 * ONE_DAY;
    
    if (age_seconds < ONE_DAY) {
        return 1.0;  // Very fresh
    } else if (age_seconds < ONE_WEEK) {
        return 0.8;  // Recent
    } else if (age_seconds < ONE_MONTH) {
        return 0.6;  // Moderately fresh
    } else {
        return 0.3;  // Old content
    }
}

/**
 * @brief Calculate popularity score
 */
float calculate_popularity_score(int click_count, int impression_count, float bounce_rate) {
    if (impression_count <= 0) {
        return 0.0;
    }
    
    // Calculate click-through rate
    float ctr = (float)click_count / impression_count;
    
    // Adjust for bounce rate (lower bounce rate is better)
    float engagement_factor = 1.0 - bounce_rate;
    
    // Combine CTR and engagement
    float popularity = ctr * engagement_factor;
    
    // Normalize to 0-1 range
    return popularity > 1.0 ? 1.0 : popularity;
}

/**
 * @brief Extract features from document for ranking
 */
int extract_document_features(const search_result_t *document, const char *query, 
                             document_features_t *features) {
    if (!document || !query || !features) {
        return -1;
    }
    
    memset(features, 0, sizeof(document_features_t));
    
    features->document_id = document->document_id;
    features->tf_idf_score = calculate_tf_idf_score(query, document, 10000); // Assume 10k docs
    features->bm25_score = calculate_bm25_score(query, document, 500.0, 10000); // Avg doc length 500
    features->pagerank_score = calculate_authority_score(document->document_id);
    features->freshness_score = calculate_freshness_score(document->timestamp, time(NULL));
    features->popularity_score = calculate_popularity_score(document->click_count, 
                                                           document->click_count * 10, 0.3);
    features->semantic_similarity = calculate_semantic_similarity(query, document);
    
    // Count term matches
    char tokens[32][64];
    int num_tokens = tokenize_query(query, tokens, 32);
    
    for (int i = 0; i < num_tokens; i++) {
        char *title_lower = strdup(document->title);
        char *desc_lower = strdup(document->description);
        
        // Convert to lowercase for case-insensitive matching
        for (int j = 0; title_lower[j]; j++) title_lower[j] = tolower(title_lower[j]);
        for (int j = 0; desc_lower[j]; j++) desc_lower[j] = tolower(desc_lower[j]);
        
        if (strstr(title_lower, tokens[i])) {
            features->title_matches++;
            features->term_matches++;
        }
        if (strstr(desc_lower, tokens[i])) {
            features->term_matches++;
        }
        
        free(title_lower);
        free(desc_lower);
    }
    
    features->document_age = time(NULL) - document->timestamp;
    features->click_count = document->click_count;
    features->bounce_rate = 0.3; // Default bounce rate
    
    return 0;
}

/**
 * @brief Calculate semantic similarity (simplified implementation)
 */
float calculate_semantic_similarity(const char *query, const search_result_t *document) {
    if (!query || !document) {
        return 0.0;
    }
    
    // Very simplified semantic similarity based on common words
    char tokens[32][64];
    int num_tokens = tokenize_query(query, tokens, 32);
    int matches = 0;
    
    for (int i = 0; i < num_tokens; i++) {
        if (strstr(document->title, tokens[i]) || strstr(document->description, tokens[i])) {
            matches++;
        }
    }
    
    return num_tokens > 0 ? (float)matches / num_tokens : 0.0;
}

/* Internal helper function implementations */

/**
 * @brief Compare search results for qsort (descending by relevance)
 */
static int compare_search_results(const void *a, const void *b) {
    const search_result_t *result_a = (const search_result_t*)a;
    const search_result_t *result_b = (const search_result_t*)b;
    
    if (result_a->relevance_score > result_b->relevance_score) return -1;
    if (result_a->relevance_score < result_b->relevance_score) return 1;
    return 0;
}

/**
 * @brief Calculate combined ranking score
 */
static float calculate_combined_score(const char *query, const search_result_t *document, 
                                     const document_features_t *features, const query_context_t *context) {
    float combined_score = 0.0;
    
    // Apply weighted scoring based on configuration
    combined_score += features->tf_idf_score * g_ranking_config.relevance_weight * 0.6;
    combined_score += features->bm25_score * g_ranking_config.relevance_weight * 0.4;
    combined_score += features->pagerank_score * g_ranking_config.authority_weight;
    combined_score += features->freshness_score * g_ranking_config.freshness_weight;
    combined_score += features->popularity_score * g_ranking_config.popularity_weight;
    combined_score += features->semantic_similarity * 0.1; // Small semantic boost
    
    // Apply personalization if context is provided
    if (context && g_ranking_config.personalization_weight > 0) {
        float personalization_score = get_personalization_score(context->user_id, query, document);
        combined_score += personalization_score * g_ranking_config.personalization_weight;
    }
    
    // Boost for title matches
    if (features->title_matches > 0) {
        combined_score += 0.1 * features->title_matches;
    }
    
    // Ensure score is in valid range
    return combined_score > 1.0 ? 1.0 : (combined_score < 0.0 ? 0.0 : combined_score);
}

/**
 * @brief Tokenize query into individual terms
 */
static int tokenize_query(const char *query, char tokens[][64], int max_tokens) {
    if (!query || !tokens || max_tokens <= 0) {
        return 0;
    }
    
    char query_copy[MAX_QUERY_LENGTH];
    strncpy(query_copy, query, sizeof(query_copy) - 1);
    query_copy[sizeof(query_copy) - 1] = '\0';
    
    // Convert to lowercase
    for (int i = 0; query_copy[i]; i++) {
        query_copy[i] = tolower(query_copy[i]);
    }
    
    int token_count = 0;
    char *token = strtok(query_copy, " \t\n\r");
    
    while (token && token_count < max_tokens) {
        strncpy(tokens[token_count], token, 63);
        tokens[token_count][63] = '\0';
        token_count++;
        token = strtok(NULL, " \t\n\r");
    }
    
    return token_count;
}

/**
 * @brief Calculate term frequency in text
 */
static float calculate_term_frequency(const char *term, const char *text) {
    if (!term || !text) {
        return 0.0;
    }
    
    int term_count = 0;
    int total_words = 0;
    
    char text_copy[1024];
    strncpy(text_copy, text, sizeof(text_copy) - 1);
    text_copy[sizeof(text_copy) - 1] = '\0';
    
    // Convert to lowercase
    for (int i = 0; text_copy[i]; i++) {
        text_copy[i] = tolower(text_copy[i]);
    }
    
    char *word = strtok(text_copy, " \t\n\r");
    while (word) {
        total_words++;
        if (strcmp(word, term) == 0) {
            term_count++;
        }
        word = strtok(NULL, " \t\n\r");
    }
    
    return total_words > 0 ? (float)term_count / total_words : 0.0;
}

/**
 * @brief Calculate inverse document frequency (simplified)
 */
static float calculate_inverse_document_frequency(const char *term, int total_documents) {
    (void)term; // Suppress unused parameter warning
    
    // Simplified IDF calculation
    // In practice, this would require a term-document index
    float estimated_doc_frequency = total_documents * 0.1; // Assume term appears in 10% of docs
    return log((float)total_documents / (1 + estimated_doc_frequency));
}

/* Stub implementations for remaining functions */
float calculate_relevance_score(const char *query, const search_result_t *document, 
                               const document_features_t *features) { 
    (void)query; (void)document;
    return features ? features->tf_idf_score : 0.0; 
}

int update_ranking_model(const char *query, const search_result_t *clicked_document, 
                        int position, int dwell_time) { 
    (void)query; (void)clicked_document; (void)position; (void)dwell_time;
    return 0; 
}

float get_personalization_score(const char *user_id, const char *query, 
                               const search_result_t *document) { 
    (void)user_id; (void)query; (void)document;
    return 0.0; 
}

int configure_ranking_system(const ranking_config_t *config) { 
    if (config) g_ranking_config = *config; 
    return 0; 
}

const ranking_config_t* get_ranking_config(void) { return &g_ranking_config; }

void get_ranking_stats(ranking_stats_t *stats) { 
    if (stats) *stats = g_ranking_stats; 
}

void reset_ranking_stats(void) { memset(&g_ranking_stats, 0, sizeof(ranking_stats_t)); }

int save_ranking_model(const char *filename) { 
    (void)filename; return 0; 
}
int load_ranking_model(const char *filename) { 
    (void)filename; return 0; 
}

void normalize_scores(float *scores, int num_scores) {
    if (!scores || num_scores <= 0) return;
    
    float max_score = 0.0;
    for (int i = 0; i < num_scores; i++) {
        if (scores[i] > max_score) max_score = scores[i];
    }
    
    if (max_score > 0.0) {
        for (int i = 0; i < num_scores; i++) {
            scores[i] /= max_score;
        }
    }
}

int expand_query(const char *original_query, char *expanded_query, size_t max_length) {
    if (!original_query || !expanded_query) return -1;
    strncpy(expanded_query, original_query, max_length - 1);
    expanded_query[max_length - 1] = '\0';
    return 0;
}

void log_ranking_decision(const char *query, const search_result_t *document, 
                         float final_score, double ranking_time) {
    // Log to console for now
    printf("RANK: '%s' -> '%s' (score: %.3f, time: %.2fms)\n", 
           query, document ? document->title : "NULL", final_score, ranking_time);
}