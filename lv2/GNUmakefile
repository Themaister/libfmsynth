BUNDLE := fmsynth.lv2
INSTALL_DIR = /usr/lib/lv2

SOURCE := fmsynth_lv2.cpp
OBJECTS := $(SOURCE:.cpp=.o)

PRESETS := ../presets

LV2_GUI_CFLAGS := $(shell pkg-config --cflags lvtk-gtkui-1)
LV2_GUI_LIBS := $(shell pkg-config --libs lvtk-gtkui-1)
LV2_CFLAGS := $(shell pkg-config lvtk-plugin-1 --cflags)
LV2_LIBS := $(shell pkg-config lvtk-plugin-1 --libs)

LDFLAGS += -fPIC -Wl,-no-undefined -lm
CXXFLAGS += -fPIC -std=c++11 -Wall -pedantic $(LV2_CFLAGS)

HOST_CXX = $(CXX)

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

$(BUNDLE): manifest.ttl fmsynth.so fmsynth_gui.so
	rm -rf $(BUNDLE)
	mkdir $(BUNDLE)
	install -m644 fmsynth.ttl manifest.ttl fmsynth.so fmsynth_gui.so $(BUNDLE)
	cp -r $(PRESETS) $(BUNDLE)

fmsynth.peg: fmsynth.ttl
	ttl2c $< $@

fmsynth.so: $(OBJECTS) fmsynth_lib
	$(CXX) -o $@ $(OBJECTS) ../libfmsynth.a -shared $(LDFLAGS) $(LV2_LIBS)

fmsynth_gui.so: fmsynth_gui.cpp fmsynth.peg fmsynth_lib
	$(CXX) -o $@ fmsynth_gui.cpp ../libfmsynth.a -shared $(LDFLAGS) $(LV2_GUI_CFLAGS) $(CXXFLAGS) $(LV2_GUI_LIBS)

fmsynth_lib:
	$(MAKE) -C ../

%.o: %.cpp fmsynth.peg
	$(CXX) -c -o $@ $< $(CXXFLAGS)

install:
	cp -r $(BUNDLE) $(INSTALL_DIR)

clean:
	rm -rf $(BUNDLE) fmsynth.so fmsynth_gui.so $(OBJECTS) *.peg
	$(MAKE) -C ../ clean

.PHONY: clean benchmark fmsynth_lib

