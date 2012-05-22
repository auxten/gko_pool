.PHONY: all client server async_dns

INCLUDE_LIB_PATH = -I../src -L../src -I../src/hash -L../src/hash -I../lib/libev/include -I../lib/c-ares/include -L../lib/libev/lib -lgko -lev -lgko_zip -lpthread 
all: client client2 server async_dns

server:
	g++ server.cpp -g -O0 $(INCLUDE_LIB_PATH) -o server
	
client:
	g++ client.cpp -g -O0 $(INCLUDE_LIB_PATH) -o client

client2:
	g++ client2.cpp -g -O0 $(INCLUDE_LIB_PATH) -o client2

async_dns:
	g++ async_dns.cpp -g -O0 $(INCLUDE_LIB_PATH) ../lib/c-ares/lib/libcares.a -o async_dns
