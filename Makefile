CXX ?= g++

# Common flags
CXXFLAGS ?= -std=c++17 -O2
CPPFLAGS ?= -Isrc
LDFLAGS  ?=
LDLIBS   ?=

# Server deps
SERVER_BIN := ingame_server_demo
SERVER_SRCS := \
	src/ingame_server/main.cpp \
	src/ingame_server/core/GameServer.cpp \
	src/common/network/TCPSocket.cpp \
	src/common/network/TCPSocketUtils.cpp

# Client deps (SDL)
CLIENT_BIN := net_game_client
CLIENT_SRCS := \
	src/client/net_game_main.cpp \
	src/client/core/Game.cpp \
	src/client/core/InputHandler.cpp \
	src/client/core/StateMachine.cpp \
	src/client/core/TextureManager.cpp \
	src/client/core/Window.cpp \
	src/client/network/ClientSocket.cpp \
	src/client/scenes/SceneGameNet.cpp \
	src/client/ui/Button.cpp \
	src/client/ui/Text.cpp \
	src/client/ui/TextInput.cpp \
	src/common/network/TCPSocket.cpp \
	src/common/network/TCPSocketUtils.cpp

SDL_CFLAGS := $(shell pkg-config --cflags sdl2 SDL2_image SDL2_ttf 2>/dev/null)
SDL_LIBS   := $(shell pkg-config --libs   sdl2 SDL2_image SDL2_ttf 2>/dev/null)

.PHONY: all server client clean

all: server client

server: $(SERVER_BIN)
client: $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_SRCS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -pthread $(SERVER_SRCS) $(LDFLAGS) $(LDLIBS) -o $@

$(CLIENT_BIN): $(CLIENT_SRCS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -pthread $(SDL_CFLAGS) $(CLIENT_SRCS) $(SDL_LIBS) $(LDFLAGS) $(LDLIBS) -o $@

clean:
	rm -f $(SERVER_BIN) $(CLIENT_BIN)
