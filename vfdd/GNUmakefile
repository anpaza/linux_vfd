.PHONY: all clean

# release or debug
MODE = debug

CC = $(CROSS_COMPILE)gcc -c
LD = $(CROSS_COMPILE)gcc

CFLAGS.release = -s -O2 -Wall
CFLAGS.debug = -g -Wall

LDFLAGS.release = -s
LDFLAGS.debug = -g

CFLAGS.local = $(CFLAGS.$(MODE)) -Icfg_parse
LDFLAGS.local = $(LDFLAGS.$(MODE))

LDLIBS = -lpthread

OUT = out/$(CROSS_COMPILE)$(MODE)/

all: $(OUT)vfdd

clean:
	rm -rf $(OUT)

$(OUT)%.o: %.c $(OUT).stamp.dir
	$(CC) $(CFLAGS.local) $(CFLAGS) -o $@ $<

$(OUT)%.o: cfg_parse/%.c $(OUT).stamp.dir
	$(CC) $(CFLAGS.local) $(CFLAGS) -o $@ $<

$(OUT).stamp.dir:
	mkdir -p $(@D)
	touch $@

VFDD_SRC = vfdd.c cfg_parse.c cfg.c sysfs.c task.c \
	task-display.c task-suspend.c task-clock.c task-temp.c task-disk.c task-dot.c

$(OUT)vfdd: $(addprefix $(OUT),$(VFDD_SRC:.c=.o))
	$(LD) $(LDFLAGS.local) $(LDFLAGS) -o $@ $^ $(LDLIBS)
