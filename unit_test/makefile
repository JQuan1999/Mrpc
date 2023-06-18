CXX = g++

CXX_FLAGS = -g -W -Wall -O2
INCLUDE = -I../output/include
CXX_FLAGS += $(INCLUDE)

LDFLAGS = -L../output/lib -lgtest -lprotobuf -lboost_system -lmrpc -lpthread
TARGET = test_buffer test_endpoint test_service_pool test_threadgroup

PROTO = test_buffer.proto
PROTO_HEADER = test_buffer.pb.h
PROTO_SRC = test_buffer.pb.cc
PROTO_OBJ = test_buffer.o

all: $(TARGET)

test_buffer: $(PROTO_OBJ) test_buffer.cc
	$(CXX) $^ -o $@ $(CXX_FLAGS) $(LDFLAGS)

test_endpoint: test_endpoint.cc
	$(CXX) $^ -o $@ $(CXX_FLAGS) $(LDFLAGS)

test_service_pool: $(PROTO_OBJ) test_service_pool.cc
	$(CXX) $^ -o $@ $(CXX_FLAGS) $(LDFLAGS)

test_threadgroup: test_threadgroup.cc
	$(CXX) $^ -o $@ $(CXX_FLAGS) $(LDFLAGS)

$(PROTO_OBJ): $(PROTO_SRC)
	$(CXX) -c $< -o $@

$(PROTO_SRC): $(PROTO)
	protoc --cpp_out=. $<

clean:
	rm -f $(TARGET) $(PROTO_SRC) $(PROTO_HEADER) $(PROTO_OBJ)