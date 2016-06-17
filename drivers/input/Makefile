#
# Makefile for the input core drivers.
#

# The target object and module list name.

O_TARGET	:= inputdrv.o

# Objects that export symbols.

export-objs	:= input.o

# Object file lists.

obj-y	:=
obj-m	:=
obj-n	:=
obj-	:=

# Each configuration option enables a list of files.

obj-$(CONFIG_INPUT)		+= input.o
obj-$(CONFIG_INPUT_KEYBDEV)	+= keybdev.o
obj-$(CONFIG_INPUT_MOUSEDEV)	+= mousedev.o
obj-$(CONFIG_INPUT_JOYDEV)	+= joydev.o
obj-$(CONFIG_INPUT_EVDEV)	+= evdev.o
obj-$(CONFIG_INPUT_UINPUT)	+= uinput.o

# The global Rules.make.

include $(TOPDIR)/Rules.make
