
CFLAGS += -std=c99 -Wall -Wextra -pedantic -fPIC -g
ASFLAGS += -g
LDFLAGS += -lm

FMSYNTH_STATIC_LIB := libfmsynth.a
OBJDIR := obj
FMSYNTH_C_SOURCES := fmsynth.c
FMSYNTH_TEST_SOURCES := fmsynth_test.c
FMSYNTH_TEST_OBJECTS := $(addprefix $(OBJDIR)/,$(FMSYNTH_TEST_SOURCES:.c=.o))
FMSYNTH_TEST := fmsynth_test

SIMD = 1
PREFIX = /usr/local

ifeq ($(ARCH),)
   ARCH := $(shell uname -m)
endif

ifneq ($(findstring armv7,$(ARCH)),)
   CFLAGS += -march=armv7-a -mfpu=neon -marm
   ASFLAGS += -mfpu=neon
   FMSYNTH_ASM_SOURCES += arm/fmsynth_neon.S
endif

ifneq ($(TOOLCHAIN_PREFIX),)
   CC = $(TOOLCHAIN_PREFIX)gcc
   AS = $(TOOLCHAIN_PREFIX)gcc
   AR = $(TOOLCHAIN_PREFIX)ar
endif

FMSYNTH_OBJECTS := \
	$(addprefix $(OBJDIR)/,$(FMSYNTH_C_SOURCES:.c=.o)) \
	$(addprefix $(OBJDIR)/,$(FMSYNTH_ASM_SOURCES:.S=.o))

DEPS := $(FMSYNTH_TEST_OBJECTS:.o=.d) $(FMSYNTH_OBJECTS:.o=.d)

ifneq ($(TUNE),)
   CFLAGS += -mtune=$(TUNE)
else
   CFLAGS += -march=native
endif

ifeq ($(SIMD), 1)
   CFLAGS += -DFMSYNTH_SIMD
endif

ifeq ($(DEBUG), 1)
   CFLAGS += -O0
else
   CFLAGS += -Ofast
endif

all: $(FMSYNTH_STATIC_LIB)

test: $(FMSYNTH_TEST)

-include $(DEPS)

$(FMSYNTH_STATIC_LIB): $(FMSYNTH_OBJECTS)
	$(AR) rcs $@ $^

$(FMSYNTH_TEST): $(FMSYNTH_TEST_OBJECTS) $(FMSYNTH_STATIC_LIB)
	$(CC) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(CFLAGS) -MMD

$(OBJDIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) -c -o $@ $< $(ASFLAGS)

clean:
	rm -f $(FMSYNTH_TEST) $(FMSYNTH_STATIC_LIB)
	rm -rf $(OBJDIR)

install:
	@mkdir -p $(PREFIX)/lib
	@mkdir -p $(PREFIX)/include
	install -m644 $(FMSYNTH_STATIC_LIB) $(PREFIX)/lib/
	install -m644 fmsynth.h $(PREFIX)/include/

docs:
	doxygen

.PHONY: clean install docs

