TARGET = tcp_stack
all: $(TARGET) cleano

CC = gcc
LD = gcc

CFLAGS = -g -O2 -Wall -Iinclude
LDFLAGS = 

LIBS = -lpthread

HDRS = ./include/*.h

SRCS = arp.c arpcache.c device_internal.c icmp.c ip_base.c ip.c main.c rtable.c rtable_internal.c \
	   tcp.c tcp_apps2.c tcp_in.c tcp_out.c tcp_sock.c tcp_timer.c

OBJS = $(patsubst %.c,%.o,$(SRCS))

$(OBJS) : %.o : %.c include/*.h
	$(CC) -c $(CFLAGS) $< -o $@

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $(TARGET) $(LIBS) 

clean:
	rm -f *.o $(TARGET)

cleano:
	rm -f *.o