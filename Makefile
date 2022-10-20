TARGET = opcuagaugereader
OBJECTS = $(wildcard $(CURDIR)/src/*.cpp)
RM ?= rm

PKGS = gio-2.0 gio-unix-2.0 vdostream open62541 axparameter

CXXFLAGS += -Os -pipe -std=c++11 -Wall -Werror -Wextra
CXXFLAGS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags-only-I $(PKGS))
LDLIBS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs $(PKGS))

CXXFLAGS += -I$(SDKTARGETSYSROOT)/usr/include/opencv4 -I$(CURDIR)/include
LDFLAGS = -L./lib -Wl,--no-as-needed,-rpath,'$$ORIGIN/lib'
LDLIBS += -lm -lopencv_core -lopencv_imgproc -lopencv_video -lpthread

# Set DEBUG_WRITE to write debug images to storage
ifneq ($(DEBUG_WRITE),)
CXXFLAGS += -DDEBUG_WRITE
LDLIBS += -lopencv_imgcodecs
endif

.PHONY: all %.eap dockerbuild clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(LDLIBS) $^ -o $@ ; \
	$(STRIP) --strip-unneeded $@

# docker build container targets
%.eap:
	DOCKER_BUILDKIT=1 docker build --build-arg ARCH=$(@:.eap=) -o type=local,dest=. "$(CURDIR)"

dockerbuild: armv7hf.eap aarch64.eap

clean:
	$(RM) -f $(TARGET) *.eap* *_LICENSE.txt pa*.conf
