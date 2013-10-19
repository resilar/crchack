RM = rm -f
#CC = clang --analyze
CC = gcc
CFLAGS = -g -Wall -std=c99 -pedantic
LDFLAGS =

EXEC = crchack
SRCS = crchack.c forge32.c crc32.c
OBJS := $(SRCS:.c=.o)

all: $(SRCS) $(EXEC)

$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

.PHONY: clean
clean:
	$(RM) *.o
