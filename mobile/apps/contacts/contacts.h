/*
 * Contacts App - Contact Management
 * uOS(m) - User OS Mobile
 */

#ifndef _CONTACTS_H_
#define _CONTACTS_H_

#include <stdint.h>

#define MAX_CONTACTS 256
#define MAX_NAME 64
#define MAX_PHONE 32
#define MAX_EMAIL 64

typedef struct {
    char name[MAX_NAME];
    char phone[MAX_PHONE];
    char email[MAX_EMAIL];
    uint32_t avatar_color;
    char initials[4];
} contact_t;

typedef struct {
    contact_t contacts[MAX_CONTACTS];
    int contact_count;
    int selected_index;
    int view_mode;
    char search_query[32];
} contacts_t;

int contacts_init(void);
void contacts_deinit(void);
void contacts_render(void);
void contacts_handle_touch(int x, int y, int action);
void contacts_search(const char *query);
void contacts_add_contact(const char *name, const char *phone, const char *email);
void contacts_edit_contact(int index, const char *name, const char *phone, const char *email);
void contacts_delete_contact(int index);

#endif /* _CONTACTS_H_ */