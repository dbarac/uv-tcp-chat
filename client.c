#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#define DEFAULT_SERVER_PORT 7000
#define DEFAULT_SERVER_IP "127.0.0.1"

uv_loop_t *loop;
struct sockaddr_in addr;

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

void echo_write(uv_write_t *req, int status) {
  if (status) {
    fprintf(stderr, "Write error %s\n", uv_strerror(status));
  }
  free_write_req(req);
}

void echo_read(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
  if (nread > 0) {
    //write_req_t *req = (write_req_t*) malloc(sizeof(write_req_t));
    //req->buf = uv_buf_init(buf->base, nread);
    printf("%s\n", buf->base);
    //uv_write((uv_write_t*) req, client, &req->buf, 1, echo_write);
    return;
  } else if (nread < 0) {
    if (nread != UV_EOF) {
      fprintf(stderr, "Read error %s\n", uv_err_name(nread));
    }
    uv_close((uv_handle_t*) client, NULL);
  }
  free(buf->base);
}




void on_connect(uv_connect_t* req, int status) {
  if (status < 0) {
    fprintf(stderr, "New connection error %s\n", uv_strerror(status));
    return;
  } else {
    printf("Connected to server\n");
  }
  write_req_t *w_req = (write_req_t*) malloc(sizeof(write_req_t));
  char *message = (char*) malloc(10);
  strcpy(message, "Hello");
  w_req->buf = uv_buf_init(message, strlen(message));
  uv_write((uv_write_t*) w_req, req->handle, &w_req->buf, 1, echo_write);
  uv_read_start((uv_stream_t*)req->handle, alloc_buffer, echo_read);
}

int main() {
  loop = uv_default_loop();
  uv_tcp_t *socket = (uv_tcp_t*) malloc(sizeof(uv_tcp_t));
  uv_tcp_init(loop, socket);
  uv_connect_t* connect = (uv_connect_t*) malloc(sizeof(uv_connect_t));
  uv_ip4_addr(DEFAULT_SERVER_IP, DEFAULT_SERVER_PORT, &addr);
  
  uv_tcp_connect(connect, socket, (const struct sockaddr*)&addr, on_connect);
  int r = 0;
  if (r) {
    fprintf(stderr, "Listen error %s\n", uv_strerror(r));
    return 1;
  }
  return uv_run(loop, UV_RUN_DEFAULT);
}
