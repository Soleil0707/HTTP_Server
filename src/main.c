#include "http.h"

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
            
            // while((*s++ = *t++) != '\0')

            strcat(first_boundary, "\r\n");
            strcat(last_boundary, "--\r\n");

            // printf("first_boundary is:%s, sizeof=%d, strlen=%d\n", first_boundary, sizeof(first_boundary), strlen(first_boundary));
        } else if (!evutil_ascii_strcasecmp(header->key, "Content-Length")) {
            content_len = atoi(header->value);
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

void request_cb(struct evhttp_request* req, void*arg) 
{
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
        printf("couldn't create evhttp. Exiting.\n");
        ret = 1;
    }

    SSL_CTX* ctx = create_ssl();
    if(!ctx) {
        printf("can not create a ssl\n");
        goto err;
    }

    //evhttp_set_bevcb(http, sslcb, ctx);

    evhttp_set_gencb(http, request_cb, &opt);

    handle = evhttp_bind_socket_with_handle(http, "0.0.0.0", opt.port);
    if (!handle) {
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


