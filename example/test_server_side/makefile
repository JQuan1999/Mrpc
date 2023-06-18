SRC = $(wildcard ./cc)
OBJ = $(patsubst %.cc,%.o,$(SRC))
BIN = client server

PROTO = test.proto
PROTO_OBJ = test.pb.o
PROTO_SRC = test.pb.cc
PROTO_HEADER = test.pb.h

CXX_FLAGS = -g -W -Wall -O2
OUTPUT = ../../output
INCLUDE = -I$(OUTPUT)/include
CXX_FLAGS += $(INCLUDE)

LIB = -L$(OUTPUT)/lib/ -lgtest -lprotobuf -lboost_system -lmrpc -lpthread
LDFLAGS += $(LIB)

all: $(BIN)

client: $(PROTO_OBJ) client.cc
	g++ client.cc $(PROTO_OBJ) -o $@ $(CXX_FLAGS) $(LDFLAGS)

server: $(PROTO_OBJ) server.cc
	g++ server.cc $(PROTO_OBJ) -o $@ $(CXX_FLAGS) $(LDFLAGS)

$(PROTO_OBJ): $(PROTO_SRC)
	g++ -c $^ -o $@ $(CXX_FLAGS)

%.pb.cc: %.proto
	protoc --cpp_out=. $<

clean:
	rm -f client server test.pb.o test.pb.cc test.pb.h client.o server.o