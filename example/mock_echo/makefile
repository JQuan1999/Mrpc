SRC = mock_echo_test.cc
OBJ = mock_echo_test.o
BIN = mock_echo_test

PROTO_SRC = echo.proto
PROTO_OBJ = echo.pb.o

CXX_FLAGS = -g -W -Wall -O0
OUTPUT = /home/ubuntu/Desktop/mrpc/output
INCLUDE = -I$(OUTPUT)/include
CXX_FLAGS += $(INCLUDE)

LIB = -L$(OUTPUT)/lib/ -lgtest -lprotobuf -lboost_system -lmrpc -lpthread
LDFLAGS += $(LIB)

all: $(BIN)

$(BIN): $(PROTO_OBJ) $(OBJ)
	g++ $(PROTO_OBJ) $(OBJ) -o $@ $(LDFLAGS)

%.pb.o: %.pb.cc
	g++ -c $^ -o $@ $(CXX_FLAGS)

%.pb.cc: %.proto
	protoc --cpp_out=. $<

%.o: %.cc
	g++ -c $^ -o $@ $(CXX_FLAGS)

clean:
	rm -f $(BIN) $(PROTO_OBJ) $(OBJ)