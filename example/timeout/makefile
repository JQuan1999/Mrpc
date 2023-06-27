SRC = client.cc server.cc
BIN = client server
OBJ = client.o server.o

PROTO = sleep.proto
PROTO_OBJ = sleep.pb.o
PROTO_SRC = sleep.pb.cc
PROTO_HEADER = sleep.pb.h

CXX_FLAGS = -g -W -Wall -O2 -fPIC
OUTPUT = ../../output
INCLUDE = -I$(OUTPUT)/include
CXX_FLAGS += $(INCLUDE)

LIB = -L$(OUTPUT)/lib/ -lprotobuf -lboost_system -lmrpc -lpthread
LDFLAGS += $(LIB)

all: $(BIN)

client: $(PROTO_OBJ) client.o
	g++ $^ -o $@ $(LDFLAGS)

server: $(PROTO_OBJ) server.o
	g++ $^ -o $@ $(LDFLAGS)

%.o: %.cc
	g++ $(CXX_FLAGS) -c $< -o $@

%.pb.cc: %.proto
	protoc --cpp_out=. $<

clean:
	rm -f $(OBJ) $(BIN) $(PROTO_OBJ) $(PROTO_SRC) $(PROTO_HEADER)