FUSE_FLAGS := $(shell pkg-config fuse3 --cflags --libs) -DFUSE_USE_VERSION=35
override CFLAGS += $(FUSE_FLAGS) -Iinclude -g
# override CFLAGS += -Wall -Wextra -pedantic
CC := gcc

# DEBUG helpers
CFLAGS += -Wno-unused-parameter -O2
# CFLAGS += -Wno-unused-variable

COBJS  := $(patsubst %.c, %.o, $(wildcard src/*.c))
# TODO: make dynamic
CHDR := $(wildcard incclude/*.h)


.PHONY: all
all: filesystem

src/%.o:src/%.c $(CHDR)
	$(CC) $(CFLAGS) -c $^ -o $@

filesystem: $(COBJS)
	$(CC) $(CFLAGS) $(COBJS) -o filesystem

.PHONY: clean
clean:
	rm -f filesystem $(COBJS)

LOOP_NUM := 0

.PHONY: loop_dev
loop_dev:
	losetup -d /dev/loop$(LOOP_NUM) || true
	dd if=/dev/zero of=./loop$(LOOP_NUM)_data count=1M
	losetup /dev/loop$(LOOP_NUM) ./loop$(LOOP_NUM)_data
	chmod 666 /dev/loop$(LOOP_NUM)

