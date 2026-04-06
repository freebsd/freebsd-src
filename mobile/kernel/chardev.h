/*
 * Character Device Abstraction
 * uOS(m) - User OS Mobile
 */

#ifndef _CHARDEV_H_
#define _CHARDEV_H_

#include <stdint.h>
#include <stddef.h>

typedef long ssize_t;  /* Signed size type */

/* Character device operations */
typedef struct {
    int (*open)(void);
    int (*close)(void);
    ssize_t (*read)(void *buf, size_t count);
    ssize_t (*write)(const void *buf, size_t count);
    int (*ioctl)(unsigned long cmd, void *arg);
} chardev_ops_t;

/* Character device */
typedef struct {
    const char *name;
    chardev_ops_t *ops;
    void *private_data;
} chardev_t;

/* Register a character device */
int chardev_register(chardev_t *dev);

/* Unregister a character device */
int chardev_unregister(const char *name);

/* Get a character device by name */
chardev_t *chardev_get(const char *name);

#endif /* _CHARDEV_H_ */