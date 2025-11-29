#ifndef FUZZY_H
#define FUZZY_H

typedef struct {
    char *value;
    int distance;
} FuzzyMatch;

typedef struct {
    int dummy; // placeholder for fuzzy matcher state if needed
} FuzzyMatcher;

FuzzyMatcher* fuzzy_create(void);
int fuzzy_levenshteinDistance(const char *str1, const char *str2);
int fuzzy_isFuzzyMatch(const char *query, const char *target, int threshold);
FuzzyMatch* fuzzy_findFuzzyMatches(const char *query, const char **candidates, 
                                   int candidateCount, int threshold, int *matchCount);
double fuzzy_getFuzzyScore(const char *query, const char *target);
void fuzzy_free(FuzzyMatcher *matcher);
void fuzzy_freeMatches(FuzzyMatch *matches, int count);

#endif