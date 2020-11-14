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