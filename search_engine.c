/**
 * @file search_engine.c
 * @brief Core search engine implementation (local repo-aware)
 */

#include "search_engine.h"
#include "autocomplete.h"
#include "ranking.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/* ---------- CONFIG ---------- */

#define MAX_DOCUMENTS 100

/* ---------- GLOBAL STATE ---------- */

static search_config_t g_search_config = {0};
static bool   g_search_engine_initialized = false;
static int    g_total_documents   = 0;
static int    g_total_queries     = 0;
static double g_avg_response_time = 0.0;

static search_result_t g_documents[MAX_DOCUMENTS];
static int g_document_count = 0;

/* ---------- GLOBAL COMPARATOR (ONLY ONE) ---------- */

static int cmp_results_descending(const void *a, const void *b) {
    const search_result_t *ra = (const search_result_t *)a;
    const search_result_t *rb = (const search_result_t *)b;

    if (ra->relevance_score < rb->relevance_score) return 1;
    if (ra->relevance_score > rb->relevance_score) return -1;
    return 0;
}

/* ---------- INTERNAL HELPERS ---------- */
/* Highlight all occurrences of the query inside the line using ANSI color */
 void highlight_term(const char *line,
                           const char *term,
                           char *out, int out_size)
{
    const char *p = line;
    char lowered_line[2048];
    char lowered_term[256];

    // Make lowercase versions for matching
    strncpy(lowered_line, line, sizeof(lowered_line));
    lowered_line[sizeof(lowered_line)-1] = '\0';

    strncpy(lowered_term, term, sizeof(lowered_term));
    lowered_term[sizeof(lowered_term)-1] = '\0';

    for (int i = 0; lowered_line[i]; i++) lowered_line[i] = tolower(lowered_line[i]);
    for (int i = 0; lowered_term[i]; i++) lowered_term[i] = tolower(lowered_term[i]);

    int out_pos = 0;
    int term_len = strlen(lowered_term);

    while (*p && out_pos < out_size - 1) {

        // Check if lowercase match occurs
        if (strncasecmp(p, lowered_term, term_len) == 0) {

            // Append color start
            out_pos += snprintf(out + out_pos, out_size - out_pos,
                                "\033[1;33m");   // Yellow Bright

            // Append actual matching chars
            out_pos += snprintf(out + out_pos, out_size - out_pos,
                                "%.*s", term_len, p);

            // Reset color
            out_pos += snprintf(out + out_pos, out_size - out_pos,
                                "\033[0m");

            p += term_len;
        }
        else {
            out[out_pos++] = *p++;
        }
    }

    out[out_pos] = '\0';
}

static void to_lower_inplace(char *s) {
    for (int i = 0; s[i]; i++) {
        if (s[i] >= 'A' && s[i] <= 'Z')
            s[i] = (char)(s[i] - 'A' + 'a');
    }
}
/* Add a prebuilt document (like commit message) to search engine */
void add_document_to_search_engine_virtual(const search_result_t *doc) {
    if (g_document_count >= MAX_DOCUMENTS) return;

    g_documents[g_document_count] = *doc;
    g_document_count++;
    g_total_documents = g_document_count;
}

static int count_occurrences(const char *text, const char *term) {
    if (!*term) return 0;
    int count = 0;
    const char *p = text;
    size_t term_len = strlen(term);

    while ((p = strstr(p, term)) != NULL) {
        count++;
        p += term_len;
    }
    return count;
}

/* ---------- ADD DOCUMENT ---------- */

void add_document_to_search_engine(const char *filename) {
    if (!filename) return;
    if (g_document_count >= MAX_DOCUMENTS) {
        printf("[DEBUG] Document limit reached.\n");
        return;
    }

    printf("[DEBUG] Adding search document: %s\n", filename);

    search_result_t *doc = &g_documents[g_document_count];

    snprintf(doc->title, sizeof(doc->title), "%s", filename);

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        snprintf(doc->description, sizeof(doc->description),
                 "(Could not read file '%s')", filename);
    } else {
        size_t n = fread(doc->description, 1,
                         sizeof(doc->description) - 1, fp);
        doc->description[n] = '\0';
        fclose(fp);
    }

    snprintf(doc->url, sizeof(doc->url), "local-file");

    doc->document_id      = g_document_count + 1;
    doc->relevance_score  = 0.0f;
    doc->timestamp        = (long)time(NULL);
    doc->click_count      = 0;
    doc->authority_score  = 0.0f;

    g_document_count++;
    g_total_documents = g_document_count;
}

/* ---------- INIT ---------- */

int init_search_engine(void) {
    printf("Initializing core search engine...\n");

    g_search_config.relevance_threshold   = 0.1f;
    g_search_config.suggestion_threshold  = 0.1f;
    g_search_config.max_results           = MAX_SEARCH_RESULTS;
    g_search_config.max_suggestions       = MAX_AUTOCOMPLETE_SUGGESTIONS;

    g_document_count   = 0;
    g_total_documents  = 0;
    g_total_queries    = 0;
    g_avg_response_time = 0.0;

    g_search_engine_initialized = true;
    printf("Search engine initialized.\n");
    return 0;
}

void cleanup_search_engine(void) {
    memset(&g_search_config, 0, sizeof(g_search_config));
    g_document_count = 0;
    g_total_documents = 0;
    g_total_queries = 0;
    g_avg_response_time = 0.0;
    g_search_engine_initialized = false;

    printf("Search engine cleanup completed.\n");
}

int build_search_index(void) {
    printf("Building search index...\n");
    printf("Search index built (%d documents).\n", g_total_documents);
    return 0;
}

/* ---------- SEARCH + RANK ---------- */

int search_and_rank(const char *query, search_result_t *results, int max_results) {
    if (!query || !results || max_results <= 0) return 0;
    if (!g_search_engine_initialized) {
        fprintf(stderr, "Error: Search engine not initialized\n");
        return 0;
    }

    clock_t start_time = clock();

    if (g_document_count == 0) {
        printf("[DEBUG] No documents indexed.\n");
        return 0;
    }

    /* ---- 1. Split query into lowercase tokens ---- */

    char query_copy[MAX_QUERY_LENGTH];
    strncpy(query_copy, query, sizeof(query_copy)-1);
    query_copy[sizeof(query_copy)-1] = '\0';
    to_lower_inplace(query_copy);

    char *tokens[16];
    int token_count = 0;

    char *tok = strtok(query_copy, " ");
    while (tok && token_count < 16) {
        tokens[token_count++] = tok;
        tok = strtok(NULL, " ");
    }

    /* ---- 2. Prepare scoring ---- */

    search_result_t local[MAX_DOCUMENTS];
    float scores[MAX_DOCUMENTS];
    int n_local = 0;

    for (int i = 0; i < g_document_count; i++) {

        local[n_local] = g_documents[i];

        char title_l[MAX_TITLE_LENGTH];
        char desc_l[MAX_DESCRIPTION_LENGTH];

        strncpy(title_l, g_documents[i].title, sizeof(title_l)-1);
        title_l[sizeof(title_l)-1] = '\0';

        strncpy(desc_l, g_documents[i].description, sizeof(desc_l)-1);
        desc_l[sizeof(desc_l)-1] = '\0';

        to_lower_inplace(title_l);
        to_lower_inplace(desc_l);

        float doc_score = 0.0f;
        int words_matched = 0;

        /* ---- 3. Score each token ---- */
        for (int t = 0; t < token_count; t++) {
            char *term = tokens[t];

            int title_hits = count_occurrences(title_l, term);
            int body_hits  = count_occurrences(desc_l,  term);

            if (title_hits > 0 || body_hits > 0)
                words_matched++;

            doc_score += (float)(title_hits * 3 + body_hits); // weighted
        }

        /* ---- 4. Bonus: matching more words boosts rank ---- */
        if (token_count > 1)
            doc_score *= (1.0f + (float)words_matched / token_count);

        scores[n_local] = doc_score;
        n_local++;
    }

    /* ---- 5. Normalize and sort ---- */

    float max_raw = 0.0f;
    for (int i = 0; i < n_local; i++)
        if (scores[i] > max_raw) max_raw = scores[i];

    if (max_raw < 0.001f)
        max_raw = 0.001f;

    for (int i = 0; i < n_local; i++)
        local[i].relevance_score = scores[i] / max_raw;

    qsort(local, n_local, sizeof(search_result_t), cmp_results_descending);

    int out_count = n_local < max_results ? n_local : max_results;
    for (int i = 0; i < out_count; i++)
        results[i] = local[i];

    /* ---- 6. Stats ---- */

    clock_t end_time = clock();
    double ms = ((double)(end_time - start_time)) / CLOCKS_PER_SEC * 1000.0;

    g_total_queries++;
    g_avg_response_time =
        (g_avg_response_time * (g_total_queries - 1) + ms) / g_total_queries;

    log_search_query(query, out_count, ms);

    return out_count;
}

/* ---------- UTILS ---------- */

search_config_t* get_search_config(void) { return &g_search_config; }
int update_search_config(const search_config_t *config) { g_search_config = *config; return 0; }

void get_search_stats(int *t_docs, int *t_q, double *avg) {
    if (t_docs) *t_docs = g_total_documents;
    if (t_q)    *t_q    = g_total_queries;
    if (avg)    *avg    = g_avg_response_time;
}
/* Extract a line containing the search term from file */
 int extract_matching_line(const char *filename,
                                 const char *query,
                                 char *out, int out_size) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        snprintf(out, out_size, "(Could not open file)");
        return -1;
    }

    char line[1024];
    int line_no = 1;
    char query_lower[256];

    strncpy(query_lower, query, sizeof(query_lower)-1);
    query_lower[sizeof(query_lower)-1] = '\0';
    for (int i = 0; query_lower[i]; i++)
        query_lower[i] = tolower(query_lower[i]);

    while (fgets(line, sizeof(line), fp)) {
        char line_lower[1024];
        strncpy(line_lower, line, sizeof(line_lower));
        line_lower[sizeof(line_lower)-1] = '\0';

        for (int i = 0; line_lower[i]; i++)
            line_lower[i] = tolower(line_lower[i]);

        if (strstr(line_lower, query_lower)) {
            snprintf(out, out_size,
                     "Line %d: %s", line_no, line);
            fclose(fp);
            return line_no;
        }

        line_no++;
    }

    fclose(fp);
    snprintf(out, out_size, "(No matching line found)");
    return -1;
}

int normalize_query(const char *query, char *out, size_t max_len) {
    if (!query || !out) return -1;
    strncpy(out, query, max_len-1);
    out[max_len-1] = '\0';
    to_lower_inplace(out);
    return 0;
}

float calculate_similarity(const char *a, const char *b) { return 0.0f; }

void log_search_query(const char *query, int results, double ms) {
    printf("SEARCH LOG: '%s', results=%d, time=%.2fms\n", query, results, ms);
}
