/*
 * Messages App - SMS Messaging
 * uOS(m) - User OS Mobile
 */

#ifndef _MESSAGES_H_
#define _MESSAGES_H_

#include <stdint.h>

#define MAX_CONVERSATIONS 64
#define MAX_MESSAGES 256
#define MAX_CONTACTS 32
#define MAX_TEXT 512

typedef enum {
    MESSAGE_SENT,
    MESSAGE_RECEIVED
} message_direction_t;

typedef struct {
    char text[MAX_TEXT];
    int timestamp;
    message_direction_t direction;
} message_t;

typedef struct {
    char contact_name[32];
    message_t messages[MAX_MESSAGES];
    int message_count;
    int unread;
} conversation_t;

typedef struct {
    conversation_t conversations[MAX_CONVERSATIONS];
    int conversation_count;
    int selected_conversation;
    int scroll_offset;
    char input_buffer[MAX_TEXT];
} messages_t;

int messages_init(void);
void messages_deinit(void);
void messages_render(void);
void messages_handle_touch(int x, int y, int action);
void messages_send_message(const char *text);
void messages_add_conversation(const char *contact);
void messages_select_conversation(int index);

#endif /* _MESSAGES_H_ */