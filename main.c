/**
 * @file main.c
 * @brief Main entry point for Miin GT Search Engine
 * @author Autocomplete and Ranking Specialist
 * @version 1.0
 * 
 * This is the main entry point for the Miin GT Search Engine project.
 * The system focuses on autocomplete suggestions and search result ranking.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "autocomplete.h"
#include "trie_index.h"
#include "ranking.h"
#include "search_engine.h"

#define MAX_QUERY_LENGTH 256
#define MAX_RESULTS 10

/**
 * @brief Print usage information
 * @param program_name The name of the program
 */
void print_usage(const char *program_name) {
    printf("Miin GT Search Engine - Autocomplete and Ranking System\n");
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -h, --help     Show this help message\n");
    printf("  -i, --index    Build search index from data\n");
    printf("  -s, --search   Enter interactive search mode\n");
    printf("  -b, --batch    Process batch queries from file\n");
    printf("\nAutocomplete and Ranking Specialist - 30%% contribution focus\n");
}

/**
 * @brief Interactive search mode
 */
void interactive_search_mode() {
    char query[MAX_QUERY_LENGTH];
    search_result_t results[MAX_RESULTS];
    autocomplete_result_t suggestions[MAX_RESULTS];
    
    printf("=== Miin GT Search Engine - Interactive Mode ===\n");
    printf("Enter search queries (type 'exit' to quit, 'help' for commands)\n\n");
    
    while (1) {
        printf("Search> ");
        fflush(stdout);
        
        if (!fgets(query, sizeof(query), stdin)) {
            break;
        }
        
        // Remove newline
        query[strcspn(query, "\n")] = 0;
        
        if (strcmp(query, "exit") == 0) {
            printf("Goodbye!\n");
            break;
        }
        
        if (strcmp(query, "help") == 0) {
            printf("Commands:\n");
            printf("  exit          - Quit the program\n");
            printf("  help          - Show this help\n");
            printf("  suggest <term> - Get autocomplete suggestions\n");
            printf("  search <term>  - Perform full search with ranking\n");
            continue;
        }
        
        if (strncmp(query, "suggest ", 8) == 0) {
            char *term = query + 8;
            int count = get_autocomplete_suggestions(term, suggestions, MAX_RESULTS);
            
            printf("Autocomplete suggestions for '%s':\n", term);
            for (int i = 0; i < count; i++) {
                printf("  %d. %s (score: %.2f)\n", 
                       i + 1, suggestions[i].suggestion, suggestions[i].score);
            }
            printf("\n");
            continue;
        }
        
        if (strncmp(query, "search ", 7) == 0) {
            char *term = query + 7;
            int count = search_and_rank(term, results, MAX_RESULTS);
            
            printf("Search results for '%s':\n", term);
            for (int i = 0; i < count; i++) {
                printf("  %d. %s (relevance: %.2f)\n", 
                       i + 1, results[i].title, results[i].relevance_score);
                printf("      %s\n", results[i].description);
            }
            printf("\n");
            continue;
        }
        
        // Default behavior - show both suggestions and search results
        if (strlen(query) > 0) {
            // Get autocomplete suggestions
            int suggestion_count = get_autocomplete_suggestions(query, suggestions, MAX_RESULTS);
            if (suggestion_count > 0) {
                printf("Did you mean:\n");
                for (int i = 0; i < suggestion_count && i < 3; i++) {
                    printf("  • %s\n", suggestions[i].suggestion);
                }
                printf("\n");
            }
            
            // Get search results
            int result_count = search_and_rank(query, results, MAX_RESULTS);
            printf("Search results:\n");
            for (int i = 0; i < result_count; i++) {
                printf("  %d. %s (%.2f)\n", 
                       i + 1, results[i].title, results[i].relevance_score);
            }
            printf("\n");
        }
    }
}

/**
 * @brief Main function
 * @param argc Argument count
 * @param argv Argument vector
 * @return Exit status
 */
int main(int argc, char *argv[]) {
    printf("Miin GT Search Engine v1.0\n");
    printf("Autocomplete and Ranking System\n");
    printf("Specialist Contribution: 30%%\n\n");
    
    // Initialize search engine components
    if (init_search_engine() != 0) {
        fprintf(stderr, "Error: Failed to initialize search engine\n");
        return EXIT_FAILURE;
    }
    
    if (init_autocomplete_system() != 0) {
        fprintf(stderr, "Error: Failed to initialize autocomplete system\n");
        cleanup_search_engine();
        return EXIT_FAILURE;
    }
    
    if (init_ranking_system() != 0) {
        fprintf(stderr, "Error: Failed to initialize ranking system\n");
        cleanup_autocomplete_system();
        cleanup_search_engine();
        return EXIT_FAILURE;
    }
    
    // Parse command line arguments
    if (argc == 1) {
        // No arguments - enter interactive mode
        interactive_search_mode();
    } else {
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
                print_usage(argv[0]);
                break;
            } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--search") == 0) {
                interactive_search_mode();
                break;
            } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--index") == 0) {
                printf("Building search index...\n");
                if (build_search_index() == 0) {
                    printf("Index built successfully!\n");
                } else {
                    fprintf(stderr, "Error building index\n");
                }
                break;
            } else {
                printf("Unknown option: %s\n", argv[i]);
                print_usage(argv[0]);
                break;
            }
        }
    }
    
    // Cleanup
    cleanup_ranking_system();
    cleanup_autocomplete_system();
    cleanup_search_engine();
    
    return EXIT_SUCCESS;
}