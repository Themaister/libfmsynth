BUNDLE := fmsynth.lv2
INSTALL_DIR = /usr/lib/lv2

OBJDIR := obj
SOURCE := fmsynth_lv2.cpp
OBJECTS := $(addprefix $(OBJDIR)/,$(SOURCE:.cpp=.o))
GUI_SOURCE := fmsynth_gui.cpp
GUI_OBJECTS := $(addprefix $(OBJDIR)/,$(GUI_SOURCE:.cpp=.o))

FMSYNTH_LIB := fmsynth.so
FMSYNTH_GUI_LIB := fmsynth_gui.so

PRESETS := ../presets

LV2_GUI_CFLAGS := $(shell pkg-config --cflags lvtk-gtkui-1)
LV2_GUI_LIBS := $(shell pkg-config --libs lvtk-gtkui-1)
LV2_CFLAGS := $(shell pkg-config lvtk-plugin-1 --cflags)
LV2_LIBS := $(shell pkg-config lvtk-plugin-1 --libs)

LDFLAGS += -fPIC -Wl,-no-undefined -lm
CXXFLAGS += -fPIC -std=c++11 -Wall -pedantic $(LV2_CFLAGS) -I../include

ifeq ($(ARCH),)
   ARCH := $(shell uname -m)
endif

ifneq ($(findstring armv7,$(ARCH)),)
   CXXFLAGS += -mfpu=neon -march=armv7-a -marm
endif

ifeq ($(DEBUG), 1)
   CXXFLAGS += -O0 -g
else
   CXXFLAGS += -Ofast
endif

all: $(BUNDLE)

$(BUNDLE): manifest.ttl $(FMSYNTH_LIB) $(FMSYNTH_GUI_LIB)
	rm -rf $(BUNDLE)
	mkdir $(BUNDLE)
	install -m644 fmsynth.ttl manifest.ttl $(FMSYNTH_LIB) $(FMSYNTH_GUI_LIB) $(BUNDLE)
	cp -r $(PRESETS) $(BUNDLE)

fmsynth.peg: fmsynth.ttl
	ttl2c $< $@

$(FMSYNTH_LIB): $(OBJECTS)
	$(MAKE) -C ../
	$(CXX) -o $@ $(OBJECTS) ../libfmsynth.a -shared $(LDFLAGS) $(LV2_LIBS)

$(FMSYNTH_GUI_LIB): $(GUI_OBJECTS) fmsynth.peg
	$(MAKE) -C ../
	$(CXX) -o $@ $(GUI_OBJECTS) ../libfmsynth.a -shared $(LDFLAGS) $(CXXFLAGS) $(LV2_GUI_LIBS)

$(OBJDIR)/%.o: %.cpp fmsynth.peg
	@mkdir -p $(dir $@)
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(LV2_GUI_CFLAGS)

install:
	cp -r $(BUNDLE) $(INSTALL_DIR)

clean:
	rm -rf $(BUNDLE) fmsynth.so fmsynth_gui.so $(OBJDIR) *.peg
	$(MAKE) -C ../ clean

.PHONY: clean benchmark

