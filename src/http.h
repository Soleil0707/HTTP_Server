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
#define MAX_CHUNK_SIZE 512
struct options {
	int port;
	int iocp;
	int verbose;

	int unlink;
	const char *unixsock;
	const char *docroot;
};

char uri_root[512];

static const struct table_entry {
	const char *extension;
	const char *content_type;
} content_type_table[] = {
	{ "txt", "text/plain" },
	{ "c", "text/plain" },
	{ "h", "text/plain" },
	{ "html", "text/html" },
	{ "htm", "text/htm" },
	{ "css", "text/css" },
	{ "gif", "image/gif" },
	{ "jpg", "image/jpeg" },
	{ "jpeg", "image/jpeg" },
	{ "png", "image/png" },
	{ "pdf", "application/pdf" },
	{ "ps", "application/postscript" },
	{ NULL, NULL },
};

char * guess_content_type(const char *path);

int get_buffer_line(struct evbuffer* buffer, char* cbuf);

void send_data_by_chunk(struct evhttp_request* req, char* data, int len);

void print_usage(FILE* out, const char* prog, int exit_code);

void do_term(int sig, short events, void *arg);

int display_listen_sock(struct evhttp_bound_socket *handle);

struct options parse_opts(int argc, char** argv);

void print_evbuffer(struct evbuffer* buf);

char *read_evbuffer_line(struct evbuffer *buf, enum evbuffer_eol_style style);

void write_post2file(struct evbuffer* buf, char* first_boundary, char* last_boundary, FILE* f);

void file_revise(FILE* f, FILE* f_tmp, char* first_boundary, char* last_boundary, char* f_name);

int get_file_size(char* filename);

struct bufferevent* sslcb(struct event_base* base, void* arg);
SSL_CTX* create_ssl();
#endif