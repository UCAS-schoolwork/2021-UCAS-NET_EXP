TARGET := stp

all : $(TARGET) clear

CC = gcc
LD = gcc

CFLAGS = -g  -Iinclude
LDFLAGS = 

LIBS = -lpthread

SRCS = stp.c stp_timer.c main.c device_internal.c mac.c broadcast.c

OBJS = $(patsubst %.c,%.o,$(SRCS))

$(OBJS) : %.o : %.c include/*.h
	$(CC) -c $(CFLAGS) $< -o $@

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $(TARGET) $(LIBS) 

clean:
	rm -f *.o $(TARGET)

clear:
	rm -f *.o

tags: *.c include/*.h
	ctags *.c include/*.h
