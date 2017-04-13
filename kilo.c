#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <string.h>

// Defines

#define CTRL_KEY(k) ((k) & 0x1f)
#define KILO_VERSION "0.0.1"

// Data

struct editorConfig {
  struct termios orig_termois;
  int screenrows;
  int screencols;
  int x, y;
};

struct editorConfig E;

// Terminal
void die (const char *s){
  write (STDOUT_FILENO, "\x1b[2J", 4);
  write (STDOUT_FILENO, "\x1b[H", 3);
  
  perror(s);
  exit(1);

}

void disableRawMode() {
  if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termois) == -1){
    die("tcsetattr");
  }
}

void enableRawMode(){
 
  if( tcgetattr(STDIN_FILENO, &E.orig_termois) == -1){
    die("tcgetattr");
  }
  atexit(disableRawMode);

  struct termios raw = E.orig_termois;

  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag  &= ~(OPOST);
  raw.c_cflag |= (CS8);
  //raw.c_lflag &= ~(ICRNL | IXON);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG );
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;
  
   if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termois) == -1){
    die("tcsetattr");
  }

}

char editorReadKey(){
  int nread;
  char c;
  while((nread = read(STDIN_FILENO, &c, 1)) != 1){
    if (nread == -1 && errno!= EAGAIN) {
      die ("read");
    }

  }
  return c;
}

int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;
  if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4){
    return -1;
  }
  while(i < sizeof(buf) -1){
    if(read(STDIN_FILENO, &buf[i], 1) != 1) {
      break;
    }
    if(buf[i] == 'R'){
      break;
    }
    i++;
  }
  buf[i] = '\0';
  if(buf[0] != '\x1b' || buf[1] != '[') {
    return -1;
  }
  if(sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
  //printf("\r\n&buf[1]: '%s'\r\n", &buf[1]);
  //editorReadKey();
  return -1;
}
/*

  //char buf[32];
  unsigned int i = 0;
  if (write(STDIN_FILENO, "\x1b[6n",4) != 4){
    return -1;
  }
  while (i < sizeof(buf) - 1){
    if (read (STDOUT_FILENO, &buf[i], 1) != 1) {
      break;
    }
    if (buf[i] == 'R'){
      break;
    }
    i++;
  }
  buf[i] = '\0';
  if(buf[0] != '\x1b' || buf[1] != '[') {
    return -1;
  }
  if(sscanf(&buf[2], "%d;%d", rows, cols) != 2) {
    return -1;
  }
  //printf ("\r\n&buf[1]: '%s'\r\n", &buf[1]);
  editorReadKey();
  return -1;

}
*/
int getWindowSize (int *rows, int *cols) {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0){
    if(write(STDOUT_FILENO, "\x1b[999C\1xb[999B", 12) != 12) {
      return getCursorPosition(rows, cols);
    }
  }
  else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}


// Append to buffer

struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0} 

void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);

  if (new == NULL){
    return;
  }
  memcpy(&new[ab->len],s,len);
  ab->b = new;
  ab->len += len;
}

void abFree (struct abuf *ab){
  free (ab->b);
}


// output

void editorDrawRows(struct abuf *ab) {
  int y;
  for (y = 0; y < E.screenrows; y++){
    //  if (y == E.screenrows / 3) {
    // char welcome[80];
      //int welcomelen = snprintf(welcome, sizeof(welcome),
      //	"Kilo editor -- version %s", KILO_VERSION);
      /* if(welcomelen > E.screencols){
	welcomelen = E.screencols;
      }

      */
      //abAppend(ab, welcome, welcomelen);
      //    }
      //else{
      abAppend(ab, "~", 1);
      // }
      //abAppend(ab, "\1xb[K", 3);
    // write(STDOUT_FILENO, "~", 3);
    if(y < E.screenrows - 1){
      //write(STDOUT_FILENO, "\r\n", 2);
      abAppend(ab, "\r\n", 2);
    }
  }
}

void editorRefreshScreen(){

  struct abuf ab = ABUF_INIT;
  abAppend(&ab, "\x1b[?25l", 6);
  // abAppend(&ab, "\x1b[2J", 4);
  abAppend(&ab, "\x1b[H", 3);

  editorDrawRows(&ab);
  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.y + 1, E.x + 1);
  abAppend(&ab, buf, strlen(buf));
  
  //abAppend(&ab, "\x1b[H", 3);
  abAppend(&ab, "\x1b[?25h", 6);
  write (STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
  /*  write(STDIN_FILENO, "\x1b[2J", 4);
  write(STDIN_FILENO, "\x1b[H", 3);
  struct abuf ab = ABUF_INIT;

  editorDrawRows(&ab);
  write(STDIN_FILENO, "\x1b[H", 3);

  abFree(&ab);
  */
}

// Input
void editorMoveCursor(char key){
    if (key == 'a'){
    E.x--;
  }
  if (key == 'd'){
    E.x++;
  }
  if (key == 'w'){
    E.y--;
  }
  if(key == 's') {
    E.y++;
  }
  return;
}

void editorKeypress(){
  char c = editorReadKey();
  switch (c) {
  case CTRL_KEY('q'):
    write (STDOUT_FILENO, "\x1b[2J", 4);
    write (STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
    break;
  }
  if (c == 'w' || c == 's' || c == 'a' || c == 'd'){
    editorMoveCursor(c);
  }
    /*
  case 'w':
  case 's':
  case 'a':
  case 'd':
    editorMoveCursor(c);
    break;
  }
  */
}



// init
void initEditor() {
  E.x = 0;
  E.y = 10;
  if (getWindowSize(&E.screenrows, &E.screencols) == -1){
    die ("get window size");
  }
}
int main() {
  enableRawMode();
  initEditor();
  while(1){
    editorRefreshScreen();
    editorKeypress();
    
  }
  return 0;

}
