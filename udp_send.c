#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#define PORT 7001

uv_loop_t *loop;
uv_udp_t send_socket;


void on_send(uv_udp_send_t *req, int status) {
  if (status) {
    fprintf(stderr, "Send error %s\n", uv_strerror(status));
  } else {
    printf("Sent UDP packet\n");
  }
}


int main() {
  loop = uv_default_loop();
  uv_udp_init(loop, &send_socket);
  struct sockaddr_in addr;
  uv_ip4_addr("127.0.0.1", PORT, &addr);
  uv_udp_bind(&send_socket, (const struct sockaddr*) &addr, 0);

  uv_udp_send_t send_req;
  char *msg = "udp message\n";
  uv_buf_t buf = uv_buf_init(msg, strlen(msg) + 1);
  uv_udp_send(&send_req, &send_socket, &buf, 1, (const struct sockaddr *) &addr, on_send);

  return uv_run(loop, UV_RUN_DEFAULT);
}
