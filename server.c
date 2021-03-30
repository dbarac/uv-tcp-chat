#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#define MAX_CLIENTS 32
#define DEFAULT_PORT 7000
#define DEFAULT_BACKLOG 128

uv_loop_t *loop;
struct sockaddr_in addr;
uv_stream_t *clients[MAX_CLIENTS];
int num_clients;

typedef struct {
  uv_write_t req;
  uv_buf_t buf;
} write_req_t;


void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
  buf->base = (char*) malloc(suggested_size);
  buf->len = suggested_size;
}


void free_write_req(uv_write_t *req) {
  write_req_t *wr = (write_req_t*) req;
  free(wr->buf.base);
  free(wr);
}


void free_client(uv_handle_t *client) {
  free(client);  
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if ((uv_stream_t*)client == clients[i]) {
      clients[i] = NULL;
      break;
    }
  }
}


void after_write(uv_write_t *req, int status) {
  if (status) {
    fprintf(stderr, "Write error %s\n", uv_strerror(status));
  }
  free_write_req(req);
}

/*
 * Send received message to all connected clients.
 * In case of error, close sender connection and remove from list of clients.
 */
void send_to_all(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
  if (nread > 0) {
    for (int i = 0; i < num_clients; i++) {
      if (clients[i] == NULL) {
        continue;
      }
      write_req_t *req = (write_req_t*) malloc(sizeof(write_req_t));
      char *message = (char*) malloc(nread);
      memcpy(message, buf->base, nread);
      req->buf = uv_buf_init(message, nread);
      uv_write((uv_write_t*) req, clients[i], &req->buf, 1, after_write);
    }
    //return;
  } else if (nread < 0) {
    num_clients--;
    if (nread != UV_EOF) {
      fprintf(stderr, "Read error%s\n", uv_err_name(nread));
    }
    printf("Closing client connection.\n");
    uv_close((uv_handle_t*) client, free_client);
  }
  free(buf->base);
}

/*
 * Accept new client connection, add to list of all clients.
 * Start reading incoming messages.
 */
void on_new_connection(uv_stream_t *server, int status) {
  if (status < 0) {
    fprintf(stderr, "New connection error %\n", uv_strerror(status));
    return;
  } 
  if (num_clients == MAX_CLIENTS) {
    printf("Max number of clients already connected.\n");
  }
  uv_tcp_t *client = (uv_tcp_t*) malloc(sizeof(uv_tcp_t));
  uv_tcp_init(loop, client);

  if (uv_accept(server, (uv_stream_t*) client) == 0 && num_clients < MAX_CLIENTS) {
    int i = 0;
    while (clients[i] != NULL) i++;
    clients[i] = (uv_stream_t*)client;
    num_clients++;
    uv_read_start((uv_stream_t*) client, alloc_buffer, send_to_all);
    printf("New client connected\n");
  } else {
    uv_close((uv_handle_t*) client, free_client);
  }
}

/*
 * Multi-user TCP chat server
 */
int main() {
  loop = uv_default_loop();
  uv_tcp_t server;
  uv_tcp_init(loop, &server);

  uv_ip4_addr("0.0.0.0", DEFAULT_PORT, &addr);

  uv_tcp_bind(&server, (const struct sockaddr*)&addr, 0);
  int r = uv_listen((uv_stream_t*) &server, DEFAULT_BACKLOG, on_new_connection);
  if (r) {
    fprintf(stderr, "Listen error %s\n", uv_strerror(r));
    return 1;
  } else {
    printf("Listening on port %d\n", DEFAULT_PORT);
  }
  return uv_run(loop, UV_RUN_DEFAULT);
}
