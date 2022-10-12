#!/bin/bash

#gcc object/test_data.o object/test_data.o object/test_data.o binary/test_data

CC = gcc
INC = include
OBJ = object
BIN = binary
SRC = source
LIB = lib

.PHONY : clean all proto folders

#FASE 1
#deps_test_data = $(OBJ)/test_data.o $(OBJ)/data.o
#deps_test_entry = $(OBJ)/test_entry.o $(OBJ)/data.o $(OBJ)/entry.o
#deps_test_tree = $(OBJ)/test_tree.o $(OBJ)/data.o $(OBJ)/entry.o $(OBJ)/tree.o
#deps_test_serialization = $(OBJ)/test_serialization.o $(OBJ)/serialization.o $(OBJ)/data.o $(OBJ)/entry.o $(OBJ)/tree.o

#CFLAGS = -Wall -I include -g


#all : test_data test_entry test_serialization test_tree

#test_data: $(BIN)/test_data
#$(BIN)/test_data: $(deps_test_data)
#	$(CC) -o $@ $^

#test_entry: $(BIN)/test_entry
#$(BIN)/test_entry: $(deps_test_entry)
#	$(CC) -o $@ $^

#test_serialization: $(BIN)/test_serialization
#$(BIN)/test_serialization: $(deps_test_serialization)
#	$(CC) -o $@ $^

#test_tree: $(BIN)/test_tree
#$(BIN)/test_tree: $(deps_test_tree)
#	$(CC) -o $@ $^

#FASE 2
CFLAGS = -DTHREADED -Wall -g -I $(INC) -I/usr/local/include
#LFLAGS = -I $(INC) -I/usr/local/include -lprotobuf-c -L/usr/local/lib
#LFLAGS = -pthread -I$(INC) -I/usr/include -I/usr/local/include -lprotobuf-c -L/usr/lib -L/usr/local/lib #FASE3 
#LDFLAGS = -I$(INC) -I/usr/include -I/usr/local/include -lprotobuf-c -L/usr/lib -L/usr/local/lib #FASE3
LFLAGS = -pthread -I$(INC) -I/usr/include -I/usr/local/include -lprotobuf-c -L/usr/lib -L/usr/local/lib -lzookeeper_mt #FASE4 
LDFLAGS = -I$(INC) -I/usr/include -I/usr/local/include -lprotobuf-c -L/usr/lib -L/usr/local/lib -lzookeeper_mt #FASE4

deps_client_lib = $(OBJ)/client_stub.o $(OBJ)/network_client.o $(OBJ)/sdmessage.pb-c.o $(OBJ)/data.o $(OBJ)/entry.o $(OBJ)/message.o #$(OBJ)/tree.o
#deps_server_lib = $(OBJ)/tree_skel.o $(OBJ)/network_server.o $(OBJ)/sdmessage.pb-c.o $(OBJ)/data.o $(OBJ)/entry.o $(OBJ)/message.o $(OBJ)/tree.o
deps_tree_client = $(OBJ)/tree_client.o $(LIB)/client-lib.o
#deps_tree_server = $(OBJ)/tree_server.o $(LIB)/server-lib.o
deps_tree_server = $(OBJ)/tree_server.o $(LIB)/client-lib.o $(OBJ)/tree_skel.o $(OBJ)/network_server.o $(OBJ)/tree.o

all: proto tree-server tree-client

# Regras de compilação das bibliotecas
client-lib.o: client-lib
client-lib: $(LIB)/client-lib.o
$(LIB)/client-lib.o: $(deps_client_lib)
	ld -r $^ -o $(LIB)/client-lib.o $(LDFLAGS)

server-lib.o: server-lib
server-lib: $(LIB)/server-lib.o
$(LIB)/server-lib.o: $(deps_server_lib)
	ld -r $^ -o $(LIB)/server-lib.o $(LDFLAGS)


# Regras de compilação dos ficheiros executáveis
tree-client: $(BIN)/tree-client
$(BIN)/tree-client: $(deps_tree_client)
	$(CC) $^ -o $@ $(LFLAGS)

tree-server: $(BIN)/tree-server
$(BIN)/tree-server: $(deps_tree_server)
	$(CC) $^ -o $@ $(LFLAGS)

# Regras de compilação dos ficheiros objectos
$(OBJ)/%.o : $(SRC)/%.c
	$(CC) -c $< -o $@ $(CFLAGS)


run-client: tree-client
	$(BIN)/tree-client 127.0.0.1:2181

run-server1: tree-server
	$(BIN)/tree-server 1235 127.0.0.1:2181

run-server2: tree-server
	$(BIN)/tree-server 1236 127.0.0.1:2181 

folders:
	mkdir -p $(OBJ); mkdir -p $(BIN); mkdir -p $(LIB)


# Regras de compilação do ProtoBuffer
proto: 	
	protoc --c_out=source sdmessage.proto
	mv source/sdmessage.pb-c.h include


clean:
	rm $(LIB)/* $(OBJ)/*.o $(BIN)/* $(SRC)/~*.c ~makefile 2>/dev/null; true
