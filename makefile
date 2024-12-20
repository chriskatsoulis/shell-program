.SECONDEXPANSION:
TARGETS := release debug 
.PHONY: $(TARGETS) all

export TERM ?= xterm-256color

all: $(TARGETS)

compile_commands.json:
	bear -- $(MAKE) -B all

EXE := shell
SRCS := $(shell find src -type f -name '*.c')
OBJS := $(SRCS:src/%.c=%.o)

CFLAGS = -std=c99 -Wall -Werror=vla
release: CFLAGS += -O3 
debug: CFLAGS += -g -O0

CPPFLAGS :=
release: CPPFLAGS += -DNDEBUG

define PROGRAM_template = 
$(1): $(1)/$$(EXE) | $(1)/

$(1)/$$(EXE): $$(addprefix $(1)/,$$(OBJS)) | $(1)/
	$$(LINK.c) $$^ $$(LOADLIBES) $$(LDLIBS) -o $$@


$$(addprefix $(1)/,$$(OBJS)): $(1)/%.o : src/%.c | $$(foreach obj,$$(addprefix $(1)/,$$(OBJS)),$$(dir $$(obj)))
	$$(COMPILE.c) $$(OUTPUT_OPTION) $$<
endef


$(foreach target,$(TARGETS),$(eval $(call PROGRAM_template,$(target))))

clean:
	rm -vr $(TARGETS)

%/:
	mkdir -vp $@
