CFLAGS ?= -g
CFLAGS += -Wall -std=c99 -pedantic
LDLIBS ?=

all: crchack

crchack: crchack.o bigint.o crc.o forge.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

check: crchack
	./check.sh

clean:
	$(RM) crchack *.o

.PHONY: all check clean
