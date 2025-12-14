CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11 $(shell pkg-config --cflags libevdev)
LDFLAGS = $(shell pkg-config --libs libevdev)

TARGET = unikey
SRCS = main.c telex.c keyboard.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

install: $(TARGET)
	install -Dm755 $(TARGET) /usr/local/bin/$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all install clean
