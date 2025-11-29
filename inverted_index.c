#include "inverted_index.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

static char** tokenize(const char *text, int *count) {
    char *copy = (char *)malloc(strlen(text) + 1);
    strcpy(copy, copy);

    char **tokens = (char **)malloc(sizeof(char *) * 1000);
    *count = 0;

    char *word = (char *)malloc(strlen(text) + 1);
    int wordLen = 0;

    for (int i = 0; text[i]; i++) {
        if (isalnum((unsigned char)text[i]) || text[i] == '_') {
            word[wordLen++] = tolower((unsigned char)text[i]);
        } else {
            if (wordLen > 1) {
                word[wordLen] = '\0';
                tokens[*count] = (char *)malloc(wordLen + 1);
                strcpy(tokens[*count], word);
                (*count)++;
                wordLen = 0;
            }
        }
    }
    if (wordLen > 1) {
        word[wordLen] = '\0';
        tokens[*count] = (char *)malloc(wordLen + 1);
        strcpy(tokens[*count], word);
        (*count)++;
    }

    free(word);
    free(copy);
    return tokens;
}

InvertedIndex* invertedindex_create(void) {
    InvertedIndex *index = (InvertedIndex *)malloc(sizeof(InvertedIndex));
    index->terms = (char **)malloc(sizeof(char *) * 10000);
    index->termCount = 0;
    index->postings = (char ***)malloc(sizeof(char **) * 10000);
    index->postingCounts = (int *)calloc(10000, sizeof(int));
    index->idfCache = (double *)calloc(10000, sizeof(double));
    index->idfCacheSize = 0;
    index->documents = (DocumentInfo *)malloc(sizeof(DocumentInfo) * 10000);
    index->documentCount = 0;
    return index;
}

void invertedindex_addDocument(InvertedIndex *index, File *file) {
    int tokenCount;
    char *combined = (char *)malloc(strlen(file->content) + strlen(file->filename) + 2);
    sprintf(combined, "%s %s", file->content, file->filename);
    char **terms = tokenize(combined, &tokenCount);
    free(combined);

    DocumentInfo doc;
    doc.fileId = (char *)malloc(strlen(file->id) + 1);
    strcpy(doc.fileId, file->id);
    doc.termFrequencies = (int *)calloc(tokenCount, sizeof(int));
    doc.termCount = tokenCount;
    doc.totalTerms = tokenCount;

    for (int i = 0; i < tokenCount; i++) {
        int termIdx = -1;
        for (int j = 0; j < index->termCount; j++) {
            if (strcmp(index->terms[j], terms[i]) == 0) {
                termIdx = j;
                break;
            }
        }

        if (termIdx == -1) {
            termIdx = index->termCount;
            index->terms[termIdx] = (char *)malloc(strlen(terms[i]) + 1);
            strcpy(index->terms[termIdx], terms[i]);
            index->postings[termIdx] = (char **)malloc(sizeof(char *) * 10000);
            index->postingCounts[termIdx] = 0;
            index->termCount++;
        }

        doc.termFrequencies[i]++;

        int found = 0;
        for (int j = 0; j < index->postingCounts[termIdx]; j++) {
            if (strcmp(index->postings[termIdx][j], file->id) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            index->postings[termIdx][index->postingCounts[termIdx]] = 
                (char *)malloc(strlen(file->id) + 1);
            strcpy(index->postings[termIdx][index->postingCounts[termIdx]], file->id);
            index->postingCounts[termIdx]++;
        }
    }

    index->documents[index->documentCount] = doc;
    index->documentCount++;

    index->idfCacheSize = 0;

    for (int i = 0; i < tokenCount; i++) {
        free(terms[i]);
    }
    free(terms);
}

double* invertedindex_search(InvertedIndex *index, const char *query, int *fileCount) {
    int queryTokenCount;
    char **queryTerms = tokenize(query, &queryTokenCount);
    double *scores = (double *)calloc(10000, sizeof(double));
    *fileCount = 0;

    for (int i = 0; i < queryTokenCount; i++) {
        int termIdx = -1;
        for (int j = 0; j < index->termCount; j++) {
            if (strcmp(index->terms[j], queryTerms[i]) == 0) {
                termIdx = j;
                break;
            }
        }

        if (termIdx != -1) {
            double idf = invertedindex_getIDF(index, queryTerms[i]);
            for (int j = 0; j < index->postingCounts[termIdx]; j++) {
                // Add TF-IDF score
                scores[j] += idf;
            }
        }
    }

    for (int i = 0; i < queryTokenCount; i++) {
        free(queryTerms[i]);
    }
    free(queryTerms);

    return scores;
}

char** invertedindex_getAllUniqueTerms(InvertedIndex *index, int *count) {
    *count = index->termCount;
    char **result = (char **)malloc(sizeof(char *) * index->termCount);
    for (int i = 0; i < index->termCount; i++) {
        result[i] = (char *)malloc(strlen(index->terms[i]) + 1);
        strcpy(result[i], index->terms[i]);
    }
    return result;
}

double invertedindex_getIDF(InvertedIndex *index, const char *term) {
    for (int i = 0; i < index->termCount; i++) {
        if (strcmp(index->terms[i], term) == 0) {
            if (index->idfCache[i] > 0) {
                return index->idfCache[i];
            }

            int docFreq = index->postingCounts[i];
            int totalDocs = index->documentCount;

            double idf = docFreq > 0 ? log((double)totalDocs / docFreq) : 0;
            index->idfCache[i] = idf;
            return idf;
        }
    }
    return 0;
}

int invertedindex_getTermFrequency(InvertedIndex *index, const char *fileId, const char *term) {
    for (int i = 0; i < index->documentCount; i++) {
        if (strcmp(index->documents[i].fileId, fileId) == 0) {
            for (int j = 0; j < index->documents[i].termCount; j++) {
                // Simplified - would need to track term names in DocumentInfo
                if (index->documents[i].termFrequencies[j] > 0) {
                    return index->documents[i].termFrequencies[j];
                }
            }
        }
    }
    return 0;
}

int invertedindex_getDocumentLength(InvertedIndex *index, const char *fileId) {
    for (int i = 0; i < index->documentCount; i++) {
        if (strcmp(index->documents[i].fileId, fileId) == 0) {
            return index->documents[i].totalTerms;
        }
    }
    return 0;
}

double invertedindex_getAverageDocumentLength(InvertedIndex *index) {
    if (index->documentCount == 0) return 0;
    int total = 0;
    for (int i = 0; i < index->documentCount; i++) {
        total += index->documents[i].totalTerms;
    }
    return (double)total / index->documentCount;
}

void invertedindex_removeDocument(InvertedIndex *index, const char *fileId) {
    for (int i = 0; i < index->documentCount; i++) {
        if (strcmp(index->documents[i].fileId, fileId) == 0) {
            free(index->documents[i].fileId);
            free(index->documents[i].termFrequencies);
            // Shift remaining documents
            for (int j = i; j < index->documentCount - 1; j++) {
                index->documents[j] = index->documents[j + 1];
            }
            index->documentCount--;
            index->idfCacheSize = 0;
            break;
        }
    }
}

void invertedindex_free(InvertedIndex *index) {
    if (!index) return;
    for (int i = 0; i < index->termCount; i++) {
        free(index->terms[i]);
        for (int j = 0; j < index->postingCounts[i]; j++) {
            free(index->postings[i][j]);
        }
        free(index->postings[i]);
    }
    free(index->terms);
    free(index->postings);
    free(index->postingCounts);
    free(index->idfCache);
    for (int i = 0; i < index->documentCount; i++) {
        free(index->documents[i].fileId);
        free(index->documents[i].termFrequencies);
    }
    free(index->documents);
    free(index);
}