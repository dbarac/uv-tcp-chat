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
#define MSG_BUF_SIZE 256
#define INPUT_AREA_HEIGHT 5
#define KEY_BACKSPACE 127
#define PROMPT_LEN 14
#define USERNAME_LEN 10

typedef struct {
  uv_write_t req;
  uv_buf_t buf;
} write_req_t;

uv_loop_t *loop;
uv_tty_t tty; /* STDOUT */
uv_pipe_t in; /* STDIN */
uv_tcp_t *socket_;

int width;
int height;
int msgs_on_screen = 0;
uv_buf_t user_msg;
char username[USERNAME_LEN];

void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
  buf->base = (char*) malloc(suggested_size);
  buf->len = suggested_size;
}


void free_write_req(uv_write_t *req) {
  write_req_t *wr = (write_req_t*) req;
  free(wr->buf.base);
  free(wr);
}


void after_write(uv_write_t *req, int status) {
  if (status) {
    fprintf(stderr, "Write error %s\n", uv_strerror(status));
  }
  free_write_req(req);
}


void get_username() {
  printf("Enter username: ");
  scanf("%8s", username);
  int i = strlen(username);
  username[i] = ':';
  while (++i <= USERNAME_LEN) username[i] = ' ';
}
 

void disable_line_buffering_and_echo() {
  struct termios term;
  tcgetattr(STDIN_FILENO, &term);
  term.c_lflag &= ~ICANON;
  term.c_lflag &= ~ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &term);
  setbuf(stdin, NULL);
}


/*
 * Find window size, disable line buffering and keypress echo,
 * setup interface and initial cursor position.
 */
void setup_terminal() {
  if (uv_tty_get_winsize(&tty, &width, &height)) {
    fprintf(stderr, "Could not get TTY information\n");
    uv_tty_reset_mode();
    return;
  }
  disable_line_buffering_and_echo();
  write_req_t *req = (write_req_t*) malloc(sizeof(write_req_t));
  req->buf.base = (char*) malloc(MSG_BUF_SIZE);
  int prompt_pos = height - INPUT_AREA_HEIGHT;
  req->buf.len = sprintf(req->buf.base,
                         "\033[2J\033[H\033[%dB------------------\nYour message: ",
                         prompt_pos);
  uv_write((uv_write_t*) req, (uv_stream_t*) &tty, &req->buf, 1, after_write);
}


void remove_old_messages() {
  write_req_t *req = (write_req_t*) malloc(sizeof(write_req_t));
  req->buf.base = (char*) malloc(MSG_BUF_SIZE);
  int prompt_pos = height - INPUT_AREA_HEIGHT;
  req->buf.len = sprintf(req->buf.base,
                         "\033[2J\033[H\033[%dB------------------\nYour message: %s",
                         prompt_pos, user_msg.base);
  uv_write((uv_write_t*) req, (uv_stream_t*) &tty, &req->buf, 1, after_write);
}


/*
 * Clear old messages if screen is full,
 * print received message to screen and return cursor to message input area.
 */
void update_chat(const uv_buf_t *message) {
  if (msgs_on_screen == height - INPUT_AREA_HEIGHT - 1) {
    remove_old_messages();
    msgs_on_screen = 1;
  } else {
    msgs_on_screen++;
  }
  write_req_t *req = (write_req_t*) malloc(sizeof(write_req_t));
  req->buf.base = (char*) malloc(MSG_BUF_SIZE);

  int cursor_y = height - INPUT_AREA_HEIGHT + 1;
  int cursor_x = PROMPT_LEN + user_msg.len;
  req->buf.len = sprintf(req->buf.base, "\033[H\033[%dB%s\033[H\033[%dB\033[%dC",
                         msgs_on_screen, message->base, cursor_y, cursor_x);
  uv_write((uv_write_t*) req, (uv_stream_t*) &tty, &req->buf, 1, after_write);
}


void receive_message(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
  if (nread > 0) {
    update_chat(buf);
    return;
  } else if (nread < 0) {
    if (nread != UV_EOF) {
      fprintf(stderr, "Read error %s\n", uv_err_name(nread));
    }
    printf("close\n");
    uv_close((uv_handle_t*) client, NULL);
  }
  free(buf->base);
}


/*
 * Send username and message to server if message not empty.
 */
void send_message() {
  if (user_msg.len == 0) {
    return;
  }
  write_req_t *req = (write_req_t*) malloc(sizeof(write_req_t));
  req->buf.len = USERNAME_LEN + user_msg.len + 1;
  req->buf.base = (char*) malloc(USERNAME_LEN + user_msg.len + 1);
  memcpy(req->buf.base, username, USERNAME_LEN);
  memcpy(req->buf.base + USERNAME_LEN, user_msg.base, user_msg.len + 1);

  uv_write((uv_write_t*) req, (uv_stream_t*) socket_, &req->buf, 1, after_write);
  for (int i = 0; i < MAX_MSG_LEN; i++) {
    user_msg.base[i] = ' ';
  }
  user_msg.len = 0;
}


void on_connect(uv_connect_t* req, int status) {
  if (status < 0) {
    fprintf(stderr, "New connection error %s\n", uv_strerror(status));
    return;
  } else {
    //printf("Connected to server: %s\n", DEFAULT_SERVER_IP);
  }
  strcpy(user_msg.base, "**connected**");
  user_msg.len = strlen(user_msg.base);
  send_message();

  uv_read_start((uv_stream_t*)req->handle, alloc_buffer, receive_message);
}

/*
 * Update user_msg and tty depending on input char:
 *   - if char is printable, print
 *   - if user pressed backspace, delete char from message input
 */
void update_user_message(char input) {
  int cursor_y = height - INPUT_AREA_HEIGHT + 1;
  int cursor_x = PROMPT_LEN + user_msg.len;

  write_req_t *req = (write_req_t*) malloc(sizeof(write_req_t));
  req->buf.base = (char*) malloc(MSG_BUF_SIZE);

  if (input == KEY_BACKSPACE && user_msg.len > 0) {
    user_msg.len--;
    cursor_x--;
    req->buf.len = sprintf(req->buf.base, "\033[H\033[%dB\033[%dC \033[1D",
                           cursor_y, cursor_x);
    uv_write((uv_write_t*) req, (uv_stream_t*) &tty, &req->buf, 1, after_write);
  } else if (isprint(input) && user_msg.len < MAX_MSG_LEN) {
    user_msg.base[user_msg.len++] = input;
    user_msg.base[user_msg.len] = '\0';
    req->buf.len = sprintf(req->buf.base, "\033[H\033[%dB\033[%dC%c",
                           cursor_y, cursor_x, input);
    uv_write((uv_write_t*) req, (uv_stream_t*) &tty, &req->buf, 1, after_write);
  } else {
    free(req->buf.base);
    free(req);
  }
}



void clear_message_input() {
  write_req_t *req = (write_req_t*) malloc(sizeof(write_req_t));
  req->buf.base = (char*) malloc(MSG_BUF_SIZE);


  int cursor_y = height - INPUT_AREA_HEIGHT + 1;
  int cursor_x = PROMPT_LEN;
  req->buf.len = sprintf(req->buf.base, "\033[H\033[%dB\033[%dC\033[0J",
                         cursor_y, cursor_x);//, user_msg.base);
  uv_write((uv_write_t*) req, (uv_stream_t*) &tty, &req->buf, 1, after_write);
}


/*
 * Process single keypress. (line buffering is turned off)
 * Update screen and send message if user pressed enter.
 */
void process_char(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
  if (nread < 0){
    printf("close\n");
    if (nread == UV_EOF) {
      printf("end of file\n");
    }
  } else if (nread == 1) {
    char c = buf->base[0];
    if (c == '\n') {
      send_message();
      clear_message_input();
    } else if (isprint(c) || c == KEY_BACKSPACE) {
      update_user_message(c);
    }
  }
  free(buf->base);
}


/*
 * TCP chat client
 */
int main() {
  get_username();

  user_msg.base = (char*) malloc(MAX_MSG_LEN + 1);
  user_msg.len = 0;

  loop = uv_default_loop();

  uv_pipe_init(loop, &in, 0);
  uv_pipe_open(&in, 0);
  uv_read_start((uv_stream_t*)&in, alloc_buffer, process_char);

  socket_ = (uv_tcp_t*) malloc(sizeof(uv_tcp_t));
  uv_tcp_init(loop, socket_);
  uv_connect_t* connect_ = (uv_connect_t*) malloc(sizeof(uv_connect_t));
  struct sockaddr_in addr;
  uv_ip4_addr(DEFAULT_SERVER_IP, DEFAULT_SERVER_PORT, &addr);
  int r = uv_tcp_connect(connect_, socket_, (const struct sockaddr*)&addr, on_connect);
  if (r) {
    fprintf(stderr, "Server connection error %s\n", uv_strerror(r));
    return 1;
  }

  uv_tty_init(loop, &tty, STDOUT_FILENO, 0);
  uv_tty_set_mode(&tty, UV_TTY_MODE_NORMAL);
  setup_terminal();

  return uv_run(loop, UV_RUN_DEFAULT);
}
