CFLAGS += -Wall -std=c99 -pedantic
LDFLAGS +=

all: crchack

crchack: crchack.o bigint.o crc.o forge.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	$(RM) crchack *.o
