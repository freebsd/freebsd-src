/*
 * Character Device Implementation
 * uOS(m) - User OS Mobile
 */

#include "chardev.h"
#include "memory.h"

#define MAX_CHARDEVS 16
static chardev_t *chardev_table[MAX_CHARDEVS];
static int chardev_count = 0;

extern void uart_puts(const char *s);

/* Simple string compare (since no stdlib) */
static int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

/* Initialize character device subsystem */
int chardev_init(void) {
    uart_puts("Character device subsystem initializing...\n");
    uart_puts("Character device ready\n");
    return 0;
}

/* Register a character device */
int chardev_register(chardev_t *dev) {
    if (!dev || !dev->name || !dev->ops) return -1;
    
    if (chardev_count >= MAX_CHARDEVS) {
        uart_puts("Too many character devices\n");
        return -1;
    }
    
    for (int i = 0; i < chardev_count; i++) {
        if (chardev_table[i] == dev) return -1;  /* Already registered */
    }
    
    chardev_table[chardev_count++] = dev;
    
    uart_puts("Registered chardev: ");
    uart_puts(dev->name);
    uart_puts("\n");
    
    return 0;
}

/* Unregister a character device */
int chardev_unregister(const char *name) {
    if (!name) return -1;
    
    for (int i = 0; i < chardev_count; i++) {
        if (chardev_table[i] && strcmp(chardev_table[i]->name, name) == 0) {
            chardev_table[i] = NULL;
            /* Shift remaining devices */
            for (int j = i; j < chardev_count - 1; j++) {
                chardev_table[j] = chardev_table[j + 1];
            }
            chardev_count--;
            return 0;
        }
    }
    
    return -1;
}

/* Get a character device by name */
chardev_t *chardev_get(const char *name) {
    if (!name) return NULL;
    
    for (int i = 0; i < chardev_count; i++) {
        if (chardev_table[i] && strcmp(chardev_table[i]->name, name) == 0) {
            return chardev_table[i];
        }
    }
    
    return NULL;
}