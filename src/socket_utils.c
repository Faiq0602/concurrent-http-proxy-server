#include "socket_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#else
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

static void populate_client_identity(const struct sockaddr_storage *client_addr,
    char *client_host, size_t client_host_size,
    char *client_service, size_t client_service_size)
{
    unsigned short port = 0;

    if (client_host != NULL && client_host_size > 0) {
        client_host[0] = '\0';
    }
    if (client_service != NULL && client_service_size > 0) {
        client_service[0] = '\0';
    }

    if (client_addr->ss_family == AF_INET) {
        const struct sockaddr_in *ipv4_addr = (const struct sockaddr_in *)client_addr;
        const char *address_text;

        port = ntohs(ipv4_addr->sin_port);
        if (client_host != NULL && client_host_size > 0) {
            address_text = inet_ntoa(ipv4_addr->sin_addr);
            if (address_text == NULL) {
                (void)snprintf(client_host, client_host_size, "unknown");
            } else {
                (void)snprintf(client_host, client_host_size, "%s", address_text);
            }
        }
    } else if (client_host != NULL && client_host_size > 0) {
            (void)snprintf(client_host, client_host_size, "unknown");
    }

    if (client_service != NULL && client_service_size > 0) {
        (void)snprintf(client_service, client_service_size, "%u", (unsigned int)port);
    }
}

int socket_utils_initialize(void)
{
#if defined(_WIN32)
    WSADATA wsa_data;

    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        return -1;
    }
#endif

    return 0;
}

void socket_utils_cleanup(void)
{
#if defined(_WIN32)
    (void)WSACleanup();
#endif
}

int socket_utils_get_last_error(void)
{
#if defined(_WIN32)
    return (int)WSAGetLastError();
#else
    return errno;
#endif
}

socket_handle_t socket_utils_create_listener(int port, int backlog)
{
    socket_handle_t listener_socket;
    struct sockaddr_in server_addr;
    int reuse_addr = 1;

    listener_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listener_socket == SOCKET_HANDLE_INVALID) {
        return SOCKET_HANDLE_INVALID;
    }

    if (setsockopt(listener_socket, SOL_SOCKET, SO_REUSEADDR,
            (const char *)&reuse_addr, (int)sizeof(reuse_addr)) != 0) {
        socket_utils_close(listener_socket);
        return SOCKET_HANDLE_INVALID;
    }

    (void)memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons((unsigned short)port);

    if (bind(listener_socket, (const struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        socket_utils_close(listener_socket);
        return SOCKET_HANDLE_INVALID;
    }

    if (listen(listener_socket, backlog) != 0) {
        socket_utils_close(listener_socket);
        return SOCKET_HANDLE_INVALID;
    }

    return listener_socket;
}

socket_handle_t socket_utils_connect_to_host(const char *host, int port)
{
    socket_handle_t socket_handle = SOCKET_HANDLE_INVALID;
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *cursor;
    char port_buffer[16];

    if (host == NULL || port < 1 || port > 65535) {
        return SOCKET_HANDLE_INVALID;
    }

    (void)memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    (void)snprintf(port_buffer, sizeof(port_buffer), "%d", port);
    if (getaddrinfo(host, port_buffer, &hints, &result) != 0) {
        return SOCKET_HANDLE_INVALID;
    }

    for (cursor = result; cursor != NULL; cursor = cursor->ai_next) {
        socket_handle = socket(cursor->ai_family, cursor->ai_socktype, cursor->ai_protocol);
        if (socket_handle == SOCKET_HANDLE_INVALID) {
            continue;
        }

        if (connect(socket_handle, cursor->ai_addr, (int)cursor->ai_addrlen) == 0) {
            break;
        }

        socket_utils_close(socket_handle);
        socket_handle = SOCKET_HANDLE_INVALID;
    }

    freeaddrinfo(result);
    return socket_handle;
}

socket_handle_t socket_utils_accept(socket_handle_t listener_socket,
    char *client_host, size_t client_host_size,
    char *client_service, size_t client_service_size)
{
    socket_handle_t client_socket;
    struct sockaddr_storage client_addr;
#if defined(_WIN32)
    int client_addr_len = (int)sizeof(client_addr);
#else
    socklen_t client_addr_len = sizeof(client_addr);
#endif

    client_socket = accept(listener_socket, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_socket == SOCKET_HANDLE_INVALID) {
        return SOCKET_HANDLE_INVALID;
    }

    populate_client_identity(&client_addr,
        client_host, client_host_size,
        client_service, client_service_size);

    return client_socket;
}

int socket_utils_read(socket_handle_t socket_handle, char *buffer, size_t buffer_size)
{
    if (buffer == NULL || buffer_size == 0) {
        return -1;
    }

    return recv(socket_handle, buffer, (int)buffer_size, 0);
}

int socket_utils_write_all(socket_handle_t socket_handle, const char *buffer, size_t buffer_size)
{
    size_t total_sent = 0;
    int bytes_sent;

    if (buffer == NULL) {
        return -1;
    }

    while (total_sent < buffer_size) {
        bytes_sent = send(socket_handle, buffer + total_sent, (int)(buffer_size - total_sent), 0);
        if (bytes_sent <= 0) {
            return -1;
        }

        total_sent += (size_t)bytes_sent;
    }

    return 0;
}

void socket_utils_close(socket_handle_t socket_handle)
{
    if (socket_handle == SOCKET_HANDLE_INVALID) {
        return;
    }

#if defined(_WIN32)
    (void)closesocket(socket_handle);
#else
    (void)close(socket_handle);
#endif
}
