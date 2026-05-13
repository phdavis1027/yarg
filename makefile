CC=gcc
CC_FLAGS=-shared -fPIC

GTK_CFLAGS=$(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS=$(shell pkg-config --libs gtk+-3.0)

MPV_CFLAGS=$(shell pkg-config --cflags mpv)
MPV_LIBS=$(shell pkg-config --libs mpv)

yarg:
	$(CC) $(CC_FLAGS) \
		$(GTK_CFLAGS) \
		$(MPV_CFLAGS) \
		yarg.c -o yarg.so \
		$(GTK_LIBS) \
		$(MPV_LIBS)

macro-expand:
	$(CC) \
		$(GTK_CFLAGS) \
		$(MPV_CFLAGS) \
		-E \
		yarg.c -o yarg.e \
		$(GTK_LIBS) \
		$(MPV_LIBS)
