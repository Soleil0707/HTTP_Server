#include "http.h"

static void
do_term(int sig, short events, void *arg)
{
    // TODO: 从官方示例里抄过来，啥意思暂时没读懂，返回值类型可能要改
}
static int display_listen_sock(struct evhttp_bound_socket *handle)
{
    // TODO: 从官方示例里抄过来，啥意思暂时没读懂，返回值类型可能要改
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

	return ret;
}


