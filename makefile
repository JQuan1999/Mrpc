PREFIX = ./output

CXX = g++

OPT ?= -O0

LIB=libmrpc.so

SRC=$(wildcard ./mrpc/client/*.cc ./mrpc/common/*.cc ./mrpc/server/*.cc)

OBJ=$(patsubst %.cc,%.o,$(SRC))

PROTO=$(wildcard ./mrpc/proto/*.proto)

PROTO_SRC=$(patsubst %.proto,%.pb.cc,$(PROTO))

PROTO_HEADER=$(patsubst %.proto,%.pb.h,$(PROTO))

PROTO_OBJ=$(patsubst %.cc,%.o,$(PROTO_SRC))

INCLUDE=-I.

CXXFLAGS = $(OPT) -g -pipe -W -Wall -fPIC $(INCLUDE)

LDFLAGS = -lprotobuf -lboost_system -lpthread

all: build

$(PROTO_OBJ): $(PROTO_HEADER) $(PROTO_SRC)

$(OBJ): $(PROTO_HEADER)

$(LIB): $(PROTO_OBJ) $(OBJ)
	$(CXX) -shared $(OBJ) $(PROTO_OBJ) $(LDFLAGS) -o $@

%.pb.h %.pb.cc: %.proto
	protoc --cpp_out=. --proto_path=. $<

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

build: $(LIB)
	@echo 'Build succed, run 'make install' to install to $(PREFIX)'

install: $(LIB)
	mkdir -p $(PREFIX)/include/mrpc/client
	cp -r ./mrpc/client/*.h -p $(PREFIX)/include/mrpc/client
	mkdir -p $(PREFIX)/include/mrpc/common
	cp -r ./mrpc/common/*.h -p $(PREFIX)/include/mrpc/common
	mkdir -p $(PREFIX)/include/mrpc/server
	cp -r ./mrpc/server/*.h -p $(PREFIX)/include/mrpc/server
	mkdir -p $(PREFIX)/include/mrpc/proto
	cp -r ./mrpc/proto/*.h -p $(PREFIX)/include/mrpc/proto
	mkdir -p $(PREFIX)/bin
	mkdir -p $(PREFIX)/lib
	cp $(LIB) $(PREFIX)/lib
	@echo
	@echo 'Install succeed, target directory is "'$(PREFIX)'".'

clean:
	rm -f $(LIB)
	rm -f $(PROTO_HEADER)
	rm -f $(PROTO_SRC)
	rm -f $(PROTO_OBJ)
	rm -f ./mrpc/client/*.o
	rm -f ./mrpc/common/*.o
	rm -f ./mrpc/server/*.o