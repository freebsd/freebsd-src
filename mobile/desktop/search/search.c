/*
 * Global Search / Run Implementation
 */

#include "search.h"
#include "app_launcher.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

static search_results_t *g_results = NULL;
static search_history_entry_t g_history[SEARCH_HISTORY_SIZE];
static int g_history_count = 0;
static int g_max_results = SEARCH_MAX_RESULTS;

static int fuzzy_match(const char *text, const char *pattern) {
    if (!text || !pattern) return 0;
    while (*text && *pattern) {
        if (*text == *pattern) pattern++;
        text++;
    }
    return *pattern == 0;
}

static float fuzzy_score(const char *text, const char *pattern) {
    if (!text || !pattern) return 0.0f;
    if (strcmp(text, pattern) == 0) return 100.0f;
    if (fuzzy_match(text, pattern)) {
        int consecutive = 0, max_consec = 0, curr = 0;
        while (*text && *pattern) {
            if (*text == *pattern) { consecutive++; max_consec = consecutive > max_consec ? consecutive : max_consec; pattern++; }
            else consecutive = 0;
            text++;
        }
        return 40.0f + max_consec * 5.0f;
    }
    return 0.0f;
}

int search_init(void) {
    g_results = calloc(1, sizeof(search_results_t));
    return g_results ? 0 : -1;
}

void search_shutdown(void) {
    if (g_results) { free(g_results); g_results = NULL; }
}

search_results_t *search_query(const char *text) {
    if (!g_results || !text || !text[0]) return g_results;
    g_results->count = 0;
    launcher_t *launcher = launcher_get_instance();
    if (launcher) {
        for (int i = 0; i < launcher->count && g_results->count < g_max_results; i++) {
            float score = fuzzy_score(launcher->apps[i].name, text);
            if (score > 0) {
                search_result_t *r = &g_results->results[g_results->count++];
                r->type = SEARCH_RESULT_APP;
                strncpy(r->id, launcher->apps[i].id, sizeof(r->id) - 1);
                strncpy(r->title, launcher->apps[i].name, sizeof(r->title) - 1);
                strncpy(r->icon_name, launcher->apps[i].icon, sizeof(r->icon_name) - 1);
                r->user_data = &launcher->apps[i];
            }
        }
    }
    return g_results;
}

void search_clear_cache(void) {
    if (g_results) g_results->count = 0;
}

int search_launch_result(search_result_t *result) {
    if (!result) return -1;
    if (result->type == SEARCH_RESULT_APP) return launcher_launch(result->id);
    return -1;
}

void search_add_history(const char *query) {
    if (!query || !query[0] || g_history_count >= SEARCH_HISTORY_SIZE) return;
    strncpy(g_history[g_history_count].query, query, SEARCH_QUERY_MAX - 1);
    g_history[g_history_count].timestamp = (uint64_t)time(NULL);
    g_history_count++;
}

search_history_entry_t *search_get_history(int *count) {
    if (count) *count = g_history_count;
    return g_history;
}

void search_set_max_results(int max) {
    g_max_results = max > 0 ? max : SEARCH_MAX_RESULTS;
}
