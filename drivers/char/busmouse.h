/*
 * linux/drivers/char/busmouse.h
 *
 * Copyright (C) 1995 - 1998 Russell King
 *
 * Prototypes for generic busmouse interface
 */
#ifndef BUSMOUSE_H
#define BUSMOUSE_H

struct busmouse {
	int minor;
	const char *name;
	struct module *owner;
	int (*open)(struct inode * inode, struct file * file);
	int (*release)(struct inode * inode, struct file * file);
	int init_button_state;
};

extern void busmouse_add_movementbuttons(int mousedev, int dx, int dy, int buttons);
extern void busmouse_add_movement(int mousedev, int dx, int dy);
extern void busmouse_add_buttons(int mousedev, int clear, int eor);

extern int register_busmouse(struct busmouse *ops);
extern int unregister_busmouse(int mousedev);

#endif
