CFLAGS = -std=c99 -Wall -Wextra -O2 -DNDEBUG
LDFLAGS = -pthread -lncurses

BINS = tetris
all: $(BINS)

OBJS = main.o ui.o game.o
deps := $(OBJS:%.o=.%.o.d)

# Control the build verbosity
ifeq ("$(VERBOSE)","1")
    Q :=
    VECHO = @true
else
    Q := @
    VECHO = @printf
endif

%.o: %.c
	$(VECHO) "  CC\t$@\n"
	$(Q)$(CC) -o $@ $(CFLAGS) -c -MMD -MF .$@.d $<

tetris: $(OBJS)
	$(VECHO) "  LD\t$@\n"
	$(Q)$(CC) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) $(BINS) $(OBJS)
	$(RM) $(deps)

-include $(deps)
