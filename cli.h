#ifndef CLI_H
#define CLI_H

#define MAX_INPUT_BUFFER 512
#define MAX_RESULTS 10
#define MAX_QUERY_LENGTH 256

void print_help();
void handle_search(const char *term);
void handle_suggest(const char *term);

#endif

