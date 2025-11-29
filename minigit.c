#include "minigit.h"
#include "trie_index.h"
#include "autocomplete.h"
#include "search_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include <unistd.h>     // getcwd, access
#include <sys/stat.h>   // mkdir
#include <dirent.h>     // DIR, opendir, readdir, closedir
#define MGIT_DEBUG 0

#define WORKING_DIR ".mgit_work"

/* Globals */
Repository repo;
File *index_head = NULL;

/* ---------- Helpers ---------- */

static void ensure_working_dir(void) {
    struct stat st = {0};
    if (stat(WORKING_DIR, &st) == -1) {
        mkdir(WORKING_DIR, 0700);
    }
}

/* Normalize: autocomplete keeps ASCII, only lowercase */
static void normalize_word_for_autocomplete(char *word) {
    for (int i = 0; word[i]; i++) {
        if (word[i] >= 'A' && word[i] <= 'Z')
            word[i] = (char)(word[i] - 'A' + 'a');
    }
}

/* Normalize: search trie needs a–z only */
static void normalize_word_for_trie(char *word) {
    int w = 0;
    for (int r = 0; word[r]; r++) {
        char c = word[r];
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        if (c >= 'a' && c <= 'z') word[w++] = c;
    }
    word[w] = '\0';
}

/* =============== FILE INDEXING =================== */

static void index_file_for_search(const char *filename) {

#if MGIT_DEBUG
    printf("[DEBUG] index_file_for_search CALLED for: %s\n", filename);
#endif

    FILE *fp = fopen(filename, "r");
    if (!fp) return;

    char line[1024];

    while (fgets(line, sizeof(line), fp)) {

        int i = 0, len = strlen(line);

        while (i < len) {

            char word[256];
            int w = 0;

            while (i < len && (isalnum(line[i]) || line[i]=='_')) {
                word[w++] = tolower(line[i]);
                i++;
            }

            word[w] = '\0';

            if (w > 0) {
#if MGIT_DEBUG
                printf("[DEBUG] CLEAN WORD: '%s'\n", word);
#endif
                add_autocomplete_suggestion(word, 0.6f, AC_SOURCE_DOCUMENT_TITLES);

                char trie_word[256];
                int tw = 0;

                for (int j = 0; j < w; j++)
                    if (word[j] >= 'a' && word[j] <= 'z')
                        trie_word[tw++] = word[j];

                trie_word[tw] = '\0';

                if (tw > 0)
                    trie_insert_word(trie_word, filename);
            }

            i++;
        }
    }

    fclose(fp);
}


/* =============== COMMIT MESSAGE INDEXING =================== */

/* Index commit message for autocomplete + search engine */
static void index_commit_message(const char *msg, int commit_id) {

#if MGIT_DEBUG
    printf("[DEBUG] Indexing commit message: \"%s\"\n", msg);
#endif

    char temp[512];
    strncpy(temp, msg, sizeof(temp)-1);
    temp[sizeof(temp)-1] = '\0';

    char *token = strtok(temp, " ");

    while (token) {

        char clean[256];
        int w = 0;

        for (int i = 0; token[i]; i++)
            if (isalnum(token[i]))
                clean[w++] = tolower(token[i]);

        clean[w] = '\0';

        if (w > 0) {
#if MGIT_DEBUG
            printf("[DEBUG] COMMIT WORD: %s\n", clean);
#endif
            add_autocomplete_suggestion(clean, 0.7f, AC_SOURCE_DOCUMENT_TITLES);
            trie_insert_word(clean, "COMMIT");
        }

        token = strtok(NULL, " ");
    }

    search_result_t doc = {0};
    snprintf(doc.title, sizeof(doc.title), "Commit #%d", commit_id);
    strncpy(doc.description, msg, sizeof(doc.description));
    strncpy(doc.url, "commit-msg", sizeof(doc.url));

    add_document_to_search_engine_virtual(&doc);
}

/* =============== SIMPLE VCS OPERATIONS =================== */

/* Checkout: write commit snapshots to .mgit_work/<filename> */
void checkout_commit(int cid) {
    ensure_working_dir();

    Commit *temp = repo.head;
    while (temp) {
        if (temp->commit_id == cid) {
            printf("Checking out commit %d...\n", cid);

            for (int i = 0; i < temp->file_count; i++) {
                CommitFile *cf = &temp->files[i];

                char path[512];
                snprintf(path, sizeof(path), "%s/%s", WORKING_DIR, cf->filename);

                FILE *fp = fopen(path, "w");
                if (!fp) {
                    printf("Error writing %s\n", path);
                    continue;
                }

                fprintf(fp, "%s", cf->content);
                fclose(fp);

                printf("  Wrote %s\n", path);
            }

            printf("Files written to %s/\n", WORKING_DIR);
            return;
        }
        temp = temp->next;
    }

    printf("Commit %d not found.\n", cid);
}

/* Very simple in-terminal editor */
void edit_file(const char *filename) {
    ensure_working_dir();

    char path[512];
    snprintf(path, sizeof(path), "%s/%s", WORKING_DIR, filename);

    FILE *fp = fopen(path, "r");
    if (!fp) {
        printf("File not found in working directory: %s\n", path);
        return;
    }

    printf("\n--- Current content of %s ---\n", filename);
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        printf("%s", line);
    }
    fclose(fp);

    printf("\n--- Enter new content (END with a single line containing 'EOF') ---\n");

    fp = fopen(path, "w");
    if (!fp) {
        printf("Cannot open file for writing: %s\n", path);
        return;
    }

    while (1) {
        if (!fgets(line, sizeof(line), stdin)) break;
        if (strcmp(line, "EOF\n") == 0 || strcmp(line, "EOF\r\n") == 0)
            break;
        fputs(line, fp);
    }

    fclose(fp);
    printf("File updated: %s\n", path);
}

/* Save: create a commit from everything in .mgit_work/ */
void save_commit(const char *msg) {
    ensure_working_dir();

    Commit *new_commit = malloc(sizeof(Commit));
    new_commit->commit_id = ++repo.commit_count;
    strncpy(new_commit->message, msg, 255);
    new_commit->file_count = 0;
    new_commit->next = repo.head;
    repo.head = new_commit;

    DIR *dir = opendir(WORKING_DIR);
    struct dirent *dp;

    while ((dp = readdir(dir))) {
        if (dp->d_name[0] == '.') continue;

        char path[512];
        snprintf(path, sizeof(path), "%s/%s", WORKING_DIR, dp->d_name);

        FILE *fp = fopen(path, "r");
        if (!fp) continue;

        CommitFile *cf = &new_commit->files[new_commit->file_count];
        strncpy(cf->filename, dp->d_name, MAX_FILENAME);
        int n = fread(cf->content, 1, MAX_FILE_CONTENT-1, fp);
        cf->content[n] = '\0';
        fclose(fp);

        index_file_for_search(path);
        new_commit->file_count++;
    }

    closedir(dir);
    index_commit_message(new_commit->message, new_commit->commit_id);

    printf("Created commit %d.\n", new_commit->commit_id);
}


/* =============== REPOSITORY FUNCTIONS =================== */

void init_repository(void) {
    repo.head = NULL;
    repo.commit_count = 0;
    printf("Repository has been initialized.\n");
}

void add_file(char *filename) {
    if (!filename || strlen(filename) == 0) {
        printf("Invalid filename.\n");
        return;
    }

    char fullpath[MAX_FILENAME];

    if (filename[0] == '/' || filename[0] == '\\') {
        strncpy(fullpath, filename, sizeof(fullpath) - 1);
        fullpath[sizeof(fullpath) - 1] = '\0';
    } else {
        char cwd[1024];
        getcwd(cwd, sizeof(cwd));
        snprintf(fullpath, sizeof(fullpath), "%s/%s", cwd, filename);
    }

    FILE *fp = fopen(fullpath, "r");
    if (!fp) {
        printf("Error: File '%s' does not exist.\n", fullpath);
        return;
    }
    fclose(fp);

    File *new_file = (File *)malloc(sizeof(File));
    if (!new_file) {
        printf("Memory allocation failed.\n");
        return;
    }

    strncpy(new_file->filename, fullpath, sizeof(new_file->filename) - 1);
    new_file->filename[sizeof(new_file->filename) - 1] = '\0';

    new_file->next = index_head;
    index_head = new_file;

    printf("File added: %s\n", new_file->filename);

    index_file_for_search(new_file->filename);
    add_document_to_search_engine(new_file->filename);
}

/* Create a real snapshot commit from staged files */
void commit_staged(char *msg) {
    if (!index_head) {
        printf("No files to commit.\n");
        return;
    }

    Commit *new_commit = malloc(sizeof(Commit));
    new_commit->commit_id = ++repo.commit_count;
    strncpy(new_commit->message, msg, 255);

    new_commit->file_count = 0;
    new_commit->next = repo.head;
    repo.head = new_commit;

    printf("Commit %d created.\n", new_commit->commit_id);

    File *f = index_head;
    while (f) {
        CommitFile *cf = &new_commit->files[new_commit->file_count];

        const char *base = strrchr(f->filename, '/');
        base = base ? base + 1 : f->filename;

        strncpy(cf->filename, base, MAX_FILENAME-1);

        FILE *fp = fopen(f->filename, "r");
        if (fp) {
            int n = fread(cf->content, 1, MAX_FILE_CONTENT-1, fp);
            cf->content[n] = '\0';
            fclose(fp);
        }

        index_file_for_search(f->filename);

        new_commit->file_count++;
        f = f->next;
    }

    index_commit_message(new_commit->message, new_commit->commit_id);

    while (index_head) {
        File *del = index_head;
        index_head = index_head->next;
        free(del);
    }
}


void view_commit(int cid) {
    Commit *temp = repo.head;

    while (temp) {
        if (temp->commit_id == cid) {
            printf("\n=== Commit %d ===\n", temp->commit_id);
            printf("Message: %s\n", temp->message);
            printf("Files in this commit: %d\n\n", temp->file_count);

            for (int i = 0; i < temp->file_count; i++) {
                CommitFile *cf = &temp->files[i];

                printf(" --- File #%d ---\n", i + 1);
                printf("Filename: %s\n", cf->filename);
                printf("Content:\n");
                printf("----------------------------------------\n");
                printf("%s\n", cf->content);
                printf("----------------------------------------\n\n");
            }

            return;
        }
        temp = temp->next;
    }

    printf("Commit %d not found.\n", cid);
}

void delete_commit(int cid) {
    Commit *temp = repo.head, *prev = NULL;
    while (temp != NULL && temp->commit_id != cid) {
        prev = temp;
        temp = temp->next;
    }
    if (temp == NULL) {
        printf("Commit not found.\n");
        return;
    }
    if (prev == NULL)
        repo.head = temp->next;
    else
        prev->next = temp->next;

    free(temp);
    printf("Commit %d deleted.\n", cid);
}

void view_log(void) {
    Commit *temp = repo.head;
    if (!temp) {
        printf("No commits yet.\n");
        return;
    }
    while (temp) {
        printf("Commit %d: %s\n",
               temp->commit_id, temp->message);
        temp = temp->next;
    }
}
