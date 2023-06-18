SRC = echo_client.cc echo_server.cc
BIN = echo_client echo_server

PROTO = echo.proto
PROTO_OBJ = echo.pb.o
PROTO_SRC = echo.pb.cc
PROTO_HEADER = echo.pb.h

CXX_FLAGS = -g -W -Wall -O0
OUTPUT = /home/ubuntu/Desktop/mrpc/output
INCLUDE = -I$(OUTPUT)/include
CXX_FLAGS += $(INCLUDE)

LIB = -L$(OUTPUT)/lib/ -lgtest -lprotobuf -lboost_system -lmrpc -lpthread
LDFLAGS += $(LIB)

all: $(BIN)

echo_client: $(PROTO_OBJ) echo_client.cc
	g++ echo_client.cc $(PROTO_OBJ) -o $@ $(CXX_FLAGS) $(LDFLAGS)

echo_server: $(PROTO_OBJ) echo_server.cc
	g++ echo_server.cc $(PROTO_OBJ) -o $@ $(CXX_FLAGS) $(LDFLAGS)

$(PROTO_OBJ): $(PROTO_SRC)
	g++ -c $^ -o $@ $(CXX_FLAGS)

%.pb.cc: %.proto
	protoc --cpp_out=. $<

clean:
	rm -f $(OBJ) $(BIN) $(PROTO_OBJ) $(PROTO_SRC) $(PROTO_HEADER)