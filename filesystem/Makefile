FUSE_FLAGS := $(shell pkg-config fuse3 --cflags --libs)
CFLAGS += $(FUSE_FLAGS) -Wall -Wextra
CFILES := main.c

all: filesystem
filesystem: $(CFILES)
	gcc $(CFLAGS) $(CFILES) -o filesystem
