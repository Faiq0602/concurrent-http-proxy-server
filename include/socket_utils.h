#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include <stddef.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET socket_handle_t;
#define SOCKET_HANDLE_INVALID INVALID_SOCKET
#else
typedef int socket_handle_t;
#define SOCKET_HANDLE_INVALID (-1)
#endif

int socket_utils_initialize(void);
void socket_utils_cleanup(void);
int socket_utils_get_last_error(void);
socket_handle_t socket_utils_create_listener(int port, int backlog);
socket_handle_t socket_utils_accept(socket_handle_t listener_socket,
    char *client_host, size_t client_host_size,
    char *client_service, size_t client_service_size);
int socket_utils_read(socket_handle_t socket_handle, char *buffer, size_t buffer_size);
void socket_utils_close(socket_handle_t socket_handle);

#endif
