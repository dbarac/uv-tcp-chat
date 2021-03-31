#include <stdio.h>
#include <stdlib.h>
#include <uv.h>

#define PORT 7001

uv_loop_t *loop;
uv_udp_t recv_socket;

void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}

void on_read(uv_udp_t *req, ssize_t nread, const uv_buf_t *buf, const struct sockaddr *addr, unsigned flags) {
  if (nread < 0) {
    fprintf(stderr, "Read error %s\n", uv_err_name(nread));
    uv_close((uv_handle_t*) req, NULL);
    free(buf->base);
    return;
  }
  char sender[17] = {0};
  uv_ip4_name((const struct sockaddr_in*) addr, sender, 16);
  printf("Sender: %s\n", sender);
  printf("Message: %s\n", buf->base);
  free(buf->base);
  uv_udp_recv_stop(req);
}

int main() {
  loop = uv_default_loop();
  uv_udp_init(loop, &recv_socket);
  struct sockaddr_in recv_addr;
  uv_ip4_addr("0.0.0.0", PORT, &recv_addr);
  uv_udp_bind(&recv_socket, (const struct sockaddr*) &recv_addr, 0);
  uv_udp_recv_start(&recv_socket, alloc_buffer, on_read);
  printf("Receiving on %d\n", PORT);

  return uv_run(loop, UV_RUN_DEFAULT);
}
