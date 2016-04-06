CFLAGS ?= -g -Wall -std=c99 -pedantic
LDFLAGS ?=

EXEC = crchack
SRCS = crchack.c bigint.c crc.c forge.c
OBJS := $(SRCS:.c=.o)

all: $(SRCS) $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

.PHONY: clean
clean:
	$(RM) *.o
