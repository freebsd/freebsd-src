/*
 * Global Search / Run for uOS(m) Desktop
 * Searches apps, files, contacts, settings. Inline calculator.
 */

#ifndef _SEARCH_H_
#define _SEARCH_H_

#include <stdint.h>
#include "../ui/framebuffer.h"

#define SEARCH_MAX_RESULTS     20
#define SEARCH_MAX_CATEGORIES  8
#define SEARCH_HISTORY_SIZE    50
#define SEARCH_QUERY_MAX       256

typedef enum {
    SEARCH_RESULT_APP,
    SEARCH_RESULT_FILE,
    SEARCH_RESULT_CONTACT,
    SEARCH_RESULT_SETTING,
    SEARCH_RESULT_CALCULATOR,
    SEARCH_RESULT_ACTION
} search_result_type_t;

typedef struct {
    search_result_type_t type;
    char id[128];
    char title[128];
    char subtitle[256];
    char icon_name[64];
    void *user_data;
} search_result_t;

typedef struct {
    search_result_t results[SEARCH_MAX_RESULTS];
    int count;
} search_results_t;

typedef struct {
    char query[SEARCH_QUERY_MAX];
    uint64_t timestamp;
} search_history_entry_t;

int search_init(void);
void search_shutdown(void);
search_results_t *search_query(const char *text);
void search_clear_cache(void);
int search_launch_result(search_result_t *result);
void search_add_history(const char *query);
search_history_entry_t *search_get_history(int *count);
void search_set_max_results(int max);

#endif /* _SEARCH_H_ */
