#include "fuzzy.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

FuzzyMatcher* fuzzy_create(void) {
    FuzzyMatcher *matcher = (FuzzyMatcher *)malloc(sizeof(FuzzyMatcher));
    return matcher;
}

static char* strToLower(const char *str) {
    char *result = (char *)malloc(strlen(str) + 1);
    for (int i = 0; str[i]; i++) {
        result[i] = tolower((unsigned char)str[i]);
    }
    result[strlen(str)] = '\0';
    return result;
}

static int min(int a, int b) {
    return a < b ? a : b;
}

static int min3(int a, int b, int c) {
    int m = a < b ? a : b;
    return m < c ? m : c;
}

int fuzzy_levenshteinDistance(const char *str1, const char *str2) {
    int len1 = strlen(str1);
    int len2 = strlen(str2);

    int **matrix = (int **)malloc((len1 + 1) * sizeof(int *));
    for (int i = 0; i <= len1; i++) {
        matrix[i] = (int *)calloc(len2 + 1, sizeof(int));
    }

    for (int i = 0; i <= len1; i++) matrix[i][0] = i;
    for (int j = 0; j <= len2; j++) matrix[0][j] = j;

    for (int i = 1; i <= len1; i++) {
        for (int j = 1; j <= len2; j++) {
            int cost = (str1[i - 1] == str2[j - 1]) ? 0 : 1;
            matrix[i][j] = min3(
                matrix[i - 1][j] + 1,
                matrix[i][j - 1] + 1,
                matrix[i - 1][j - 1] + cost
            );
        }
    }

    int result = matrix[len1][len2];

    for (int i = 0; i <= len1; i++) {
        free(matrix[i]);
    }
    free(matrix);

    return result;
}

int fuzzy_isFuzzyMatch(const char *query, const char *target, int threshold) {
    char *queryLower = strToLower(query);
    char *targetLower = strToLower(target);

    if (strcmp(queryLower, targetLower) == 0) {
        free(queryLower);
        free(targetLower);
        return 1;
    }

    if (strlen(queryLower) < 3) {
        int result = strstr(targetLower, queryLower) != NULL;
        free(queryLower);
        free(targetLower);
        return result;
    }

    int distance = fuzzy_levenshteinDistance(queryLower, targetLower);
    free(queryLower);
    free(targetLower);
    return distance <= threshold;
}

FuzzyMatch* fuzzy_findFuzzyMatches(const char *query, const char **candidates,
                                   int candidateCount, int threshold, int *matchCount) {
    FuzzyMatch *matches = (FuzzyMatch *)malloc(sizeof(FuzzyMatch) * candidateCount);
    *matchCount = 0;

    char *queryLower = strToLower(query);

    for (int i = 0; i < candidateCount; i++) {
        char *candidateLower = strToLower(candidates[i]);
        int distance = fuzzy_levenshteinDistance(queryLower, candidateLower);

        if (distance <= threshold) {
            matches[*matchCount].value = (char *)malloc(strlen(candidates[i]) + 1);
            strcpy(matches[*matchCount].value, candidates[i]);
            matches[*matchCount].distance = distance;
            (*matchCount)++;
        }
        free(candidateLower);
    }

    free(queryLower);

    // Sort by distance
    for (int i = 0; i < *matchCount - 1; i++) {
        for (int j = 0; j < *matchCount - i - 1; j++) {
            if (matches[j].distance > matches[j + 1].distance) {
                FuzzyMatch temp = matches[j];
                matches[j] = matches[j + 1];
                matches[j + 1] = temp;
            }
        }
    }

    return matches;
}

double fuzzy_getFuzzyScore(const char *query, const char *target) {
    char *queryLower = strToLower(query);
    char *targetLower = strToLower(target);

    if (strstr(targetLower, queryLower) != NULL) {
        free(queryLower);
        free(targetLower);
        return 1.0;
    }

    int distance = fuzzy_levenshteinDistance(queryLower, targetLower);
    int maxLen = strlen(queryLower) > strlen(targetLower) ? 
                 strlen(queryLower) : strlen(targetLower);

    free(queryLower);
    free(targetLower);

    if (maxLen == 0) return 0;
    return 1.0 - (double)distance / maxLen;
}

void fuzzy_free(FuzzyMatcher *matcher) {
    if (!matcher) return;
    free(matcher);
}

void fuzzy_freeMatches(FuzzyMatch *matches, int count) {
    if (!matches) return;
    for (int i = 0; i < count; i++) {
        free(matches[i].value);
    }
    free(matches);
}