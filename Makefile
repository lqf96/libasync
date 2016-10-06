# Variables
LIB_NAME = libasync
DEPS = taskloop.o promise.o event.o generator.o socket.o
PLATFORM_DEPS = socket.o
CXXFLAGS = -std=c++11 -fpic -O3 -Iinclude
STRIP = strip

PLATFORM=$(shell uname -s)
# macOS extra flags
ifeq ($(PLATFORM), Darwin)
CXXFLAGS := -I/usr/local/include -D_XOPEN_SOURCE $(CXXFLAGS)
endif
# Generate full dependencies list
DEPS := $(addprefix src/,$(DEPS)) $(addprefix src/$(PLATFORM)/,$(PLATFORM_DEPS))

# Library target
static: $(DEPS)
	ar rcs $(LIB_NAME).a $(DEPS)
shared: $(DEPS)
ifeq ($(PLATFORM), Darwin)
	$(CXX) -dynamiclib -o $(LIB_NAME).dylib $(DEPS)
else
	$(CXX) -shared -o $(LIB_NAME).so $(DEPS)
	$(STRIP) $(LIB_NAME).so
endif

# Other targets
clean:
	rm -f $(DEPS) $(LIB_NAME).*
