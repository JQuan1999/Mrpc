SRC = test_buffer.cc ./test_buffer.pb.cc ../common/buffer.cc ../common/logger.cc ../proto/rpc_meta.pb.cc
TARGET = test_buffer
CXX_FLAGS = -g -W -Wall -O0
INCLUDE = -I/home/ubuntu/Desktop
LIB_FLAGS = -lgtest -lprotobuf -lpthread

build:
	g++ $(CXX_FLAGS) $(SRC) -o $(TARGET) $(LIB_FLAGS) $(INCLUDE)

clean:
	rm -f $(TARGET)