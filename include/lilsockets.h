/*
 * For the implementation, please put '#define LILSOCKETS_IMPL' before including
 * this header. NOTE: This has to be done exactly one time.
 */

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#define _INVALID_SOCKET (int32_t)(~0)

typedef int32_t addr_t;

// Typedef for internal windows/linux specific pollfd struct, required for polling
typedef struct pollfd PollClient;

// Opens a server with the given ip address and port and
// returns its socket
addr_t sockets_open_server(const char *ip_address, uint32_t port); // SERVER-FUNCTION

// Connects to a server at the given ip address and port and
// returns its socket
addr_t sockets_connect_to_server(const char *ip_address, uint32_t port); // CLIENT-FUNCTION

// Sends a buffer of buffer_size to the specified socket address
// Returns the amount of bytes sent or -1 if sending fails
int64_t sockets_send(addr_t socket_addr, void *buf, uint64_t buf_size); // CLIENT/SERVER-FUNCTION

// Will block the current thread until data is received, stores data in the buffer
// Returns the amount of bytes or -1 if receiving fails
int64_t sockets_receive(addr_t socket_addr, void *buf, uint64_t buf_size); // CLIENT/SERVER-FUNCTION

// Accept a client trying to connect to the server at socket_addr
// Returns the address of the accepted client or -1 if accepting fails
addr_t sockets_server_accept_client(addr_t socket_addr); // SERVER-FUNCTION

// Poll the clients to see which one is trying to send data to the server
// updates the polling_clients, to see which ones if any are sending data
// Returns a polling result that can be validated using sockets_server_valid_poll
int64_t sockets_server_poll_clients(PollClient *polling_clients,
                                    uint64_t polling_clients_amount,
                                    int32_t timeout); // SERVER-FUNCTION

// Checks if the poll result is valid. If an error occured, on linux
// it set the error parameter to errno, while on windows it sets the
// error parameter to WSAGetLastError(). If you do not care about the
// error you can also pass NULL.
// Returns a boolean whether the poll was valid
bool sockets_server_valid_poll(int64_t result, int32_t *error); // SERVER-FUNCTION

// Closes a socket. This can be used to close the client connection to a server
// Or the server itself.
void sockets_close(addr_t socket_addr); // CLIENT/SERVER-FUNCTION

/* IMPLEMENTATION OF SOCKETS */

#ifdef LILSOCKETS_IMPL
#ifdef _WIN32
#define Rectangle winapiIsSoOldAndGrossSoMangleIt_Rectangle
#define CloseWindow winapiIsSoOldAndGrossSoMangleIt_CloseWindow
#define ShowCursor winapiIsSoOldAndGrossSoMangleIt_ShowCursor
#define LoadImage winapiIsSoOldAndGrossSoMangleIt_LoadImage
#define DrawText winapiIsSoOldAndGrossSoMangleIt_DrawText
#define DrawTextEx winapiIsSoOldAndGrossSoMangleIt_DrawTextEx
#define PlaySound winapiIsSoOldAndGrossSoMangleIt_PlaySound

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#undef Rectangle
#undef CloseWindow
#undef ShowCursor
#undef LoadImage
#undef DrawText
#undef DrawTextEx
#undef PlaySound
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#endif
#include <stdio.h>

addr_t sockets_open_server(const char *ip_address, uint32_t port) {
  int32_t opt = 1;
#ifdef _WIN32
  SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
#else
  int32_t sock = socket(AF_INET, SOCK_STREAM, 0);
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
  struct sockaddr_in server_socket = {.sin_family = AF_INET,
                                      .sin_port = htons(port)};
  if (inet_pton(AF_INET, ip_address, &server_socket.sin_addr) <= 0) {
    perror("inet_pton failed (invalid IP)");
    sockets_close(sock);
    return -1;
  }

#ifdef _WIN32
  if (sock == SOCKET_ERROR) {
    fprintf(stderr, "Socket failed: %d\n", WSAGetLastError());
    return -1;
  }
#else
  if (sock < 0) {
    perror("socket failed");
    return -1;
  }
#endif

  if (bind(sock, (struct sockaddr *)&server_socket, sizeof(server_socket)) <
      0) {
    perror("bind failed");
    sockets_close(sock);
    return -1;
  }

  if (listen(sock, 10) < 0) {
    perror("listen failed");
    sockets_close(sock);
    return -1;
  }

  return sock;
}

int32_t sockets_connect_to_server(const char *ip_address, uint32_t port) {
#ifdef _WIN32
  SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
#else
  int32_t sock = socket(AF_INET, SOCK_STREAM, 0);
#endif
  if (sock < 0) {
    perror("socket failed");
    return -1;
  }

  struct sockaddr_in server = {.sin_family = AF_INET, .sin_port = htons(port)};

  if (inet_pton(AF_INET, ip_address, &server.sin_addr) <= 0) {
    perror("invalid address");
    sockets_close(sock);
    return -1;
  }

  if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
    perror("connect failed");
    sockets_close(sock);
    return -1;
  }
  return sock;
}

addr_t sockets_server_accept_client(addr_t socket_addr) {
  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);
#ifdef _WIN32
  SOCKET client_fd =
      accept(socket_addr, (struct sockaddr *)&client_addr, &client_len);
#else
  int32_t client_fd =
      accept(socket_addr, (struct sockaddr *)&client_addr, &client_len);
#endif

  if (client_fd < 0) {
    perror("accept failed");
    sockets_close(socket_addr);
    return -1;
  }

  return client_fd;
}

int64_t sockets_send(int32_t socket_addr, void *buf, uint64_t buf_size) {
  return send(socket_addr, buf, buf_size, 0);
}

int64_t sockets_receive(int32_t socket_addr, void *buf, uint64_t buf_size) {
  return recv(socket_addr, buf, buf_size, 0);
}

int64_t sockets_server_poll_clients(PollClient *client_addresses,
                                    uint64_t client_addresses_amount,
                                    int32_t timeout) {
  if (client_addresses_amount > 0) {

#ifdef _WIN32
    int32_t poll_result =
        WSAPoll(client_addresses, client_addresses_amount, timeout);
#else
    int32_t poll_result =
        poll(client_addresses, client_addresses_amount, timeout);
#endif
    return poll_result;
  }
  return 0;
}

bool sockets_server_valid_poll(int64_t result, int32_t *error) {
#ifdef _WIN32
  if (result == SOCKET_ERROR) {
    fprintf(stderr, "WSAPoll failed: %d\n", WSAGetLastError());
    if (error != NULL) {
      *error = WSAGetLastError();
    }
    return false;
  }
#else
  if (result < 0) {
    if (error != NULL) {
      return errno;
    }
    return false;
  }
#endif
  return true;
}

void sockets_close(addr_t socket_addr) {
#ifdef _WIN32
  closesocket(socket_addr);
#else
  close(socket_addr);
#endif
}

#endif
