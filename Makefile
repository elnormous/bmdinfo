ifeq ($(OS),Windows_NT)
    platform=windows
else
    UNAME := $(shell uname -s)
    ifeq ($(UNAME),Linux)
        platform=linux
    endif
    ifeq ($(UNAME),Darwin)
        platform=macos
    endif
endif
CXXFLAGS=-c -std=c++11 -Wall -I"$(SDK_PATH)/include"
LDFLAGS=-O2
SOURCES=main.cpp
OBJECTS=$(SOURCES:.cpp=.o)
EXECUTABLE=bmdinfo

ifeq ($(platform),linux)
LDFLAGS=-lpthread
else ifeq ($(platform),macos)
LDFLAGS=-framework CoreFoundation
endif

.PHONY: all
all: $(SOURCES) $(EXECUTABLE)

.PHONY: debug
debug: CXXFLAGS+=-DDEBUG -g
debug: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) 
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

.PHONY: clean
clean:
	rm -f $(EXECUTABLE) *.o
