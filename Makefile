all: HTTP_SERVER.ELF

LIBS = -levent -lssl -lcrypto -levent_openssl
LDFLAGS=-L/usr/local/lib
INCS=-I/usr/local/include

SRCDIR = src
SRCS := $(shell find $(SRCDIR) -name "*.c")

HTTP_SERVER.ELF: ./src/main.c ./src/http.c
	gcc ${SRCS} -Wall $(LDFLAGS) $(INCS) $(LIBS) -o $@ 
clean:
	rm -f HTTP_SERVER.ELF
