# ── http-server Makefile ─────────────────────────────────────────────────────────
CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2 -Iinclude
LDFLAGS  := -lpthread
TARGET   := http-server
SRCDIR   := src
SRCS     := $(wildcard $(SRCDIR)/*.cpp)
OBJS     := $(SRCS:$(SRCDIR)/%.cpp=build/%.o)
PREFIX   ?= /usr/local

# ── Default target ────────────────────────────────────────────────────────────
.PHONY: all debug run clean install

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

build/%.o: $(SRCDIR)/%.cpp | build
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

build:
	mkdir -p build

-include $(OBJS:.o=.d)

# ── Debug build (AddressSanitizer + UBSan) ────────────────────────────────
debug: CXXFLAGS += -g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer
debug: LDFLAGS  += -fsanitize=address,undefined
debug: $(TARGET)

# ── Quick run ─────────────────────────────────────────────────────────────────
run: all
	./$(TARGET)

# ── Install ────────────────────────────────────────────────────────────────────
install: all
	install -Dm755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)

# ── Clean ─────────────────────────────────────────────────────────────────────
clean:
	rm -rf build $(TARGET)
