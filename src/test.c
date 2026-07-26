#include <pthread.h>
#include <stdio.h>
#include <string.h>
#define DEBUG_PORT 12345
#define DEBUG_ADDR "127.0.0.1"

#define LILSOCKETS_IMPL
#include "../include/sockets.h"
#include <stdbool.h>

struct client_connection {
  addr_t addr;
  bool connected;
};

#define MAX_CLIENTS_AMOUNT 32

struct server {
  struct client_connection clients[MAX_CLIENTS_AMOUNT];
  pthread_mutex_t mutex;
};

static struct server server = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
};

struct thread_args {
  addr_t server_addr;
};

static void *server_run(void *args) {
  struct thread_args server_args = *(struct thread_args *)args;

  for (;;) {
    puts("Enter a message to send to the client: ");

    char buf[64];
    scanf("%s", buf);

    if (strcmp(buf, ":q") == 0) {
      break;
    }

    // Lock server mutex to get access to all connected clients
    pthread_mutex_lock(&server.mutex);

    for (size_t i = 0; i < MAX_CLIENTS_AMOUNT; i++) {
      if (server.clients[i].connected) {
        // Send input message to connected clients
        sockets_send(server.clients[i].addr, (SocketDataBuffer){.buffer_size = sizeof(buf), .buffer = buf}, 0);
      }
    }

    pthread_mutex_unlock(&server.mutex);

  }

  return NULL;
}

static void *server_client_acceptor_run(void *args) {
  struct thread_args server_args = *(struct thread_args *)args;

  for (;;) {
    printf("Waiting for clients to join the server\n");
    int64_t client_addr = sockets_server_accept_client(server_args.server_addr);
    if (client_addr == -1) {
      printf("Invalid client trying to connect!\n");
      continue;
    }

    // Store the new client in the connected clients array, so we can send data
    pthread_mutex_lock(&server.mutex);

    bool found_client_slot = false;

    for (size_t i = 0; i < MAX_CLIENTS_AMOUNT; i++) {
      // Find empty client slot in the client array
      if (!server.clients[i].connected) {
        server.clients[i].addr = client_addr;
        server.clients[i].connected = true;
        found_client_slot = true;

        printf("successfully connected client\n");

        break;
      }
    }

    if (!found_client_slot) {
      printf("Cannot connect client, server is full!\n");
      sockets_close(client_addr);
    }

    pthread_mutex_unlock(&server.mutex);
  }
  return NULL;
}

static void *server_data_listener_run(void *args) { return NULL; }

static void server_start(void) {
  // Open the server
  addr_t server_addr = sockets_open_server(DEBUG_ADDR, DEBUG_PORT);
  if (server_addr == -1) {
    perror("Failed to open server");
    return;
  }

  printf("Server opened, socket: %d\n", server_addr);

  // Create a thread for server logic
  pthread_t server_thread;
  // Create a thread for listening to data sent to server from client
  pthread_t net_data_listener_thread;
  // Create a thread for handling clients trying to connect to the server
  pthread_t net_client_acceptor_thread;

  struct thread_args server_args = {
      .server_addr = server_addr,
  };

  if (pthread_create(&server_thread, NULL, server_run, &server_args)) {
    perror("Failed to create server thread\n");
    exit(1);
  }

  if (pthread_create(&net_data_listener_thread, NULL, server_data_listener_run,
                     &server_args)) {
    perror("Failed to create server data listener thread\n");
    exit(1);
  }

  if (pthread_create(&net_client_acceptor_thread, NULL,
                     server_client_acceptor_run, &server_args)) {
    perror("Failed to create server client polling thread\n");
    exit(1);
  }

  pthread_join(server_thread, NULL);

  pthread_cancel(net_data_listener_thread);
  pthread_cancel(net_client_acceptor_thread);

  pthread_join(net_data_listener_thread, NULL);
  pthread_join(net_client_acceptor_thread, NULL);

  printf("Server has finished");

  // Stop server
  sockets_close(server_addr);
}

static void *client_run(void *args) {
  for (;;) {

  }

  return NULL;
}

static void *client_data_listener_run(void *args) {
  struct thread_args client_args = *(struct thread_args *) args;

  char buf[64];

  for (;;) {
    // Wait for the server to send data
    // TODO: MSG_WAITALL is not in header
    ssize_t n = sockets_receive(client_args.server_addr, (SocketDataBuffer){.buffer = buf, .buffer_size = sizeof(buf)}, MSG_WAITALL);
    if (n != sizeof(buf)) {
      perror("Failed to read data");
      return NULL;
    }

    printf("Received data: %s\n", buf);
  }

  return NULL;
}

static void client_start(void) {
  addr_t server_addr = sockets_connect_to_server(DEBUG_ADDR, DEBUG_PORT);
  if (server_addr == -1) {
    perror("Failed to connect to server");
    return;
  }

  printf("Connected to server at addr: %d\n", server_addr);

  pthread_t client_thread;
  pthread_t client_data_listener_thread;

  struct thread_args args = {.server_addr = server_addr};
  if (pthread_create(&client_thread, NULL, client_run, &args)) {
    perror("Failed to create client logic thread");
    exit(1);
  }

  if (pthread_create(&client_data_listener_thread, NULL, client_data_listener_run, &args)) {
    perror("Failed to create client data listener thread");
    exit(1);
  }

  pthread_join(client_thread, NULL);
  pthread_join(client_data_listener_thread, NULL);

  sockets_close(server_addr);

  printf("Client stopped\n");
}

int main(void) {
#ifdef SERVER
  server_start();
#else
  client_start();
#endif
}
