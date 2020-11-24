#include "http.h"

void print_usage(FILE* out, const char* prog, int exit_code) {
    // fprintf(out, "usage: %s [-p port_num] <docroot>\n", prog);
	fprintf(out,
		"Syntax: %s [ OPTS ] <docroot>\n"
		" -p      - port\n", prog);
    exit(exit_code);
}

void do_term(int sig, short events, void *arg){
	struct event_base *base = arg;
	event_base_loopbreak(base);
	// fprintf(stderr, "Got %i, Terminating\n", sig);
	printf("Received signal %i, Terminating ...\n", sig);
}

int display_listen_sock(struct evhttp_bound_socket *handle){
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

struct options parse_opts(int argc, char** argv) {
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
const char *guess_content_type(const char *path){
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
char *read_evbuffer_line(struct evbuffer *buf, enum evbuffer_eol_style style){
	char *p = NULL;
	p = evbuffer_readln(buf, NULL, style);
	// if((style == EVBUFFER_EOL_CRLF_STRICT) && (p != NULL)) {
	// 	strncat(p, "\r\n", 2);
	// }
	return p;
}


void write_post2file(struct evbuffer* buf, char* first_boundary, char* last_boundary, FILE* f) {
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
void file_revise(FILE* f, FILE* f_tmp, char* first_boundary, char* last_boundary, char* f_name) {
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
			//printf("this is last_boundary\n");
			break;
		} else if(strstr(c, first_boundary)) {
			has_begin_bound = 1;
			//("this is first_boundary\n");
			continue;
		} else if(strstr(c, "Content-Disposition") && (has_content_disposition == 0) && (has_begin_bound == 1)) {
			has_content_disposition = 1;
			//printf("this is Content-Disposition\n");
			continue;
		} else if(strstr(c, "Content-Type") && (has_content_type == 0) && (has_begin_bound == 1)) {
			has_content_type = 1;
			////printf("this is Content-Type\n");
			continue;
		} else if((strlen(c) == 2) && (has_blank_line == 0) && (has_begin_bound == 1)){
			has_blank_line = 1;
			//printf("this is blank line\n");
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
			printf("Success\n");
		}
	}
	printf("Write success!\n");

	free(c);
	c = NULL;
	free(last_c);
	last_c = NULL;
}

/**
 * 获取指定路径文件的大小
*/
int get_file_size(char* filename) {
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

unsigned int request_count = 0;
int handle_post_request(struct evhttp_request* req, char* whole_path) {
    struct evkeyvalq* headers = NULL;
	struct evkeyval *header = NULL;
    const char* bound_key = "boundary=";
    char first_boundary[256] = {0};
    char last_boundary[256] = {0};
    int content_len = 0;
    // int current_len = 0;
    // int data_left = -1;
    struct evbuffer *buf = NULL;

    headers = evhttp_request_get_input_headers(req);
    int is_file_data = 0;
    printf("---begin a request header print-----------------\n");
	for (header = headers->tqh_first; header; header = header->next.tqe_next) {
		printf("%s: %s\n", header->key, header->value);

        if (!evutil_ascii_strcasecmp(header->key, "Content-Type")) {
            if(strstr(header->value, "x-www-form-urlencoded")) {
                is_file_data = -1;
                continue;
            }
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
            strcat(first_boundary, "--");
            strncat(first_boundary, boundary_value, strlen(boundary_value)+1);
            strncat(last_boundary, first_boundary, strlen(first_boundary)+1);

            strcat(first_boundary, "\r\n");
            strcat(last_boundary, "--\r\n");

            // printf("first_boundary is:%s, sizeof=%d, strlen=%d\n", first_boundary, sizeof(first_boundary), strlen(first_boundary));
        } else if (!evutil_ascii_strcasecmp(header->key, "Content-Length")) {
            content_len = atoi(header->value);
			printf("content length is: %d", content_len);
        }

	}
    printf("---finish a request header print----------------\n");
    if(is_file_data < 0) {
        evhttp_send_reply(req, HTTP_OK, "OK", evhttp_request_get_input_buffer(req));

    } else if(is_file_data > 0) {
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

void handle_get_request(struct evhttp_request* req, const char* path, char* whole_path, char* decoded_path) {
    struct stat file_state;

    int fd;
    stat(whole_path,&file_state);    

    if(S_ISDIR(file_state.st_mode)){//if it is a directory
        printf("\n============================\nclient is getting the directory %s\n",whole_path);
        DIR *loc_dir;
        char temp_buffer[MAX_CHUNK_SIZE]; 
        struct dirent* temp_dir;
        memset(temp_buffer, 0, MAX_CHUNK_SIZE);

        if(!(loc_dir = opendir(whole_path))){
            printf("error\n============================\n");
            goto err;
        }
        
        evhttp_add_header(evhttp_request_get_output_headers(req),
		    "Content-Type", "text/html");
        evhttp_send_reply_start(req, HTTP_OK,
                            "Start to send directory by chunk.");

        sprintf(temp_buffer,"<!DOCTYPE html>\n"
                            "<html>\n"
                            "<head>\n"
                                "<meta charset=\"utf-8\">\n"
                                "<meta name=\"author\" content=\"Yang Xiaomao\">\n"
                                "<title>This is title!</title>\n"
                            "</head>\n"
                            "<body>\n"
                                "<h>file list</h>\n"
                                    "<ul>\n");
        send_data_by_chunk(req,temp_buffer,strlen(temp_buffer));

        while ((temp_dir = readdir(loc_dir))) {
            const char* name = temp_dir->d_name;
            if (evutil_ascii_strcasecmp(name, ".") &&
                evutil_ascii_strcasecmp(name, "..")) {
                sprintf(temp_buffer, "    <li>%s</li>\n", name);
                send_data_by_chunk(req, temp_buffer, strlen(temp_buffer));
            }
		}
        sprintf(temp_buffer,
			    "</ul>\n"
                "</body>\n"
                "</html>\n");
        send_data_by_chunk(req, temp_buffer, strlen(temp_buffer));
        closedir(loc_dir);
    }else{//if it is a file
        //analyse the type of file(jpg gif or etc.)
        printf("============================\nclient is getting the file %s\n",whole_path);
        const char *file_type = guess_content_type(decoded_path);

        if((fd = open(whole_path,O_RDONLY)) < 0){
            printf("error\n============================\n\n");
            goto err;
        }

        struct stat del;
        
        fstat(fd,&del);
        //file size to send
        size_t file_size = del.st_size;
        //file_send_offset
        off_t offset = 0;
        //the rest of the file to send  
        size_t rest_file_len = file_size;
        //chunk_size to send 
        size_t send_size;   

        evhttp_add_header(evhttp_request_get_output_headers(req),
		    "Content-Type", file_type);
        evhttp_send_reply_start(req, HTTP_OK, "Start to send file by chunk.");

        while(offset < file_size){
            rest_file_len = file_size - offset;
            send_size = (rest_file_len > MAX_CHUNK_SIZE) ? MAX_CHUNK_SIZE : rest_file_len;
            
            struct evbuffer* file_buffer = evbuffer_new();
            struct evbuffer_file_segment* ev_seg = 
                evbuffer_file_segment_new(fd,offset,send_size,0);
            evbuffer_add_file_segment(file_buffer,
                                      ev_seg,0,send_size);
            evhttp_send_reply_chunk(req,file_buffer);
            printf("Send chunk from %ld to %ld bytes of the files to the client, size is %lu\n",offset,offset + send_size,send_size);
            evbuffer_file_segment_free(ev_seg);
            evbuffer_free(file_buffer);

            offset += send_size;
        }
        close(fd);
    }
    printf("Send finish\n============================\n\n");
    evhttp_send_reply_end(req);
    return;
err:
    evhttp_send_error(req, 404, "Document was not found,or \
please don't let your browser try to get my favicon.ico!");
    return;
}

void request_cb(struct evhttp_request* req, void*arg) {
    // struct options *o = (struct options*)arg;
    struct options *o = arg;
    const char *cmdtype;
    const char *uri;
    const char *path; 
    struct evhttp_uri *decoded = NULL;
    char *decoded_path;
    size_t len;
    char *whole_path = NULL;

    request_count++;
    printf("Total request number is %d\n",request_count);

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
    uri = evhttp_request_get_uri(req);
    printf("Received a %s request for %s\nHeaders:\n",
        cmdtype, uri);

    if (!evutil_ascii_strcasecmp(cmdtype, "unknown")) {
        printf("[HTTP STATUS] 405 method not allowed\n");
        evhttp_send_error(req, HTTP_BADMETHOD, 0);
        return;
    }

    /* Decode the URI */
    decoded = evhttp_uri_parse(uri);
	if (!decoded) {
		printf("It's not a good URI. Sending BADREQUEST\n");
		evhttp_send_error(req, HTTP_BADREQUEST, 0);
		return;
	}

    path = evhttp_uri_get_path(decoded);
    printf("path is %s\n",path);
    if (!path) path = "/";

	decoded_path = evhttp_uridecode(path, 0, NULL);
	if (decoded_path == NULL)
		goto err;
    printf("decoded path: %s\n", decoded_path);

	if (strstr(decoded_path, ".."))
		goto err; 

    len = strlen(decoded_path)+strlen(o->docroot)+2;
	if (!(whole_path = malloc(len))) {
		perror("malloc");
		goto err;
	}

    int offset = (decoded_path[0] == '/') ? 1 : 0;
    evutil_snprintf(whole_path, len - offset, "%s/%s", o->docroot,
                    decoded_path + offset);
    printf("the whole path of current request is: %s\n", whole_path);

    // if (evhttp_request_get_command(req) == EVHTTP_REQ_POST) {
    // POST request
    if (!evutil_ascii_strcasecmp(cmdtype, "POST")) {
        handle_post_request(req, whole_path);
    // } else if (evhttp_request_get_command(req) == EVHTTP_REQ_GET) {
    // GET request
    } else if (!evutil_ascii_strcasecmp(cmdtype, "GET")) {
        handle_get_request(req, path, whole_path, decoded_path);
    } else {

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
