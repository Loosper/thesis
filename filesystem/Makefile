FUSE_FLAGS := $(shell pkg-config fuse3 --cflags --libs) -DFUSE_USE_VERSION=35
CFLAGS += $(FUSE_FLAGS) -Wall -Wextra -Iinclude

COBJS  := $(patsubst %.c, %.o, $(wildcard src/*.c))
# TODO: make dynamic
CHDR := $(wildcard incclude/*.h)

all: filesystem


src/%.o:src/%.c $(CHDR)
	gcc $(CFLAGS) -c $^ -o $@

filesystem: $(COBJS)
	gcc $(CFLAGS) $(COBJS) -o filesystem
