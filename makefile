CC=gcc
DEPS = headers.h
OBJ = p2p.o

%.o: %.c $(DEPS)
	    $(CC) -c -o $@ $< $(CFLAGS)

p2p: $(OBJ)
	    gcc -o $@ $^ $(CFLAGS)
