BIN = echo_client echo_server
OBJ = echo_client.o echo_server.o

PROTO = echo.proto
PROTO_OBJ = echo.pb.o
PROTO_SRC = echo.pb.cc
PROTO_HEADER = echo.pb.h

CXX_FLAGS = -g -W -Wall -O2 -fPIC
OUTPUT = ../../output
INCLUDE = -I$(OUTPUT)/include
CXX_FLAGS += $(INCLUDE)

LIB = -L$(OUTPUT)/lib/ -lprotobuf -lboost_system -lmrpc -lpthread
LDFLAGS += $(LIB)

all: $(BIN)

echo_client: $(PROTO_OBJ) echo_client.o
	g++ $^ -o $@ $(LDFLAGS)

echo_server: $(PROTO_OBJ) echo_server.o
	g++ $^ -o $@ $(LDFLAGS)

%.o: %.cc
	g++ $(CXX_FLAGS) -c $< -o $@

%.pb.cc: %.proto
	protoc --cpp_out=. $<

clean:
	rm -f $(OBJ) $(BIN) $(PROTO_OBJ) $(PROTO_SRC) $(PROTO_HEADER)