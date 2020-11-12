#include "http.h"

static void
do_term(int sig, short events, void *arg)
{
    // TODO: 从官方示例里抄过来，啥意思暂时没读懂，返回值类型可能要改
	struct event_base *base = arg;
	event_base_loopbreak(base);
	// fprintf(stderr, "Got %i, Terminating\n", sig);
	printf("XXXXX Got %i, Terminating XXXXXXXXXXX\n", sig);
}
static int display_listen_sock(struct evhttp_bound_socket *handle)
{
    // TODO: 从官方示例里抄过来，啥意思暂时没读懂，返回值类型可能要改
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
void request_cb(struct evhttp_request* req, void*arg) 
{
    // TODO: 这里处理http服务器接收到的请求，分为get和post
}
struct options parse_opts(int argc, char* argv) 
{
    // TODO: 在这里处理命令行参数
}


int 
main(int argc, char* argv[])
{
    int ret = 0;
    struct event *term = NULL;
    struct evhttp_bound_socket *handle = NULL;
    struct event_config *cfg = NULL;
    struct event_base *base = NULL;
    struct evhttp *http = NULL;

    struct options opt = parse_opts(argc, argv);

    cfg = event_config_new();
    base = event_base_new_with_config(cfg);
	if (!base) {
		// fprintf(stderr, "Couldn't create an event_base: exiting\n");
		printf("Couldn't create an event_base: exiting\n");
		ret = 1;
	}
	event_config_free(cfg);
	cfg = NULL;

    http = evhttp_new(base);
	if (!http) {
		// fprintf(stderr, "couldn't create evhttp. Exiting.\n");
		printf("couldn't create evhttp. Exiting.\n");
		ret = 1;
	}

	// TODO: 此处实现https中的ssl功能

    evhttp_set_gencb(http, request_cb, &opt);

    handle = evhttp_bind_socket_with_handle(http, "0.0.0.0", opt.port);
    if (!handle) {
        // fprintf(stderr,"couldn't bind to port %d. Exiting.\n", o.port);
        printf("couldn't bind to port %d. Exiting.\n", o.port);
        ret = 1;
        goto err;
    }

	if (display_listen_sock(handle)) {
		ret = 1;
		goto err;
	}

    term = evsignal_new(base, SIGINT, do_term, base);
    if (!term)
		goto err;
	if (event_add(term, NULL))
		goto err;

	event_base_dispatch(base);

err:
	if (cfg)
		event_config_free(cfg);
	if (http)
		evhttp_free(http);
	if (term)
		event_free(term);
	if (base)
		event_base_free(base);
	// TODO: 此处加入ssl出错的处理
	return ret;
}


