# Compiler and Flags
CXX      := g++
CXXFLAGS := -std=c++17 -Wall -g
LDFLAGS  := -lSDL2 -lSDL2_image

# Output Target
TARGET   := ui_test

# Source Definitions
ENTRY_POINT := src/client/uitest.cpp
CORE_SRCS   := $(wildcard src/client/core/*.cpp)
UI_SRCS     := $(wildcard src/client/ui/*.cpp)
SCENE_SRCS  := $(wildcard src/client/scenes/*.cpp)

# Combined Source List
SRCS     := $(ENTRY_POINT) $(CORE_SRCS) $(UI_SRCS) $(SCENE_SRCS)

# Object Files Generation
OBJS     := $(SRCS:%.cpp=build/%.o)

# Include Paths
INCLUDES := -Isrc/client

# --- Build Rules ---

all: $(TARGET)

# Link Object Files
$(TARGET): $(OBJS)
	@echo "Linking $(TARGET)..."
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)
	@echo "Build successful! Run command: ./$(TARGET)"

# Compile Source Files
build/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean Build Artifacts
clean:
	rm -rf build $(TARGET)

.PHONY: all clean