.PHONY: all client server

INCLUDE_LIB_PATH = -I../src -L../src -I../src/hash -L../src/hash -I../lib/libev/include -L../lib/libev/lib -lgko -lev -lgko_zip
all: client server

server:
	g++ server.cpp $(INCLUDE_LIB_PATH) -o server
	
client:
	g++ client.cpp $(INCLUDE_LIB_PATH) -o client
