BIN = col

OBJ = col.o

FLAGS = $(DEFINES) -I../include

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) ${_CFLAGS} $(_LDFLAGS) $(OBJ) -o $(BIN)

install:
	$(INSTALL) -c $(BIN) $(ROOT)$(BINDIR)/$(BIN)
	$(STRIP) $(ROOT)$(BINDIR)/$(BIN)

clean:
	rm -f $(OBJ) $(BIN) core log *~

mrproper: clean

.c.o:
	${CC} ${_CFLAGS} $(FLAGS) -c $<
