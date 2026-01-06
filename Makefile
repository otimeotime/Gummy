CXX ?= g++

# Common flags
CXXFLAGS ?= -std=c++17 -O2
CPPFLAGS ?= -Isrc
LDFLAGS  ?=
LDLIBS   ?=

# Server deps
INGAME_SERVER_BIN := ingame_server_demo
INGAME_SERVER_SRCS := \
	src/ingame_server/main.cpp \
	src/ingame_server/core/GameServer.cpp \
	src/common/network/TCPSocket.cpp \
	src/common/network/TCPSocketUtils.cpp

SERVICE_SERVER_BIN := service_server_app
SERVICE_SERVER_SRCS := \
	src/service_server/main.cpp \
	src/service_server/core/ServiceServer.cpp \
	src/service_server/logic/AuthServer.cpp \
	src/service_server/database/DatabaseServer.cpp \
	src/service_server/database/UserDAO.cpp \
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
	src/client/scenes/SceneGame.cpp \
	src/client/scenes/TerminalScene.cpp \
	src/client/ui/Button.cpp \
	src/client/ui/Text.cpp \
	src/client/ui/TextInput.cpp \
	src/common/network/TCPSocket.cpp \
	src/common/network/TCPSocketUtils.cpp

SERVICE_CLIENT_BIN := service_client
SERVICE_CLIENT_SRCS := \
	src/client/service_client_main.cpp \
	src/client/core/Game.cpp \
	src/client/core/InputHandler.cpp \
	src/client/core/StateMachine.cpp \
	src/client/core/TextureManager.cpp \
	src/client/core/Window.cpp \
	src/client/network/ClientSocket.cpp \
	src/client/scenes/SceneLogin.cpp \
	src/client/scenes/SceneRegister.cpp \
	src/client/scenes/SceneDashboard.cpp \
	src/client/ui/Button.cpp \
	src/client/ui/Text.cpp \
	src/client/ui/TextInput.cpp \
	src/common/network/TCPSocket.cpp \
	src/common/network/TCPSocketUtils.cpp

SDL_CFLAGS := $(shell pkg-config --cflags sdl2 SDL2_image SDL2_ttf 2>/dev/null)
SDL_LIBS   := $(shell pkg-config --libs   sdl2 SDL2_image SDL2_ttf 2>/dev/null)
PQXX_LIBS  := -lpqxx -lpq

.PHONY: all server client service_server service_client clean

all: server client service_server service_client

server: $(INGAME_SERVER_BIN)
client: $(CLIENT_BIN)
service_server: $(SERVICE_SERVER_BIN)
service_client: $(SERVICE_CLIENT_BIN)

$(INGAME_SERVER_BIN): $(INGAME_SERVER_SRCS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -pthread $(INGAME_SERVER_SRCS) $(LDFLAGS) $(LDLIBS) -o $@

$(SERVICE_SERVER_BIN): $(SERVICE_SERVER_SRCS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -pthread $(SERVICE_SERVER_SRCS) $(LDFLAGS) $(LDLIBS) $(PQXX_LIBS) -o $@

$(CLIENT_BIN): $(CLIENT_SRCS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -pthread $(SDL_CFLAGS) $(CLIENT_SRCS) $(SDL_LIBS) $(LDFLAGS) $(LDLIBS) -o $@

$(SERVICE_CLIENT_BIN): $(SERVICE_CLIENT_SRCS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -pthread $(SDL_CFLAGS) $(SERVICE_CLIENT_SRCS) $(SDL_LIBS) $(LDFLAGS) $(LDLIBS) -o $@

clean:
	rm -f $(SERVER_BIN) $(CLIENT_BIN)
