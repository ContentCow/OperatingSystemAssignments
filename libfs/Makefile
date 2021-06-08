targets := libfs.a
objs 		:= disk.o fs.o
lib 		:= libfs.a
CC			:= gcc
RC 			:= ar rcs
CFLAGS	:= -Wall -Werror

ifeq ($(D),1)
CFLAGS += -O0
CFLAGS += -g
CFLAGS += -DDEBUG
endif

all:$(targets)
deps := $(patsubst %.o,%.d,$(objs),$(provided_objs))
-include $(deps)
DEPFLAGS = -MMD -MF $(@:.o=.d)

libfs.a: $(objs)
	@echo "CC $@"
	$(RC) $(lib) $^

%.o: %.c
	@echo "CC $@"
	$(Q) $(CC) $(CFLAGS) -c -o $@ $< $(DEPFLAGS)

clean:

		@echo "clean"
		$(Q)rm -f $(targets) $(objs) $(deps)
