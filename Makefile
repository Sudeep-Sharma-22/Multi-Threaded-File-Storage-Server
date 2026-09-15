CXX       = g++
CXXFLAGS  = -std=c++17 -Wall -Wextra -Wpedantic -g -Isrc
LDFLAGS   = -lws2_32

COMMON_SRC = src/common/protocol.cpp \
             src/common/network_utils.cpp

SERVER_SRC = src/server/main.cpp \
             src/server/server.cpp \
             src/server/thread_pool.cpp \
             src/server/task_queue.cpp \
             src/server/file_manager.cpp \
             src/server/metadata_manager.cpp \
             src/server/logger.cpp

CLIENT_SRC = src/client/main.cpp \
             src/client/client.cpp \
             src/client/cli.cpp

LOAD_TEST_SRC = tests/load_test.cpp

SERVER_BIN    = bin/server.exe
CLIENT_BIN    = bin/client.exe
LOAD_TEST_BIN = bin/load_test.exe

.PHONY: all server client load_test clean dirs

all: dirs server client load_test

server: dirs $(SERVER_BIN)
client: dirs $(CLIENT_BIN)
load_test: dirs $(LOAD_TEST_BIN)

$(SERVER_BIN): $(COMMON_SRC) $(SERVER_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(CLIENT_BIN): $(COMMON_SRC) $(CLIENT_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

$(LOAD_TEST_BIN): $(COMMON_SRC) $(LOAD_TEST_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

dirs:
	@if not exist bin mkdir bin
	@if not exist storage mkdir storage

clean:
	@if exist bin rmdir /s /q bin