CC = gcc
CFLAGS = -Wall -O2 $(shell pkg-config --cflags libmpdclient)
LDFLAGS = $(shell pkg-config --libs libmpdclient)
TARGET = mpdtags
STATIC = mpdtags-static
VERSION = 0.1.0

.PHONY: all clean release

all: $(TARGET) $(STATIC)

$(TARGET): mpdtags.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(STATIC): mpdtags.c
	$(CC) $(CFLAGS) -static -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(STATIC)

release: all
	mkdir -p release/mpdtags-$(VERSION)
	cp $(TARGET) $(STATIC) README.md LICENSE release/mpdtags-$(VERSION)/
	tar czf release/mpdtags-$(VERSION).tar.gz -C release mpdtags-$(VERSION)
