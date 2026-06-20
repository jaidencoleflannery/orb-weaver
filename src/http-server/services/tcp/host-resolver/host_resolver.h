#ifndef HOST_RESOLVER_H
#define HOST_RESOLVER_H

bool get_local_addresses(bool is_https, addrinfo **address_list);

bool print_addresses(ipv4_node *ipv4_list, ipv6_node *ipv6_list);

#endif
