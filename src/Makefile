# Variables
LIB_NAME = libasync
TARGETS = taskloop.o promise.o event.o generator.o socket.o
PLATFORM_TARGETS = socket.o
CXXFLAGS = -std=c++11 -fpic -O3 -I../include
STRIP = strip

# Platform specific
PLATFORM=$(shell uname -s)
ifeq ($(PLATFORM), Darwin)
CXXFLAGS := -I/usr/local/include -D_XOPEN_SOURCE $(CXXFLAGS)
endif
TARGETS := $(TARGETS) $(addprefix $(PLATFORM)/,$(PLATFORM_TARGETS))

# Library target
static: $(TARGETS)
	ar rcs $(LIB_NAME).a $(TARGETS)
shared: $(TARGETS)
ifeq ($(PLATFORM), Darwin)
	$(CXX) -dynamiclib -o $(LIB_NAME).dylib $(TARGETS)
else
	$(CXX) -shared -o $(LIB_NAME).so $(TARGETS)
	$(STRIP) $(LIB_NAME).so
endif

# Other targets
clean:
	rm -f $(TARGETS) $(LIB_NAME).*
