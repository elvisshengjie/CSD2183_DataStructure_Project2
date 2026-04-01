CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2 -Iinclude
TARGET := simplify
SERVER_TARGET := dashboard_server
SRC := src/csv_io.cpp src/geometry.cpp src/main.cpp src/simplifier.cpp
OBJ := $(SRC:.cpp=.o)
SERVER_SRC := src/server.cpp
SERVER_OBJ := $(SERVER_SRC:.cpp=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ)

$(SERVER_TARGET): $(SERVER_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(SERVER_OBJ)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(SERVER_OBJ) $(TARGET) $(SERVER_TARGET)
