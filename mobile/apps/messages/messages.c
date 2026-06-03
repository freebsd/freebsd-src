/*
 * Messages App - SMS Messaging
 * uOS(m) - User OS Mobile
 */

#include "messages.h"
#include "../../ui/mobile_ui.h"
#include "../../ui/framebuffer.h"
#include "../../ui/ui_widget.h"
#include "../../ui/window_manager.h"
#include <string.h>
#include <stdio.h>

static messages_t g_messages = {0};
static widget_t *g_input_field = NULL;

static void draw_conversation_list(void) {
    int i, y = 80;
    for (i = 0; i < g_messages.conversation_count; i++) {
        if (i == g_messages.selected_conversation) {
            fb_fill_rect(0, y, FB_WIDTH, 80, 0xFF0066CC);
        } else {
            fb_fill_rect(0, y, FB_WIDTH, 80, 0xFF101010);
        }
        
        fb_draw_text(20, y + 30, g_messages.conversations[i].contact_name, 
                   COLOR_WHITE, COLOR_TRANSPARENT);
        
        if (g_messages.conversations[i].unread > 0) {
            fb_fill_circle(FB_WIDTH - 40, y + 40, 15, COLOR_RED);
        }
        y += 80;
    }
}

static void draw_chat_bubbles(void) {
    conversation_t *conv = &g_messages.conversations[g_messages.selected_conversation];
    int y = 120 - g_messages.scroll_offset;
    int i;
    
    for (i = 0; i < conv->message_count; i++) {
        message_t *msg = &conv->messages[i];
        int bubble_w = FB_WIDTH - 80;
        int bubble_x, bubble_y;
        
        if (msg->direction == MESSAGE_SENT) {
            bubble_x = FB_WIDTH - bubble_w - 20;
        } else {
            bubble_x = 20;
        }
        
        fb_fill_rect(bubble_x, y, bubble_w, 60,
                     msg->direction == MESSAGE_SENT ? 0xFF0066CC : 0xFF404040);
        fb_draw_text(bubble_x + 20, y + 10, msg->text, COLOR_WHITE, COLOR_TRANSPARENT);
        y += 70;
    }
}

static void draw_input_bar(void) {
    int y = FB_HEIGHT - 100;
    fb_fill_rect(0, y, FB_WIDTH, 100, 0xFF202020);
    
    fb_fill_rect(20, y + 20, FB_WIDTH - 100, 60, 0xFF303030);
    fb_draw_text(40, y + 35, g_messages.input_buffer, COLOR_WHITE, COLOR_TRANSPARENT);
    
    fb_fill_rect(FB_WIDTH - 70, y + 20, 50, 60, 0xFF0066CC);
    fb_draw_text(FB_WIDTH - 60, y + 35, "Send", COLOR_WHITE, COLOR_TRANSPARENT);
}

int messages_init(void) {
    memset(&g_messages, 0, sizeof(g_messages));
    
    messages_add_conversation("Alice");
    messages_add_conversation("Bob");
    messages_add_conversation("Charlie");
    
    conversation_t *c = &g_messages.conversations[0];
    strcpy(c->messages[0].text, "Hey, how are you?");
    c->messages[0].direction = MESSAGE_RECEIVED;
    c->messages[0].timestamp = 123456;
    c->message_count = 1;
    
    c = &g_messages.conversations[1];
    strcpy(c->messages[0].text, "Meeting at 3pm");
    c->messages[0].direction = MESSAGE_SENT;
    strcpy(c->messages[1].text, "Got it, see you!");
    c->messages[1].direction = MESSAGE_RECEIVED;
    c->message_count = 2;
    
    wm_init();
    ui_widget_init();
    return 0;
}

void messages_deinit(void) {}

void messages_add_conversation(const char *contact) {
    if (g_messages.conversation_count < MAX_CONVERSATIONS) {
        strncpy(g_messages.conversations[g_messages.conversation_count].contact_name, 
                contact, 31);
        g_messages.conversation_count++;
    }
}

void messages_select_conversation(int index) {
    if (index >= 0 && index < g_messages.conversation_count) {
        g_messages.selected_conversation = index;
        g_messages.conversations[index].unread = 0;
    }
}

void messages_send_message(const char *text) {
    conversation_t *conv = &g_messages.conversations[g_messages.selected_conversation];
    if (conv->message_count < MAX_MESSAGES) {
        message_t *msg = &conv->messages[conv->message_count];
        strncpy(msg->text, text, MAX_TEXT - 1);
        msg->direction = MESSAGE_SENT;
        msg->timestamp = 0;
        conv->message_count++;
        memset(g_messages.input_buffer, 0, MAX_TEXT);
    }
}

void messages_render(void) {
    fb_fill_rect(0, 0, FB_WIDTH, FB_HEIGHT, 0xFF000000);
    
    fb_fill_rect(0, 0, FB_WIDTH, 80, 0xFF101010);
    fb_draw_text(20, 30, "Messages", COLOR_WHITE, COLOR_TRANSPARENT);
    
    if (g_messages.selected_conversation < 0) {
        draw_conversation_list();
    } else {
        draw_chat_bubbles();
        draw_input_bar();
    }
}

void messages_handle_touch(int x, int y, int action) {
    if (action == 0) {
        if (g_messages.selected_conversation < 0) {
            int idx = (y - 80) / 80;
            if (idx >= 0 && idx < g_messages.conversation_count) {
                messages_select_conversation(idx);
            }
        } else if (y > FB_HEIGHT - 100) {
            if (x > FB_WIDTH - 80) {
                messages_send_message(g_messages.input_buffer);
            } else {
                strcat(g_messages.input_buffer, "A");
            }
        } else if (y < 80 && x < 60) {
            g_messages.selected_conversation = -1;
        }
    }
}

int main(void) {
    if (messages_init() != 0) return 1;
    
    mobile_ui_init();
    window_t *win = wm_create_window("Messages", 0, 0, FB_WIDTH, FB_HEIGHT);
    
    while (1) {
        messages_render();
        fb_flush();
        mobile_ui_event_loop();
    }
    
    messages_deinit();
    return 0;
}