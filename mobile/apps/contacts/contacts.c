/*
 * Contacts App - Contact Management
 * uOS(m) - User OS Mobile
 */

#include "contacts.h"
#include "../../ui/mobile_ui.h"
#include "../../ui/framebuffer.h"
#include "../../ui/ui_widget.h"
#include "../../ui/window_manager.h"
#include <string.h>
#include <stdio.h>

static contacts_t g_contacts = {0};
static widget_t *g_search_box = NULL;

static void get_initials(const char *name, char *out) {
    int len = strlen(name);
    if (len > 0) out[0] = name[0];
    else out[0] = '?';
    out[1] = '\0';
}

static void draw_header(void) {
    fb_fill_rect(0, 0, FB_WIDTH, 80, 0xFF202020);
    fb_draw_text(20, 30, "Contacts", COLOR_WHITE, COLOR_TRANSPARENT);
}

static void draw_search_bar(void) {
    fb_fill_rect(20, 100, FB_WIDTH - 40, 60, 0xFF303030);
    fb_draw_text(40, 115, g_contacts.search_query, COLOR_WHITE, COLOR_TRANSPARENT);
}

static void draw_contact_item(contact_t *contact, int idx, int y) {
    if (idx == g_contacts.selected_index) {
        fb_fill_rect(0, y, FB_WIDTH, 100, 0xFF0066CC);
    }
    
    fb_fill_circle(60, y + 50, 30, contact->avatar_color);
    fb_draw_text(45, y + 55, contact->initials, COLOR_WHITE, COLOR_TRANSPARENT);
    
    fb_draw_text(110, y + 30, contact->name, COLOR_WHITE, COLOR_TRANSPARENT);
    fb_draw_text(110, y + 55, contact->phone, 0xFFCCCCCC, COLOR_TRANSPARENT);
}

static void draw_contact_detail(contact_t *contact) {
    int y = 180;
    
    fb_draw_text(40, y, contact->name, COLOR_WHITE, COLOR_TRANSPARENT);
    y += 60;
    
    fb_draw_text(40, y, "Phone:", 0xFFCCCCCC, COLOR_TRANSPARENT);
    fb_draw_text(150, y, contact->phone, COLOR_WHITE, COLOR_TRANSPARENT);
    y += 50;
    
    fb_draw_text(40, y, "Email:", 0xFFCCCCCC, COLOR_TRANSPARENT);
    fb_draw_text(150, y, contact->email, COLOR_WHITE, COLOR_TRANSPARENT);
    y += 80;
    
    fb_fill_rect(40, y, FB_WIDTH - 80, 60, 0xFF404040);
    fb_draw_text(60, y + 20, "Edit", COLOR_WHITE, COLOR_TRANSPARENT);
}

int contacts_init(void) {
    memset(&g_contacts, 0, sizeof(g_contacts));
    g_contacts.view_mode = 0;
    
    contacts_add_contact("Alice Johnson", "555-0101", "alice@example.com");
    contacts_add_contact("Bob Smith", "555-0102", "bob@example.com");
    contacts_add_contact("Charlie Brown", "555-0103", "charlie@example.com");
    
    wm_init();
    ui_widget_init();
    return 0;
}

void contacts_deinit(void) {}

void contacts_add_contact(const char *name, const char *phone, const char *email) {
    if (g_contacts.contact_count < MAX_CONTACTS) {
        contact_t *c = &g_contacts.contacts[g_contacts.contact_count];
        strncpy(c->name, name, MAX_NAME - 1);
        strncpy(c->phone, phone, MAX_PHONE - 1);
        strncpy(c->email, email, MAX_EMAIL - 1);
        c->avatar_color = 0xFF4080FF + (g_contacts.contact_count * 0x101010);
        get_initials(name, c->initials);
        g_contacts.contact_count++;
    }
}

void contacts_edit_contact(int index, const char *name, const char *phone, const char *email) {
    if (index >= 0 && index < g_contacts.contact_count) {
        contact_t *c = &g_contacts.contacts[index];
        if (name) strncpy(c->name, name, MAX_NAME - 1);
        if (phone) strncpy(c->phone, phone, MAX_PHONE - 1);
        if (email) strncpy(c->email, email, MAX_EMAIL - 1);
    }
}

void contacts_delete_contact(int index) {
    if (index >= 0 && index < g_contacts.contact_count - 1) {
        memmove(&g_contacts.contacts[index], &g_contacts.contacts[index + 1],
                (g_contacts.contact_count - index - 1) * sizeof(contact_t));
        g_contacts.contact_count--;
    }
}

void contacts_search(const char *query) {
    strncpy(g_contacts.search_query, query, 31);
}

void contacts_render(void) {
    fb_fill_rect(0, 0, FB_WIDTH, FB_HEIGHT, 0xFF000000);
    draw_header();
    
    if (g_contacts.view_mode == 0) {
        draw_search_bar();
        int i, y = 180;
        for (i = 0; i < g_contacts.contact_count; i++) {
            draw_contact_item(&g_contacts.contacts[i], i, y);
            y += 100;
        }
    } else {
        draw_contact_detail(&g_contacts.contacts[g_contacts.selected_index]);
    }
}

void contacts_handle_touch(int x, int y, int action) {
    if (action == 0) {
        if (y < 180) {
            if (x > FB_WIDTH - 80) {
                contacts_add_contact("New Contact", "555-0000", "new@example.com");
            }
        } else if (g_contacts.view_mode == 0) {
            int idx = (y - 180) / 100;
            if (idx >= 0 && idx < g_contacts.contact_count) {
                g_contacts.selected_index = idx;
                g_contacts.view_mode = 1;
            }
        }
    }
}

int main(void) {
    if (contacts_init() != 0) return 1;
    
    mobile_ui_init();
    window_t *win = wm_create_window("Contacts", 0, 0, FB_WIDTH, FB_HEIGHT);
    
    while (1) {
        contacts_render();
        fb_flush();
        mobile_ui_event_loop();
    }
    
    contacts_deinit();
    return 0;
}