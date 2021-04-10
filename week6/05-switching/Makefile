

all : test
TARGET = switch
CC = gcc
LD = gcc

CFLAGS = -g -Og -Wall -Iinclude
LDFLAGS = 

LIBS = -lpthread

SRCS = broadcast.c device_internal.c mac.c main.c

OBJS = $(patsubst %.c,%.o,$(SRCS))

$(OBJS) : %.o : %.c include/*.h
	$(CC) -c $(CFLAGS) $< -o $@

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $(TARGET) $(LIBS) 
	rm -f *.o

clean:
	rm -f *.o $(TARGET)

tags: *.c include/*.h
	ctags *.c include/*.h

test: main.c broadcast.c device_internal.c mac.c
	gcc -Iinclude/ -Og -g $^ -lpthread -o switch
