all: http

LIBS = -levent -lssl -lcrypto -levent_openssl
LDFLAGS=-L/usr/local/lib
INCS=-I/usr/local/include

SRCDIR = src
SRCS := $(shell find $(SRCDIR) -name "*.c")

http: 
	gcc -g ${SRCS} -Wall $(LDFLAGS) $(INCS) $(LIBS) -o $@ 
clean:
	rm http