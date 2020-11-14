#ifndef __HTTP_H__
#define __HTTP_H__

// printf()
#include <stdio.h>
// atoi()
#include <stdlib.h>
// memset() strcmp()
#include <string.h>
// socket()
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>  /* open() */
#include <unistd.h> /* close() */
#include <signal.h> /* for signal handlers */
#include <dirent.h> /* for directory entries */
#include <netinet/in.h>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>
#include <event2/bufferevent_ssl.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h> /* for RAND_poll() */

#define CBUF_LEN 1024

struct options {
	int port;
	int iocp;
	int verbose;

	int unlink;
	const char *unixsock;
	const char *docroot;
};

char uri_root[512];


void print_usage(FILE* out, const char* prog, int exit_code);

void do_term(int sig, short events, void *arg);

int display_listen_sock(struct evhttp_bound_socket *handle);

struct options parse_opts(int argc, char** argv);

#endif