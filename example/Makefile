.PHONY: all client server

INCLUDE_LIB_PATH = -I../src -L../src -I../src/hash -L../src/hash -I../lib/libev/include -L../lib/libev/lib -lgko -lev -lgko_zip -lpthread
all: client client2 server

server:
	g++ server.cpp -g -O0 $(INCLUDE_LIB_PATH) -o server
	
client:
	g++ client.cpp -g -O0 $(INCLUDE_LIB_PATH) -o client

client2:
	g++ client2.cpp -g -O0 $(INCLUDE_LIB_PATH) -o client2
