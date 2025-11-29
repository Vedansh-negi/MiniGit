#ifndef INVERTED_INDEX_H
#define INVERTED_INDEX_H

#include "schema.h"

typedef struct {
    char *fileId;
    int *termFrequencies;
    int termCount;
    int totalTerms;
} DocumentInfo;

typedef struct {
    char **terms;
    int termCount;
    char ***postings; // postings[i] = fileIds for term i
    int *postingCounts;
    double *idfCache;
    int idfCacheSize;
    DocumentInfo *documents;
    int documentCount;
} InvertedIndex;

InvertedIndex* invertedindex_create(void);
void invertedindex_addDocument(InvertedIndex *index, File *file);
double* invertedindex_search(InvertedIndex *index, const char *query, int *fileCount);
char** invertedindex_getAllUniqueTerms(InvertedIndex *index, int *count);
double invertedindex_getIDF(InvertedIndex *index, const char *term);
int invertedindex_getTermFrequency(InvertedIndex *index, const char *fileId, const char *term);
int invertedindex_getDocumentLength(InvertedIndex *index, const char *fileId);
double invertedindex_getAverageDocumentLength(InvertedIndex *index);
void invertedindex_removeDocument(InvertedIndex *index, const char *fileId);
void invertedindex_free(InvertedIndex *index);

#endif