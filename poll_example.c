#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <uv.h>

uv_loop_t *loop;
uv_poll_t poll_handle;

void poll_callback(uv_poll_t *handle, int status, int events) {
  if (status < 0) {
    printf("poll error\n");
  }
  if (events == UV_READABLE) {
    printf("/tmp/myfifo is readable\n");
  }
  uv_poll_stop(&poll_handle);
}

int main() {
  loop = uv_default_loop();

  mkfifo("/tmp/myfifo", S_IRWXU);
  int fd_read = open("/tmp/myfifo", O_RDONLY);

  uv_poll_init(loop, &poll_handle, fd_read);
  uv_poll_start(&poll_handle, UV_READABLE, poll_callback);
  return uv_run(loop, UV_RUN_DEFAULT);
}
