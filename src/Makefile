# Variables
LIB_NAME = libasync
INCLUDE = -I../include
TARGETS = taskloop.o promise.o event.o

# Platform specific
PLATFORM=$(shell uname -s)
ifeq ($(PLATFORM), Darwin)
INCLUDE += -I/usr/local/include
endif

# Built-in variables
CXXFLAGS = -std=c++11 -fpic -O3 $(INCLUDE)

# Library target
static: $(TARGETS)
	ar rcs $(LIB_NAME).a $(TARGETS)
shared: $(TARGETS)
ifeq ($(PLATFORM), Darwin)
	$(CXX) -dynamiclib -o $(LIB_NAME).dylib $(TARGETS)
else
	$(CXX) -shared -o $(LIB_NAME).so $(TARGETS)
	strip $(LIB_NAME).so
endif

# Other targets
clean:
	rm -f $(TARGETS) $(LIB_NAME).*
