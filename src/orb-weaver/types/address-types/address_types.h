#ifndef ADDRESS_TYPES_H
#define ADDRESS_TYPES_H

#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define IPV4_LOCALHOST "127.0.0.1"
#define IPV6_LOCALHOST "::1"
#define INET_PORTSTRLEN 6

typedef struct addrinfo addrinfo;
typedef struct sockaddr sockaddr;
typedef struct sockaddr_in sockaddr_in;
typedef struct sockaddr_in6 sockaddr_in6;
typedef struct sockaddr_storage sockaddr_storage;

typedef struct ipv4_node {
    sockaddr_in sockaddr_in;
    struct ipv4_node *next;
} ipv4_node;

typedef struct ipv6_node{
    sockaddr_in6 sockaddr_in6;
    struct ipv6_node *next;
} ipv6_node;

typedef struct {
    char *name;
    size_t name_length;
    char *port;
} hostname;

#endif
