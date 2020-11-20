#include "http.h"

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
/* Try to guess a good content-type for 'path' */
static const char *
guess_content_type(const char *path)
{
	const char *last_period, *extension;
	const struct table_entry *ent;
	//搜索最后一次出现“．”的位置，返回之后的字符串/c/ds/c/sd.jpg返回""jpg"
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


// 获取一行内容,以\n\0作为结束符,返回该行的字节数
// TODO: 该函数的正确性还需要检查
int get_buffer_line(struct evbuffer* buffer, char* cbuf) {
    int i = 0;
    char c = '\0';
    char last_c = '\0';
    int flag = 0;

    // 逐字符读取一行,保存在cbuf中
    while (evbuffer_get_length(buffer)) {
        if(c == '\n') {
            break;
        }
        // cbuf溢出
        if(i >= CBUF_LEN - 2) {
            break;
        }
        // 从evbuffer读取一个字符保存在c，并从evbuffer中移除该字符，返回值为读出的字节数
        // 如果buffer为空，则返回-1
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

int handle_post_request(struct evhttp_request* req, char* whole_path) {
    // TODO: post请求的处理,梁
    struct evkeyvalq* headers = NULL;
	struct evkeyval *header = NULL;
    const char* bound_key = "boundary=";
    char first_boundary[256] = {0};
    char last_boundary[256] = {0};
    int content_len = 0;
    // int current_len = 0;
    // int data_left = -1;
    struct evbuffer *buf = NULL;

    // 获取一个请求的报头，报头里面的内容组成了一个队列
    // 队列的每个元素是一个键值对，例如'cookie':'asdasdas'这种表示方式
    // perror("get_input_buffer?");
    headers = evhttp_request_get_input_headers(req);
    int is_file_data = 0;
    // perror("error?");
    printf("---begin a request header print-----------------\n");
	for (header = headers->tqh_first; header; header = header->next.tqe_next) {
		printf("%s: %s\n", header->key, header->value);
        // perror("error?");
        // 判断报头中的content-type，该值用于定义网络文件的类型和网页的编码
        // 服务器根据编码类型使用特定的解析方式，获取数据流中的数据
        // 例如：Content-Type:multipart/form-data; boundary=ZnGpDtePMx0KrHh_G0X99Yef9r8JZsRJSXC
        if (!evutil_ascii_strcasecmp(header->key, "Content-Type")) {
            // TODO: 这里处理表单类型的post请求
            if(strstr(header->value, "x-www-form-urlencoded")) {
                is_file_data = -1;
                continue;
            }
            // TODO: 这里为什么又加上了一个长度
            // 当有多个数据要提交时，会以boundary的值作为分割
            // boundary_value = strstr(header->value, bound_key) + strlen(bound_key);
            char* boundary_value_tmp = strstr(header->value, bound_key);
            printf("boundary_value=%s\n",boundary_value_tmp);
            char *boundary_value = boundary_value_tmp + strlen(bound_key);
            printf("boundary_value=%s\n",boundary_value);

            if (boundary_value==NULL) {
                printf("this post not contain boundary=\n");
                continue;
            }
            printf("boundary value of this post request is: %s \n", boundary_value);
            is_file_data = 1;
            // 将first_boundary赋值为-- + boundary_value
            // 将last_boundary赋值为-- + boundary_value + --
            strcat(first_boundary, "--");
            strncat(first_boundary, boundary_value, strlen(boundary_value)+1);
            strncat(last_boundary, first_boundary, strlen(first_boundary)+1);
            
            // while((*s++ = *t++) != '\0')

            strcat(first_boundary, "\r\n");
            strcat(last_boundary, "--\r\n");

            printf("first_boundary is:%s, sizeof=%d, strlen=%d\n", first_boundary, sizeof(first_boundary), strlen(first_boundary));
        } else if (!evutil_ascii_strcasecmp(header->key, "Content-Length")) {
            // 得到post请求包体中内容所占字节数
            content_len = atoi(header->value);
        }

	}
    printf("---finish a request header print----------------\n");
    // post请求类型为表单
    if(is_file_data < 0) {
        // 将post的数据返回回去
        evhttp_send_reply(req, HTTP_OK, "OK", evhttp_request_get_input_buffer(req));

    // post请求类型为文件
    } else if(is_file_data > 0) {
        // TODO: open函数的路径不能带目录，这里可能报错
        FILE* f;
        if(!(f = fopen(whole_path, "wb+"))) {
            printf("post request can not open file %s to write\n", whole_path);
            evhttp_send_error(req, HTTP_INTERNAL, "Post fails in write into file.");
            return -1;
        }
        // fclose(f);

        FILE * f_tmp;
        char* f_name = whole_path;
        strncat(f_name, "_tmp", sizeof("_tmp"));
        if(!(f_tmp = fopen(f_name, "wb+"))) {
            printf("can not create temp file for write post-data\n");
            evhttp_send_error(req, HTTP_INTERNAL, "Post fails in write into file.");
            return -1;
        }

        buf = evhttp_request_get_input_buffer(req);
        // 将evbuffer的内容全部写入文件,此时还包含很多冗余信息
        evbuffer_write(buf, fileno(f_tmp));

        printf("file_revise\n");

        file_revise(f, f_tmp, first_boundary, last_boundary, f_name);
        // write_post2file(buf, first_boundary, last_boundary, f);

        evhttp_send_reply(req, 200, "OK", NULL);

        // printf("flush f\n");
        // fflush(f);
        printf("close f\n");
        // fclose(f);
        
        // printf("flush f_tmp\n");
        // fflush(f_tmp);
        printf("close f_tmp\n");
        fclose(f_tmp);
        remove(f_name);

    } else {
        evhttp_send_error(req, HTTP_INTERNAL, "Post fails: cannot recognize content-type.\n");
    }
    return 0;

}

/*根据路径和获取的req结构处理get请求
* req有用变量：*input_headers， *output_headers，remote_port
* 若浏览器访问0.0.0.0:8081/kk
* path:从uri直接提取出来的path，为“/kk”;decode_path:sjbdx
* whole_path:服务器端文件或者文件夹路径
*/
int handle_get_request(struct evhttp_request* req, const char* path, char* whole_path, char* decoded_path) {
    // TODO: get请求的处理,杨
    struct stat file_state;

    int fd;
    //根据路径获取文件状态到file_state结构体
    stat(whole_path,&file_state);
    
    struct evbuffer* send_buffer = evbuffer_new();

    DIR *loc_dir = opendir(whole_path);

    //dirent refer:https://blog.csdn.net/hello188988/article/details/47711217
    struct dirent* temp;

    if(S_ISDIR(file_state.st_mode)){//if it is a directory
        //依次加入响应正文，消息报头和状态行

        //响应正文
        evbuffer_add_printf(send_buffer,
                            "<!DOCTYPE html>\n"
                            "<html>\n"
                            "<head>\n"
                                "<meta charset=\"utf-8\">\n"
                                "<meta name=\"author\" content=\"Yang Xiaomao\">\n"
                                "<title>I am your father, I will give you a sweet response</title>\n"
                                "<base href=\"sep.ucas.ac.cn\">\n"
                            "</head>\n"
                            "<body>\n"
                                "<h>file list</h>\n"
                                    "<ul>\n");
        while ((temp = readdir(loc_dir))) {
			evbuffer_add_printf(send_buffer,
			    "<li><a href=\"%s\">%s</a></li>\n",
			    temp->d_name, temp->d_name);
		}
        evbuffer_add_printf(send_buffer,
			    "</ul>\n"
                "</body>\n"
                "</html>\n");

        //消息报头
        evhttp_add_header(evhttp_request_get_output_headers(req),
		    "Content-Type", "text/html");

    }else{//if it is a file
        //analyse the type of file(jpg gif or etc.)
        const char *file_type = guess_content_type(decoded_path);
        puts("whole path is");
        puts(whole_path);

        if((fd = open(whole_path,O_RDONLY)) < 0){
            evhttp_send_error(req, 404, "Document was not found");
        }
        //printf("fd is %d\n",fd);

        struct stat del;

        fstat(fd,&del);

        evhttp_add_header(evhttp_request_get_output_headers(req),
		    "Content-Type", file_type);

        printf("size is %ld\n",del.st_size);
        evbuffer_add_file(send_buffer,fd,0,del.st_size);
    }
    evhttp_send_reply(req, 200, "OK", send_buffer);
//     goto done;
// err:
// 	evhttp_send_error(req, 404, "Document was not found");
// done:
// 	if (decoded_path)
// 		free(decoded_path);
// 	if (whole_path)
// 		free(whole_path);
// 	if (send_buffer)
// 		evbuffer_free(send_buffer);
    return 0;
}


void request_cb(struct evhttp_request* req, void*arg) 
{
    // TODO: 这里处理http服务器接收到的请求，分为get和post
    // struct options *o = (struct options*)arg;
    struct options *o = arg;
    const char *cmdtype;
    const char *uri;
    const char *path; 
    struct evhttp_uri *decoded = NULL;
    char *decoded_path;
    size_t len;
    char *whole_path = NULL;

    // 得到了请求的类型
    switch (evhttp_request_get_command(req)) {
        case EVHTTP_REQ_GET: 	 cmdtype = "GET";     break;
        case EVHTTP_REQ_POST:    cmdtype = "POST";    break;
        case EVHTTP_REQ_HEAD: 	 cmdtype = "HEAD";    break;
        case EVHTTP_REQ_PUT: 	 cmdtype = "PUT";     break;
        case EVHTTP_REQ_DELETE:  cmdtype = "DELETE";  break;
        case EVHTTP_REQ_OPTIONS: cmdtype = "OPTIONS"; break;
        case EVHTTP_REQ_TRACE:   cmdtype = "TRACE";   break;
        case EVHTTP_REQ_CONNECT: cmdtype = "CONNECT"; break;
        case EVHTTP_REQ_PATCH:   cmdtype = "PATCH";   break;
        default:                 cmdtype = "unknown"; break;
    }
    // 得到发起请求的uri
    uri = evhttp_request_get_uri(req);
    // perror("get urio");
    printf("Received a %s request for %s\nHeaders:\n",
	    cmdtype, uri);

    // http状态码405
    if (!evutil_ascii_strcasecmp(cmdtype, "unknown")) {
        printf("[HTTP STATUS] 405 method not allowed\n");
        evhttp_send_error(req, HTTP_BADMETHOD, 0);
        return;
    }

    /* Decode the URI */
    decoded = evhttp_uri_parse(uri);
    // http状态码400
	if (!decoded) {
		printf("It's not a good URI. Sending BADREQUEST\n");
		evhttp_send_error(req, HTTP_BADREQUEST, 0);
		return;
	}

    /* Let's see what path the user asked for. */
    path = evhttp_uri_get_path(decoded);
    printf("path is %s\n",path);
    if (!path) path = "/";

    /* We need to decode it, to see what path the user really wanted. */
	decoded_path = evhttp_uridecode(path, 0, NULL);
	if (decoded_path == NULL)
		goto err;
    printf("decoded path: %s\n", decoded_path);
    // perror("decode path");
    /* Don't allow any ".."s in the path, to avoid exposing stuff outside
	 * of the docroot.  This test is both overzealous and underzealous:
	 * it forbids aceptable paths like "/this/one..here", but it doesn't
	 * do anything to prevent symlink following." */
	if (strstr(decoded_path, ".."))
		goto err; 

    len = strlen(decoded_path)+strlen(o->docroot)+2;
	if (!(whole_path = malloc(len))) {
		perror("malloc");
		goto err;
	}

    // TODO: 此处与官方文档略有不同
    int offset = (decoded_path[0] == '/') ? 1 : 0;
    evutil_snprintf(whole_path, len - offset, "%s/%s", o->docroot,
                    decoded_path + offset);
    printf("the whole path of current request is: %s\n", whole_path);

    // TODO: 判断是get还是post请求，分别进行处理
    // if (evhttp_request_get_command(req) == EVHTTP_REQ_POST) {
    // POST request
    if (!evutil_ascii_strcasecmp(cmdtype, "POST")) {
        handle_post_request(req, whole_path);
    // } else if (evhttp_request_get_command(req) == EVHTTP_REQ_GET) {
    // GET request
    } else if (!evutil_ascii_strcasecmp(cmdtype, "GET")) {
        handle_get_request(req, path, whole_path, decoded_path);
    } else {
        // http状态码501
        evhttp_send_error(req, HTTP_NOTIMPLEMENTED,
                          "Not implemented: only handle GET or POST request "
                          "for upload and download file can be processed.");
    }
    goto done;

err:
	evhttp_send_error(req, 404, "Document was not found");
done:
	if (decoded)
		evhttp_uri_free(decoded);
	if (decoded_path)
		free(decoded_path);
	if (whole_path)
		free(whole_path);
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
    SSL_CTX* ctx = create_ssl();
    if(!ctx) {
        printf("can not create a ssl\n");
        goto err;
    }

    evhttp_set_bevcb(http, sslcb, ctx);

    evhttp_set_gencb(http, request_cb, &opt);

    handle = evhttp_bind_socket_with_handle(http, "0.0.0.0", opt.port);
    if (!handle) {
        // fprintf(stderr,"couldn't bind to port %d. Exiting.\n", o.port);
        printf("couldn't bind to port %d. Exiting.\n", opt.port);
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
    if(ctx)
        SSL_CTX_free(ctx);
    return ret;
}


