#ifndef _CLIENT_STUB_PRIVATE_H
#define _CLIENT_STUB_PRIVATE_H

#include "inet.h"

#include "client_stub.h"

struct rtree_t{
	struct sockaddr_in server;
	int socket;
}rtree_t;

#endif