/**
 * @file ranking.h
 * @brief Search result ranking system
 */

#ifndef RANKING_H
#define RANKING_H

#include "search_engine.h"
#include <stdbool.h>

/* Ranking algorithm types */
typedef enum {
    RANK_ALGORITHM_TF_IDF,          
    RANK_ALGORITHM_BM25,            
    RANK_ALGORITHM_PAGERANK,        
    RANK_ALGORITHM_LEARNING_TO_RANK,
    RANK_ALGORITHM_HYBRID           
} ranking_algorithm_t;

/* Ranking factor weights */
typedef enum {
    RANK_FACTOR_RELEVANCE,          
    RANK_FACTOR_AUTHORITY,          
    RANK_FACTOR_FRESHNESS,          
    RANK_FACTOR_POPULARITY,         
    RANK_FACTOR_PERSONALIZATION,    
    RANK_FACTOR_LOCATION,           
    RANK_FACTOR_CONTEXT            
} ranking_factor_t;

/* Ranking configuration */
typedef struct {
    ranking_algorithm_t algorithm;
    float relevance_weight;
    float authority_weight;
    float freshness_weight;
    float popularity_weight;
    float personalization_weight;
    bool enable_query_expansion;
    bool enable_semantic_matching;
    float min_relevance_threshold;
} ranking_config_t;

/* Document features for ranking */
typedef struct {
    int document_id;
    float tf_idf_score;
    float bm25_score;
    float pagerank_score;
    float freshness_score;
    float popularity_score;
    float semantic_similarity;
    int term_matches;
    int title_matches;
    int exact_phrase_matches;
    long document_age;
    int click_count;
    float bounce_rate;
} document_features_t;

/* Query context for personalization */
typedef struct {
    char user_id[64];
    char location[128];
    char language[16];
    char device_type[32];
    long search_timestamp;
    char previous_queries[5][256];
    int num_previous_queries;
} query_context_t;

/* Ranking statistics */
typedef struct {
    int total_documents_ranked;
    double avg_ranking_time;
    float avg_relevance_score;
    int queries_processed;
    long last_model_update;
} ranking_stats_t;

/* Core functions */
int init_ranking_system(void);
void cleanup_ranking_system(void);
int rank_search_results(const char *query, search_result_t *documents, int num_documents, const query_context_t *context);
float calculate_relevance_score(const char *query, const search_result_t *document, const document_features_t *features);

/* Algorithm-specific scoring */
float calculate_tf_idf_score(const char *query, const search_result_t *document, int total_documents);
float calculate_bm25_score(const char *query, const search_result_t *document, float avg_doc_length, int total_documents);
float calculate_authority_score(int document_id);
float calculate_freshness_score(long document_timestamp, long current_time);
float calculate_popularity_score(int click_count, int impression_count, float bounce_rate);

/* Learning & personalization */
int update_ranking_model(const char *query, const search_result_t *clicked_document, int position, int dwell_time);
float get_personalization_score(const char *user_id, const char *query, const search_result_t *document);

/* Configuration & stats */
int configure_ranking_system(const ranking_config_t *config);
const ranking_config_t* get_ranking_config(void);
void get_ranking_stats(ranking_stats_t *stats);
void reset_ranking_stats(void);
int save_ranking_model(const char *filename);
int load_ranking_model(const char *filename);

/* Utility functions */
void normalize_scores(float *scores, int num_scores);
float calculate_semantic_similarity(const char *query, const search_result_t *document);
int extract_document_features(const search_result_t *document, const char *query, document_features_t *features);
int expand_query(const char *original_query, char *expanded_query, size_t max_length);
void log_ranking_decision(const char *query, const search_result_t *document, float final_score, double ranking_time);

#endif /* RANKING_H */
