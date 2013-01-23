.PHONY: clean all client server ares_dns stress_confAgent

LIBS= $(shell PKG_CONFIG_PATH=../lib/libgko/lib/pkgconfig pkg-config libgko --libs)
CFLAGS= -g -O2 $(shell PKG_CONFIG_PATH=../lib/libgko/lib/pkgconfig pkg-config libgko --cflags)
BINS = server client client2 ares_dns stress_confAgent
all: $(BINS)

server:
	g++ $@.cpp -g -O0 $(CFLAGS) $(LIBS) -o $@
	
client:
	g++ $@.cpp -g -O0 $(CFLAGS) $(LIBS) -o $@

client2:
	g++ $@.cpp -g -O0 $(CFLAGS) $(LIBS) -o $@

ares_dns:
	g++ $@.cpp -g -O0 $(CFLAGS) $(LIBS) -o $@

stress_confAgent:
	g++ $@.cpp -g -O0 $(CFLAGS) $(LIBS) -o $@

clean:
	-rm -rf *.dSYM
	-rm -f *.o
	-rm -f $(BINS)
