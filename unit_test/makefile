SRC = test_service_pool.cc
TARGET = test_service_pool
CXX_FLAGS = -g -W -Wall -O0
INCLUDE = -I/home/ubuntu/Desktop
LIB_FLAGS = -lgtest -lprotobuf -lpthread

build:
	g++ $(CXX_FLAGS) $(SRC) -o $(TARGET) $(LIB_FLAGS) $(INCLUDE)

clean:
	rm -f $(TARGET)