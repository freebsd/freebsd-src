all:	gustest gusload gmod midithru

gustest: gustest.c
	$(CC) -o gustest gustest.c -lm

gusload: gusload.c
	$(CC) -o gusload gusload.c

gmod: gmod.c
	$(CC) -o gmod gmod.c

midithru: midithru.c
	$(CC) -o midithru midithru.c

clean:
	rm -f gusload gustest gmod midithru *.o
