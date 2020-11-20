#include "http.h"

void print_usage(FILE* out, const char* prog, int exit_code) {
    // fprintf(out, "usage: %s [-p port_num] <docroot>\n", prog);
	fprintf(out,
		"Syntax: %s [ OPTS ] <docroot>\n"
		" -p      - port\n", prog);
    exit(exit_code);
}

void do_term(int sig, short events, void *arg)
{
	struct event_base *base = arg;
	event_base_loopbreak(base);
	// fprintf(stderr, "Got %i, Terminating\n", sig);
	printf("XXXXX Got %i, Terminating XXXXXXXXXXX\n", sig);
}
int display_listen_sock(struct evhttp_bound_socket *handle)
{
	struct sockaddr_storage ss;
	evutil_socket_t fd;
	ev_socklen_t socklen = sizeof(ss);
	char addrbuf[128];
	void *inaddr;
	const char *addr;
	int got_port = -1;

	fd = evhttp_bound_socket_get_fd(handle);
	memset(&ss, 0, sizeof(ss));
	if (getsockname(fd, (struct sockaddr *)&ss, &socklen)) {
		perror("getsockname() failed");
		return 1;
	}

	if (ss.ss_family == AF_INET) {
		got_port = ntohs(((struct sockaddr_in*)&ss)->sin_port);
		inaddr = &((struct sockaddr_in*)&ss)->sin_addr;
	} else if (ss.ss_family == AF_INET6) {
		got_port = ntohs(((struct sockaddr_in6*)&ss)->sin6_port);
		inaddr = &((struct sockaddr_in6*)&ss)->sin6_addr;
	}
	else {
		// fprintf(stderr, "Weird address family %d\n", ss.ss_family);
		printf("Weird address family %d\n", ss.ss_family);
		return 1;
	}

	addr = evutil_inet_ntop(ss.ss_family, inaddr, addrbuf, sizeof(addrbuf));
	if (addr) {
		printf("Listening on %s:%d\n", addr, got_port);
		evutil_snprintf(uri_root, sizeof(uri_root), "http://%s:%d",addr,got_port);
	} else {
		// fprintf(stderr, "evutil_inet_ntop failed\n");
		printf("evutil_inet_ntop failed\n");
		return 1;
	}

	return 0;
}

struct options parse_opts(int argc, char** argv) 
{
    struct options o;
	int opt;

    memset(&o, 0, sizeof(o));
    while ((opt = getopt(argc, argv, "p:h")) != -1) {
        switch (opt) {
            case 'p': o.port = atoi(optarg);              break;
            case 'h': print_usage(stdout, argv[0], 0);    break;
            default:  printf("Unknown option %c\n", opt); break;
			// fprintf(stderr, "Unknown option %c\n", opt); break;
        }
    }
    if (optind >= argc || (argc - optind) > 1) {
        print_usage(stdout, argv[0], 1);
    }
    o.docroot = argv[optind];

	// o.port = 8081;
	// o.docroot = "loc";
    return o;
}

void print_evbuffer(struct evbuffer* buf) {
	int len = 1024;
	struct evbuffer_iovec v[len];
	int n = evbuffer_peek(buf, -1, NULL, v, len);
	printf("---begin evbuffer print---\n");
	for(int i = 0; i<n; i++) {
		printf("%s", (char *)v[i].iov_base);
	}
	printf("---end evbuffer print---\n");
}

/* Try to guess a good content-type for 'path' */
char *
guess_content_type(const char *path)
{
	const char *last_period, *extension;
	const struct table_entry *ent;
	last_period = strrchr(path, '.');
	if (!last_period || strchr(last_period, '/'))
		goto not_found; /* no exension */
	extension = last_period + 1;
	for (ent = &content_type_table[0]; ent->extension; ++ent) {
		if (!evutil_ascii_strcasecmp(ent->extension, extension))
			return ent->content_type;
	}

not_found:
	return "application/misc";
}

void send_data_by_chunk(struct evhttp_request* req, char* data, int len) {
    struct evbuffer* send_buffer = evbuffer_new();
    evbuffer_add(send_buffer, data, len);
    evhttp_send_reply_chunk(req, send_buffer);
    evbuffer_free(send_buffer);
}


int get_buffer_line(struct evbuffer* buffer, char* cbuf) {
    int i = 0;
    char c = '\0';
    char last_c = '\0';
    int flag = 0;

    while (evbuffer_get_length(buffer)) {
        if(c == '\n') {
            break;
        }
        if(i >= CBUF_LEN - 2) {
            break;
        }

        if (evbuffer_remove(buffer, &c, 1) > 0) {
            if ((last_c == '\r') && (c == '\n')) {
                cbuf[i-1] = c;
                flag = 1;
                break;
            }
            last_c = c;
            cbuf[i++] = c;
        } else {
            c = '\n';
        }
    }
    cbuf[i] = '\0';
    return i+flag;
}


/** 从evbuffer中读取一行
 *	@param style 表示以什么判别一行的结束
 *  EVBUFFER_EOL_ANY,			以任意\r和\n结尾
 *	EVBUFFER_EOL_CRLF,			以\r\n或者\n结尾
 *	EVBUFFER_EOL_CRLF_STRICT,	以\r\n结尾
 *	EVBUFFER_EOL_LF,			以\n结尾
 *	EVBUFFER_EOL_NUL
 *  @return 返回的是读取到的字符串，注意！返回的字符串不带换行符！使用完后需要用free函数手动删除。未读取成功会返回null
 * 
 */
char *read_evbuffer_line(struct evbuffer *buf, enum evbuffer_eol_style style)
{
	char *p = NULL;
	p = evbuffer_readln(buf, NULL, style);
	// if((style == EVBUFFER_EOL_CRLF_STRICT) && (p != NULL)) {
	// 	strncat(p, "\r\n", 2);
	// }
	return p;
}


void write_post2file(struct evbuffer* buf, char* first_boundary, char* last_boundary, FILE* f) 
{
	char *p = NULL;
	int size = evbuffer_get_length(buf);
	char *target = (char *)malloc(size);
	memset(target, '\0', size);
	int detect_boundary = -1;
	int is_writing = -1;

	while((p =read_evbuffer_line(buf, EVBUFFER_EOL_CRLF_STRICT)) != NULL) {
		if (strstr(p, last_boundary) && (detect_boundary == 1)) {
			detect_boundary = -1;
			printf("find boundary, finish write post data.\n");
			// printf("write data length: %d, post data length: %d\n", current_len, content_len);
		}

		printf("-----detect_boundary=%d is_writing=%d read a line:\n", detect_boundary, is_writing);
		printf("%s\n", p);

		if(detect_boundary > 0) {
			if (strstr(p, "Content-Type") && (is_writing == -1) ) {
				is_writing = 0;
				continue;
			} else if ((strlen(p) == 0) && (is_writing == 0)){
				printf("detect null line,begin to write\n");
				is_writing = 1;
				continue;
			}
			if(is_writing > 0) {
				strncat(target, p, strlen(p));
				strcat(target, "\r\n");
				printf("write this line\n");
			}
		}

		if (strstr(p, first_boundary) && (detect_boundary == -1) && (is_writing == -1)) {
			printf("find boundary, begin write post data\n");
			detect_boundary = 1;
		}
	}
	printf("%s\n",target);
	char *tmp = (char *)malloc(size);
	memset(tmp, '\0', size);
	// char *tmp;
	memcpy(tmp, target, strlen(target)-2);
	fputs(tmp, f);

	free(p);
	free(target);
	free(tmp);
}


/**
 * 写入f的evbuffer信息不仅包含了post的信息,也包含了好多其他无用的信息,比如起始和终止boundary的值,以及content-type的信息
 * @param f 最终要保存的文件
 * @param f_tmp 要进行修正的临时文件描述符,已经执行过fopen函数
 * @param first_boundary 起始的边界符
 * @param last_boundary	终止的边界符
 * @param f_name 临时文件的文件名
*/
void file_revise(FILE* f, FILE* f_tmp, char* first_boundary, char* last_boundary, char* f_name) 
{
	int has_begin_bound = 0;
	int has_content_disposition = 0;
	int has_content_type = 0;
	int has_blank_line = 0;
	int can_write = 0;

	int size = get_file_size(f_name)+1;
	char * c = (char *)malloc(size);
	memset(c, '\0', size);
	char * last_c = (char *)malloc(size);
	memset(last_c, '\0', size);
	rewind(f_tmp);

	printf("begin revise\n");
	while(fgets(c, size, f_tmp)) {
		// printf("fd location:%d\n", ftell(f_tmp));
		printf("%s\n", c);

		if(strstr(c, last_boundary) && (has_begin_bound == 1)) {
			memset(c, '\0', size);
			memcpy(c, last_c, strlen(last_c)-2);
			fputs(c, f);
			fflush(f);
			printf("this is last_boundary\n");
			break;
		} else if(strstr(c, first_boundary)) {
			has_begin_bound = 1;
			printf("this is first_boundary\n");
			continue;
		} else if(strstr(c, "Content-Disposition") && (has_content_disposition == 0) && (has_begin_bound == 1)) {
			has_content_disposition = 1;
			printf("this is Content-Disposition\n");
			continue;
		} else if(strstr(c, "Content-Type") && (has_content_type == 0) && (has_begin_bound == 1)) {
			has_content_type = 1;
			printf("this is Content-Type\n");
			continue;
		} else if((strlen(c) == 2) && (has_blank_line == 0) && (has_begin_bound == 1)){
			has_blank_line = 1;
			printf("this is blank line\n");
			continue;
		} else if(has_begin_bound && has_content_type && has_blank_line){
			if(can_write == 1) {
				fputs(last_c, f);
				fflush(f);
			}
			if(can_write == 0) {
				can_write = 1;
			}
			memcpy(last_c, c, size);
			// strncat(last_c, c, strlen(c));
			// printf("c length=%d\n",strlen(c));
			printf("this writing success\n");
		}
	}
	printf("write success!\n");

	free(c);
	c = NULL;
	free(last_c);
	last_c = NULL;
}

/**
 * 获取指定路径文件的大小
*/
int get_file_size(char* filename) 
{
    struct stat statbuf;
    stat(filename,&statbuf);
    int size=statbuf.st_size;
    return size;
}


SSL_CTX* create_ssl(){
	SSL_library_init ();
	SSL_load_error_strings ();
	OpenSSL_add_all_algorithms ();
	// ERR_load_crypto_strings();

	if (!RAND_poll())
		return NULL;
	
	SSL_CTX *ctx = SSL_CTX_new (SSLv23_server_method ());
	if(!ctx) {
		printf("SSL_CTX_new fail!\n");
		return NULL;
	}
	SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);

	if (1 != SSL_CTX_use_certificate_chain_file (ctx, "cert_chain")) {
		printf("please verify the certificate and name it as \"cert_chain\"\n");
		return NULL;
	}
	if (1 != SSL_CTX_use_PrivateKey_file (ctx, "pri_key", SSL_FILETYPE_PEM)) {
		printf("please verify the private key and name it as \"pri_key\"\n");
		return NULL;
	}
	if (1 != SSL_CTX_check_private_key (ctx)) {
		printf("the certificate and the private key are not consist\n");
		return NULL;
	}
	return ctx;
}


/**
 * This callback is responsible for creating a new SSL connection
 * and wrapping it in an OpenSSL bufferevent.  This is the way
 * we implement an https server instead of a plain old http server.
 * from : https://github.com/ppelleti/https-example/blob/master/https-server.c
 */
struct bufferevent* sslcb(struct event_base* base, void* arg) {
	struct bufferevent* r;
	SSL_CTX *ctx = (SSL_CTX *) arg;

	r = bufferevent_openssl_socket_new (base,
                                        -1,
                                        SSL_new (ctx),
                                        BUFFEREVENT_SSL_ACCEPTING,
                                        // BEV_OPT_CLOSE_ON_FREE);
										BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
  return r;
}