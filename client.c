#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <uv.h>

#define DEFAULT_SERVER_PORT 7000
#define DEFAULT_SERVER_IP "127.0.0.1"
#define MAX_MSG_LEN 78
#define INPUT_AREA_HEIGHT 5
#define KEY_BACKSPACE 127
#define PROMPT_LEN 14

uv_loop_t *loop;
struct sockaddr_in addr;
uv_tty_t tty;
uv_pipe_t in;
uv_timer_t tick;
uv_write_t write_req;
uv_write_t write_req2;
int width;
int height;
int msgs = 1;

uv_buf_t user_msg;
typedef struct {
  uv_write_t req;
  uv_buf_t buf;
} write_req_t;


void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
  buf->base = (char*) malloc(suggested_size);
  buf->len = suggested_size;
}

/*void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
  *buf = uv_buf_init((char*) malloc(suggested_size), suggested_size);
}*/

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
    //printf("Connected to server: %s\n", DEFAULT_SERVER_IP);
  }
  write_req_t *w_req = (write_req_t*) malloc(sizeof(write_req_t));
  char *message = (char*) req->handle->data;//(char*) malloc(10);
  w_req->buf = uv_buf_init(message, strlen(message));
  uv_write((uv_write_t*) w_req, req->handle, &w_req->buf, 1, echo_write);
  uv_read_start((uv_stream_t*)req->handle, alloc_buffer, echo_read);
}


void disable_line_buffering() {
  struct termios term;
	tcgetattr(STDIN_FILENO, &term);
	term.c_lflag &= ~ICANON;
  term.c_lflag &= ~ECHO;
	tcsetattr(STDIN_FILENO, TCSANOW, &term);
	setbuf(stdin, NULL);
}


/*
 * Find window size, disable line buffering, setup interface and initial cursor position.
*/
void setup_tty() {
  if (uv_tty_get_winsize(&tty, &width, &height)) {
    fprintf(stderr, "Could not get TTY information\n");
    uv_tty_reset_mode();
    return;// 1;
  }
  disable_line_buffering();

  char input_prompt[128];
  uv_buf_t buf;
  buf.base = input_prompt;
  int prompt_pos = height - INPUT_AREA_HEIGHT;
  buf.len = sprintf(buf.base, "\033[2J\033[H\033[%dB------------------\nYour message: ", prompt_pos);
  uv_write(&write_req, (uv_stream_t*) &tty, &buf, 1, NULL);

}


/*
 * Update user_msg and tty depending on input char:
 *   - if char is printable, print
 *   - if user pressed backspace, delete char from message input
*/
void update_user_message(char input) {
  char data[128];
  uv_buf_t buf;
  buf.base = data;
  int pos_y = height - INPUT_AREA_HEIGHT + 1;
  int pos_x = PROMPT_LEN + user_msg.len;

  if (input == KEY_BACKSPACE && user_msg.len > 0) {
    user_msg.len--;
    pos_x--;
    buf.len = sprintf(buf.base, "\033[H\033[%dB\033[%dC \033[1D", pos_y, pos_x);
    uv_write(&write_req, (uv_stream_t*) &tty, &buf, 1, NULL);
  } else if (isprint(input) && user_msg.len < MAX_MSG_LEN) {
    user_msg.base[user_msg.len++] = input;
    buf.len = sprintf(buf.base, "\033[H\033[%dB\033[%dC%c", pos_y, pos_x, input);
    uv_write(&write_req, (uv_stream_t*) &tty, &buf, 1, NULL);
  }
}


void send_message() {
  int pos_y = height - INPUT_AREA_HEIGHT + 1;
  int pos_x = PROMPT_LEN + user_msg.len;
  char data[128];
  uv_buf_t buf;
  buf.base = data;

  if (user_msg.len > 0) {
    buf.len = sprintf(buf.base, "\033[H\033[%dB%s", msgs, user_msg.base);
    msgs++;
    uv_write(&write_req, (uv_stream_t*) &tty, &buf, 1, NULL);
  }
}


void clear_message_input() {
  char data[256];
  uv_buf_t buf;
  buf.base = data;
  int pos_y = height - INPUT_AREA_HEIGHT + 1;
  int pos_x = PROMPT_LEN;
  for (int i = 0; i < MAX_MSG_LEN; i++) {
    user_msg.base[i] = ' ';
  }
  user_msg.len = 0;
  buf.len = sprintf(buf.base, "\033[H\033[%dB\033[%dC\033[0J", pos_y, pos_x, user_msg.base);
  uv_write(&write_req2, (uv_stream_t*) &tty, &buf, 1, NULL);
}


void process_char(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
  if (nread < 0){
    if (nread == UV_EOF){
      // end of file
			int a = 3;
    }
  } else if (nread > 0) {
    assert(nread == 1); // line buffering is turned off
    char c = buf->base[0];
		if (c == '\n') {
      send_message();
      clear_message_input();
		} else if (isprint(c) || c == KEY_BACKSPACE) {
		  update_user_message(c);
    }
  }
  // OK to free buffer as write_data copies it.
  if (buf->base) {
    free(buf->base);
  }
}


int main() {
  char *username = malloc(20);
  printf("Enter username: ");
  scanf("%s", username);

  user_msg.base = (char*) malloc(MAX_MSG_LEN);
  user_msg.len = 0;

  loop = uv_default_loop();


  uv_pipe_init(loop, &in, 0);
  uv_pipe_open(&in, 0);
  uv_read_start((uv_stream_t*)&in, alloc_buffer, process_char);

  uv_tcp_t *socket = (uv_tcp_t*) malloc(sizeof(uv_tcp_t));
  socket->data  = (void*) username;
  uv_tcp_init(loop, socket);
  uv_connect_t* connect = (uv_connect_t*) malloc(sizeof(uv_connect_t));
  uv_ip4_addr(DEFAULT_SERVER_IP, DEFAULT_SERVER_PORT, &addr);
  uv_tcp_connect(connect, socket, (const struct sockaddr*)&addr, on_connect);
  int r = 0;
  if (r) {
    fprintf(stderr, "Listen error %s\n", uv_strerror(r));
    return 1;
  }


  uv_tty_init(loop, &tty, STDOUT_FILENO, 0);
  uv_tty_set_mode(&tty, 0);
  setup_tty();

  return uv_run(loop, UV_RUN_DEFAULT);
}
