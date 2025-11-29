#ifndef MINIGIT_H
#define MINIGIT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Max file content stored per commit */
#define MAX_FILE_CONTENT     50000        // 50 KB snapshot
#define MAX_FILENAME         200
#define MAX_FILES_PER_COMMIT 50

/* -------- Staged File (Linked List) -------- */
/* Here filename will store the FULL PATH (absolute/relative) */
typedef struct File {
    char filename[MAX_FILENAME];  // full path as added
    struct File *next;
} File;

/* -------- File Snapshot Stored in Commit -------- */
/* Here filename will store ONLY the basename, e.g. "main.c" */
typedef struct CommitFile {
    char filename[MAX_FILENAME];          // just the file name
    char content[MAX_FILE_CONTENT];       // full snapshot
} CommitFile;

/* -------- Commit Structure -------- */
typedef struct Commit {
    int commit_id;
    char message[256];

    CommitFile files[MAX_FILES_PER_COMMIT];
    int file_count;

    struct Commit *next;
} Commit;

/* -------- Repository Wrapper -------- */
typedef struct Repository {
    Commit *head;
    int commit_count;
} Repository;

/* -------- Global Variables (defined in minigit.c) -------- */
extern Repository repo;
extern File *index_head;

/* -------- API Functions -------- */
void init_repository(void);
void add_file(char *filename);
void commit_staged(char *msg);
void view_commit(int cid);
void delete_commit(int cid);
void view_log(void);

/* New simple VCS helpers */
void checkout_commit(int cid);
void edit_file(const char *filename);
void save_commit(const char *msg);

#endif /* MINIGIT_H */
