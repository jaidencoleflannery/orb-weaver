#include <stdbool.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>

#include "types/address-types/address_types.h"
#include "services/logging/logging.h"
#include "configuration/configuration-handler/configuration_handler.h"
#include "utilities/string-tools/string_tools.h"

#include "./host_resolver.h"

bool get_local_addresses(bool is_https, addrinfo **address_list) { 
    DEBUG_LOG("get_local_addresses: Searching for local addresses.");

    addrinfo address_request = { 0 };
    address_request = (addrinfo){
        .ai_family = AF_UNSPEC, // any ip version.
        .ai_socktype = SOCK_STREAM, // tcp.
        .ai_flags = AI_PASSIVE // current machines ip.
    };

    char *port_string = malloc(sizeof(size_t));
    if(!size_t_to_string(config.port, port_string)) {
        ERROR_LOG("get_local_addresses: Error converting port into a string.");
        return false;
    } 

    int status;
    // cannot use validate_syscall here due to differing errno type.
    if((status = getaddrinfo(NULL, port_string, &address_request, address_list)) != 0) {
        ERROR_LOG("get_local_addresses: Error resolving addresses. Error: %s", gai_strerror(status));
        return false;
    }

    if(address_list == NULL || *address_list == NULL) {
        ERROR_LOG("get_local_addresses: Address returned was invalid. Error: %s", gai_strerror(status));
        return false;
    }

    DEBUG_LOG("get_local_addresses: Retrieved addresses from current machine.");
    #ifndef NDEBUG
        addrinfo *cursor = *address_list;
        while(cursor != NULL) {
            char address_string[INET6_ADDRSTRLEN];
            void *binary_address;
            if(cursor->ai_family == AF_INET) {
                binary_address = &((sockaddr_in *)cursor->ai_addr)->sin_addr;
            } else {
                binary_address = &((sockaddr_in6 *)cursor->ai_addr)->sin6_addr;
            }
            inet_ntop(cursor->ai_family, binary_address, address_string, sizeof(address_string));
            DEBUG_LOG("get_local_addresses: Found Address %s.", address_string);
            cursor = cursor->ai_next;
        }
    #endif 

    return true;
}

bool print_local_addresses(ipv4_node *ipv4_list, ipv6_node *ipv6_list) {
    DEBUG_LOG("print_addresses: Printing provided addresses.");

    ipv4_node *ipv4_cursor = ipv4_list->next;
    ipv4_node *ipv4_cleanup_cursor = ipv4_cursor;
    while(ipv4_cursor != NULL) {
        char ipv4_address[INET_ADDRSTRLEN];
        void *binary_address = &(ipv4_cursor->sockaddr_in.sin_addr);
        if(inet_ntop(AF_INET, binary_address, ipv4_address, sizeof(ipv4_address)) == NULL) {
            DEBUG_LOG("print_addresses: Failed to convert an IPv4 binary address into a string.");
            continue;
        }

        uint16_t ipv4_port = ntohs(ipv4_cursor->sockaddr_in.sin_port);

        LOG("[ IPv4 ]", "%s:%u.", ipv4_address, ipv4_port);
        ipv4_cursor = ipv4_cursor->next;

        // free memory on the tail.
        free(ipv4_cleanup_cursor);
        ipv4_cleanup_cursor = ipv4_cursor;
    }

    // free lingering tail.
    free(ipv4_cleanup_cursor);

    ipv6_node *ipv6_cursor = ipv6_list->next;
    ipv6_node *ipv6_cleanup_cursor = ipv6_cursor;
    while(ipv6_cursor != NULL) {
        char ipv6_address[INET6_ADDRSTRLEN];
        void *binary_address = &(ipv6_cursor->sockaddr_in6.sin6_addr);
        if(inet_ntop(AF_INET6, binary_address, ipv6_address, sizeof(ipv6_address)) == NULL) {
            DEBUG_LOG("print_addresses: Failed to convert an IPv6 binary address into a string.");
            continue;
        }

        uint16_t ipv6_port = ntohs(ipv6_cursor->sockaddr_in6.sin6_port);

        LOG("[ IPv6 ]", "%s:%u.", ipv6_address, ipv6_port);
        ipv6_cursor = ipv6_cursor->next;
        
        // free memory on the tail.
        free(ipv6_cleanup_cursor);
        ipv6_cleanup_cursor = ipv6_cursor;
    }

    // free lingering tail.
    free(ipv4_cleanup_cursor);

    LOG("[ LOG ]", "List exhausted.");
    return true;
}

