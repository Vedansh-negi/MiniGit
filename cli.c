#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli.h"
#include "trie_index.h"
#include "minigit.h"
#include "search_engine.h"
#include "autocomplete.h"
#include "ranking.h"

/* Prototypes for helper functions implemented elsewhere */
int extract_matching_line(const char *filename,
                          const char *query,
                          char *out, int out_size);

void highlight_term(const char *line, const char *term,
                    char *out, int out_size);

/* Prototypes for new MiniGit operations (implemented in minigit.c) */
void checkout_commit(int cid);
void edit_file(const char *filename);
void save_commit(const char *msg);

void print_help() {
    printf("\n--- Mini-Git & Smart Search Engine ---\n");
    printf("Mini-Git Commands:\n");
    printf("  init                      - Initialize a new repository.\n");
    printf("  add <filename>            - Add a file to the staging area.\n");
    printf("  commit \"<message>\"        - Commit staged files.\n");
    printf("  log                       - View commit history.\n");
    printf("  view <commit_id>          - View details of a specific commit.\n");
    printf("  delete <commit_id>        - Delete a commit.\n");
    printf("\nSearch Engine Commands:\n");
    printf("  search <term>             - Perform full search with ranking.\n");
    printf("  suggest <prefix>          - Get autocomplete suggestions.\n");
    printf("\nWorking Copy / Simple VCS Commands:\n");
    printf("  checkout <commit_id>      - Load files from a commit into working directory.\n");
    printf("  edit <filename>           - Edit a file in the working directory (simple editor).\n");
    printf("  save \"message\"            - Commit all files from working directory.\n");
    printf("\nGeneral Commands:\n");
    printf("  help                      - Show this help message.\n");
    printf("  exit                      - Quit the application.\n\n");
}

void handle_search(const char *term) {
    search_result_t results[MAX_RESULTS];
    int count = search_and_rank(term, results, MAX_RESULTS);

    printf("\nSearch results for '%s':\n", term);
    if (count == 0) {
        printf("  No results found.\n");
        return;
    }
    for (int i = 0; i < count; i++) {
        printf("  %d. %s (Relevance: %.2f)\n",
               i + 1, results[i].title, results[i].relevance_score);

        if (strcmp(results[i].url, "local-file") == 0) {
            char snippet[1024];
            char highlighted[2048];

            (void)extract_matching_line(results[i].title, term,
                                        snippet, sizeof(snippet));
            highlight_term(snippet, term, highlighted, sizeof(highlighted));

            printf("      %s\n", highlighted);
        } else {
            // Commit message (virtual document)
            printf("      Message: %s\n", results[i].description);
        }

        printf("      URL: %s\n\n", results[i].url);
    }
}

void handle_suggest(const char *term) {
    autocomplete_result_t suggestions[MAX_AUTOCOMPLETE_SUGGESTIONS];
    int count = get_autocomplete_suggestions(term, suggestions, MAX_AUTOCOMPLETE_SUGGESTIONS);

    printf("\nAutocomplete suggestions for '%s':\n", term);
    if (count == 0) {
        printf("  No suggestions found.\n");
        return;
    }
    for (int i = 0; i < count; i++) {
        printf("  - %s (Score: %.2f)\n",
               suggestions[i].suggestion, suggestions[i].score);
    }
    printf("\n");
}

int main() {
    char input[MAX_INPUT_BUFFER];
    char *command, *argument;

    init_autocomplete_system();   // only one time
    initialize_trie();
    init_repository();
    init_search_engine();
    init_ranking_system();

    print_help();

    while (1) {
        printf("cli> ");

        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }

        input[strcspn(input, "\n")] = 0;
        command = strtok(input, " ");
        if (!command) continue;
        argument = strtok(NULL, "");

        if (strcmp(command, "exit") == 0) {
            printf("Goodbye!\n");
            break;
        }
        else if (strcmp(command, "help") == 0) {
            print_help();
        }
        else if (strcmp(command, "init") == 0) {
            init_repository();
        }
        else if (strcmp(command, "add") == 0) {
            argument ? add_file(argument)
                     : printf("Usage: add <filename>\n");
        }
        else if (strcmp(command, "commit") == 0) {
            argument ? commit_staged(argument)
                     : printf("Usage: commit \"<message>\"\n");
        }
        else if (strcmp(command, "log") == 0) {
            view_log();
        }
        else if (strcmp(command, "view") == 0) {
            argument ? view_commit(atoi(argument))
                     : printf("Usage: view <commit_id>\n");
        }
        else if (strcmp(command, "delete") == 0) {
            argument ? delete_commit(atoi(argument))
                     : printf("Usage: delete <commit_id>\n");
        }
        else if (strcmp(command, "search") == 0) {
            argument ? handle_search(argument)
                     : printf("Usage: search <term>\n");
        }
        else if (strcmp(command, "suggest") == 0) {
            argument ? handle_suggest(argument)
                     : printf("Usage: suggest <prefix>\n");
        }
        else if (strcmp(command, "checkout") == 0) {
            argument ? checkout_commit(atoi(argument))
                     : printf("Usage: checkout <commit_id>\n");
        }
        else if (strcmp(command, "edit") == 0) {
            argument ? edit_file(argument)
                     : printf("Usage: edit <filename>\n");
        }
        else if (strcmp(command, "save") == 0) {
            argument ? save_commit(argument)
                     : printf("Usage: save \"message\"\n");
        }
        else {
            printf("Unknown command: '%s'. Type 'help' for assistance.\n",
                   command);
        }
    }

    cleanup_ranking_system();
    cleanup_autocomplete_system();
    cleanup_search_engine();
    return 0;
}
