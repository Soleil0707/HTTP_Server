#ifndef __HTTP_H__
#define __HTTP_H__

struct options {
	int port;
	int iocp;
	int verbose;

	int unlink;
	const char *unixsock;
	const char *docroot;
};

#endif