
TARGET = $(TOPDIR)/include/asm-x86_64/offset.h

all: 

mrproper:

fastdep: $(TARGET)

.PHONY: all

$(TARGET): offset.h
	cmp -s $^ $@ || (cp $^ $(TARGET).new && mv $(TARGET).new $(TARGET))

.PHONY : offset.h all modules modules_install

offset.h: offset.sed offset.c FORCE_RECOMPILE
	$(CC) $(CFLAGS) -S -o offset.tmp offset.c
	sed -n -f offset.sed < offset.tmp > offset.h   

FORCE_RECOMPILE:

clean:
	rm -f offset.[hs] $(TARGET).new offset.tmp

mrproper:	
	rm -f offset.[hs] $(TARGET)
	rm -f $(TARGET)

include $(TOPDIR)/Rules.make

