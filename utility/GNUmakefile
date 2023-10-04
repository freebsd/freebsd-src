# PUBLIC DOMAIN - NO WARRANTY, see:
#     <http://creativecommons.org/publicdomain/zero/1.0/>
#
# Written in 2021 by Alfonso Sabato Siciliano

OUTPUT =  bsddialog
SOURCES = bsddialog.c util_builders.c util_cli.c util_theme.c
OBJECTS = $(SOURCES:.c=.o)

ifneq ($(ENABLEDEBUG),)
CFLAGS += -g
endif
CFLAGS += -D_GNU_SOURCE -Wall -Wextra -I$(LIBPATH)

ifneq ($(DISABLERPATH),)
LDFLAGS += -ltinfo -L$(LIBPATH) -lbsddialog
else
LDFLAGS += -ltinfo -Wl,-rpath=$(LIBPATH) -L$(LIBPATH) -lbsddialog
endif

RM = rm -f

all : $(OUTPUT)

$(OUTPUT): $(OBJECTS)
	$(CC) $^ -o $@ $(LDFLAGS)


%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	$(RM) $(OUTPUT) *.o *~
