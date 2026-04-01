CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2 -Iinclude

TARGET        := simplify
SERVER_TARGET := dashboard_server

SRC        := src/csv_io.cpp src/geometry.cpp src/main.cpp src/simplifier.cpp
SERVER_SRC := src/server.cpp

OBJ        := $(SRC:.cpp=.o)
SERVER_OBJ := $(SERVER_SRC:.cpp=.o)

# Windows: link Winsock and use -lstdc++fs if needed
ifeq ($(OS),Windows_NT)
  LDFLAGS_SERVER := -lws2_32
else
  LDFLAGS_SERVER :=
endif

.PHONY: all server clean run-server help

all: $(TARGET)

server: $(SERVER_TARGET)

# ── Simplifier binary ──────────────────────────────────────────────────────────
$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ)

# ── Dashboard server ───────────────────────────────────────────────────────────
$(SERVER_TARGET): $(SERVER_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(SERVER_OBJ) $(LDFLAGS_SERVER)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ── Convenience targets ────────────────────────────────────────────────────────
run-server: server
	./$(SERVER_TARGET) 8080

clean:
	rm -f $(OBJ) $(SERVER_OBJ) $(TARGET) $(SERVER_TARGET)

help:
	@echo "Targets:"
	@echo "  make all        - build the simplify binary"
	@echo "  make server     - build the dashboard_server binary"
	@echo "  make run-server - build and start the server on :8080"
	@echo "  make clean      - remove all build artifacts"