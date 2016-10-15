# Variables
LIB_NAME = libasync
DEPS = taskloop.o promise.o event.o generator.o socket.o reactor.o
PLATFORM_DEPS = socket1.o reactor1.o
CXXFLAGS = -Wall -std=c++11 -fpic -Iinclude
STRIP = strip

PLATFORM=$(shell uname -s)
# macOS extra flags
ifeq ($(PLATFORM), Darwin)
CXXFLAGS := -I/usr/local/include -D_XOPEN_SOURCE $(CXXFLAGS)
endif
# Debug mode
ifdef DEBUG
CXXFLAGS := $(CXXFLAGS) -g
else
CXXFLAGS := $(CXXFLAGS) -O3
endif
# Generate full dependencies list
DEPS := $(addprefix src/,$(DEPS)) $(addprefix src/$(PLATFORM)/,$(PLATFORM_DEPS))

# Library target
static: $(DEPS)
	$(AR) rcs $(LIB_NAME).a $(DEPS)
shared: $(DEPS)
ifeq ($(PLATFORM), Darwin)
	$(CXX) -dynamiclib -o $(LIB_NAME).dylib $(DEPS)
	$(STRIP) $(LIB_NAME).dylib
else
	$(CXX) -shared -o $(LIB_NAME).so $(DEPS)
	$(STRIP) $(LIB_NAME).so
endif

# Other targets
clean:
	rm -f $(DEPS) $(LIB_NAME).*
