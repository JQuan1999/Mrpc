SRC = $(wildcard ./cc)
OBJ = $(patsubst %.cc,%.o,$(SRC))
BIN = client server

PROTO = test.proto
PROTO_OBJ = test.pb.o
PROTO_SRC = test.pb.cc
PROTO_HEADER = test.pb.h

CXX_FLAGS = -g -W -Wall -O0
OUTPUT = /home/ubuntu/Desktop/mrpc/output
INCLUDE = -I$(OUTPUT)/include
CXX_FLAGS += $(INCLUDE)

LIB = -L$(OUTPUT)/lib/ -lgtest -lprotobuf -lboost_system -lmrpc -lpthread
LDFLAGS += $(LIB)

all: $(BIN)

client: client.o $(PROTO_OBJ)
	g++ client.o $(PROTO_OBJ) -o $@ $(LDFLAGS)

server: server.o $(PROTO_OBJ)
	g++ server.o $(PROTO_OBJ) -o $@ $(LDFLAGS)

%.pb.o: %.pb.cc
	g++ -c $^ -o $@ $(CXX_FLAGS)

%.pb.cc: %.proto
	protoc --cpp_out=. $<

%.o: %.cc
	g++ -c $^ -o $@ $(CXX_FLAGS)

clean:
	rm -f $(BIN) $(PROTO_OBJ) $(OBJ) $(PROTO_SRC) $(PROTO_HEADER)