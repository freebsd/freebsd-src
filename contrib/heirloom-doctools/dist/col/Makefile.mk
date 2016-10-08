BIN = col

OBJ = col.o

FLAGS = $(DEFINES) -I../include

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) ${CFLAGS} $(LDFLAGS) $(OBJ) -o $(BIN)

install:
	$(INSTALL) -c $(BIN) $(ROOT)$(BINDIR)/$(BIN)
	$(STRIP) $(ROOT)$(BINDIR)/$(BIN)

clean:
	rm -f $(OBJ) $(BIN) core log *~

mrproper: clean

.c.o:
	${CC} ${CFLAGS} ${CPPFLAGS} $(FLAGS) -c $<
