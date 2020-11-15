#include "http.h"

void print_usage(FILE* out, const char* prog, int exit_code) {
	// 从官方示例里抄过来，运于打印该http服务器的使用方式
    // fprintf(out, "usage: %s [-p port_num] <docroot>\n", prog);
	fprintf(out,
		"Syntax: %s [ OPTS ] <docroot>\n"
		" -p      - port\n", prog);
    exit(exit_code);
}

void do_term(int sig, short events, void *arg)
{
    // TODO: 从官方示例里抄过来，啥意思暂时没读懂
	struct event_base *base = arg;
	event_base_loopbreak(base);
	// fprintf(stderr, "Got %i, Terminating\n", sig);
	printf("XXXXX Got %i, Terminating XXXXXXXXXXX\n", sig);
}

int display_listen_sock(struct evhttp_bound_socket *handle)
{
    // TODO: 从官方示例里抄过来，啥意思暂时没读懂
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
    // 在这里处理命令行参数
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
	// 指定服务器文件位置
    o.docroot = argv[optind];
    return o;
}

// 使用evbuffer_peek函数读取evbuffer中的内容并打印，该函数不会修改buffer中的数据
void print_evbuffer(struct evbuffer* buf) {
	int len = 1024;
	struct evbuffer_iovec v[len];
	int n = evbuffer_peek(buf, -1, NULL, v, len);
	printf("---begin evbuffer print---\n");
	for(int i = 0; i<n; i++) {
		printf("evbuffer content is:\n");
		printf("%s", (char *)v[i].iov_base);
	}
	printf("---end evbuffer print---\n");
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
	// 其他情况追加什么字符暂时先不考虑
	// if((style == EVBUFFER_EOL_CRLF_STRICT) && (p != NULL)) {
	// 	strncat(p, "\r\n", 2);
	// }
	return p;
}