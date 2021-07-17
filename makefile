CC = gcc
OBJ = lilo.o
CFLAGS = -Wall
LFLAGS = -lX11

lilo: lilo.o
	$(CC) -o $@ $^ $(LFLAGS)

lilo.o: lilo.c
	$(CC) -c -o $@ $< $(CFLAGS)
	
.PHONY: clean

clean:
	rm lilo *.o
