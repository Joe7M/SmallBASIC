// This file is part of SmallBASIC
//
// Copyright(C) 2001-2018 Chris Warren-Smith.
// Copyright(C) 2000 Nicholas Christopoulos
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//

// Thanks to: https://viewsourcecode.org/snaptoken/kilo
//            https://en.wikipedia.org/wiki/ANSI_escape_code

#include "config.h"
#include "include/osd.h"
#include "common/device.h"
#include "common/plugins.h"
#include "common/smbas.h"
#include "common/keymap.h"
#include <stdio.h>
#include "terminal.h"

#if USE_TERM_IO
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#endif

#define WAIT_INTERVAL 5

#if USE_TERM_IO
struct winsize consoleSize;
struct termios termiosOriginal, termiosConsole;
#endif

typedef void (*settextcolor_fn)(long fg, long bg);
typedef void (*setpenmode_fn)(int enable);
typedef int  (*getpen_fn)(int code);
typedef void (*cls_fn)();
typedef int  (*getx_fn)();
typedef int  (*gety_fn)();
typedef void (*setxy_fn)(int x, int y);
typedef void (*write_fn)(const char *str);
typedef int  (*events_fn)(int, int*, int*);
typedef void (*setcolor_fn)(long color);
typedef void (*line_fn)(int x1, int y1, int x2, int y2);
typedef void (*ellipse_fn)(int xc, int yc, int xr, int yr, int fill);
typedef void (*arc_fn)(int xc, int yc, double r, double as, double ae, double aspect);
typedef void (*setpixel_fn)(int x, int y);
typedef long (*getpixel_fn)(int x, int y);
typedef void (*rect_fn)(int x1, int y1, int x2, int y2, int fill);
typedef void (*refresh_fn)();
typedef void (*beep_fn)();
typedef void (*sound_fn)(int frq, int ms, int vol, int bgplay);
typedef void (*clear_sound_queue_fn)();
typedef void (*audio_fn)(const char *path);
typedef int  (*textwidth_fn)(const char *str);
typedef int  (*textheight_fn)(const char *str);
typedef int  (*init_fn)(const char *prog, int width, int height);

static settextcolor_fn p_settextcolor;
static setpenmode_fn p_setpenmode;
static getpen_fn p_getpen;
static cls_fn p_cls;
static getx_fn p_getx;
static gety_fn p_gety;
static setxy_fn p_setxy;
static write_fn p_write;
static events_fn p_events;
static setcolor_fn p_setcolor;
static line_fn p_line;
static ellipse_fn p_ellipse;
static arc_fn p_arc;
static setpixel_fn p_setpixel;
static getpixel_fn p_getpixel;
static rect_fn p_rect;
static refresh_fn p_refresh;
static beep_fn p_beep;
static sound_fn p_sound;
static clear_sound_queue_fn p_clear_sound_queue;
static audio_fn p_audio;
static textwidth_fn p_textwidth;
static textheight_fn p_textheight;

extern long foregroundColor;
extern long backgroundColor;

int get_escape(const char *str, int begin, int end) {
  int result = 0;
  for (int i = begin; i < end; i++) {
    if (isdigit(str[i])) {
      result = (result * 10) + (str[i] - '0');
    } else {
      break;
    }
  }
  return result;
}

//
// Console output if vt100 (esc sequences) is not supported
//
void default_write(const char *str) {
  static int column = 0;
  int len = strlen(str);
  if (len) {
    int escape = 0;
    for (int i = 0; i < len; i++) {
      if (i + 1 < len && str[i] == '\033' && str[i + 1] == '[') {
        i += 2;
        escape = i;
      } else if (escape && str[i] == 'G') {
        // move to column
        int escValue = get_escape(str, escape, i);
        while (escValue > column) {
          putc(' ', stdout);
          column++;
        }
        escape = 0;
      } else if (escape && str[i] == 'm') {
        escape = 0;
      } else if (!escape) {
        putc(str[i], stdout);
        column = (str[i] == '\n') ? 0 : column + 1;
      }
    }
  }
  fflush(stdout);
}

long convertColor(long color) {
  float r = ((float)((-color & 0xff0000) >> 16)) / 255.0;
  float g = ((float)((-color & 0xff00) >> 8)) / 255.0;
  float b = ((float)(-color & 0xff)) / 255.0;
  
  // gray scale
  if (r == g && r == b && g == b) {
    return (232 + (long)round(23.0*r));
  }
  
  return (16 + (long)round((36.0 * 5.0*r + 6 * 5.0*g + 5.0*b)));
}

//
// console output if vt100 (esc sequences) is supported
//
void vt100_write(const char *str) {
  printf("%s", str);
  fflush(stdout);
}

void console_init() {
  p_write = default_write;
}

#if USE_TERM_IO
void enableTerminalRawMode(void) {
  tcgetattr(STDIN_FILENO, &termiosConsole);
  termiosOriginal = termiosConsole;
  termiosConsole.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  termiosConsole.c_cflag |= (CS8);
  termiosConsole.c_lflag &= ~(ECHO | ICANON | IEXTEN);
  //termiosConsole.c_oflag &= ~(OPOST);   // turns off post processing \n -> \n\r
  termiosConsole.c_cc[VMIN] = 0;
  termiosConsole.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &termiosConsole);
}

void disableTerminalRawMode(void) {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &termiosOriginal);
}

void initTerminal(void) {
  enableTerminalRawMode();
  atexit(disableTerminalRawMode);
}

void getTerminalSize(int *x, int *y) {
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &consoleSize);
  *x = consoleSize.ws_col;
  *y = consoleSize.ws_row;
}

int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;
  long int startTime;
  
  *rows = 0;
  *cols = 0;
  
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) {
    return -1;
  }
  // There might be a slight delay between write() and the
  // response from the terminal printing the escape sequence.
  // Therfore we try to read several times until either the
  // timeout is reached or the 'R' letter is printed
  startTime = dev_get_millisecond_count();
  while (i < sizeof(buf) - 1) {
    if (dev_get_millisecond_count() - startTime > 50) {
      return -1;
    } 
    if (read(STDIN_FILENO, &buf[i], 1) != 1) {
      continue;
    }
    if (buf[i] == 'R') {
      break;
    }
    i++;
  }

  buf[i] = '\0';
  if (buf[0] != '\x1b' || buf[1] != '[') {
    return -1;
  }
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) {
    *rows = 0;
    *cols = 0;
    return -1;
  }

  return 0;
}

void clearScreen(void) {
  // VT100: Move cursor to 1,1 and clear screen from cursor
  printf("\033[H\033[J");
  fflush(stdout);
}

void setTextColor(long fg, long bg) {
  // VT100: 3bit and 4 bit color
  /*if (fg < 8) {
    fg = 30 + fg;
  } else {
    fg = 90 - 8 + fg;
  }
  if (bg < 8) {
    bg = 40 + bg;
  } else {
    bg = 100 - 8 + bg;
  }
  printf("\033[%ld;%ldm", fg, bg);
  */
  
  //VT100: 8bit color mode
  if (fg >= 0) {
    fg = fg & 0xFF;
  } else {
    fg = convertColor(fg);
  }
  if (bg >= 0) {
    bg = bg & 0xFF;
  } else { 
    bg = convertColor(bg);
  }
  printf("\033[38;5;%ldm\033[48;5;%ldm", fg, bg);
  fflush(stdout);
}

void setCursorPosition(int row, int col) {
  printf("\033[%d;%dH", row, col);
  fflush(stdout);
}

// Bit 0 to 7 is drawing color
// Bit 8 to 15 is drawing character
void setForegroundColor(long fg) {
  setDrawingCharacter(fg);
  //VT100: 8bit color mode
  printf("\033[48;5;%ldm", fg & 0xFF);   // set background color
  fflush(stdout);  
}

#elif defined (_Win32)
void initTerminal(void) {
  // Enable vt100 support in newer versions of Windows 10 or 11.
  // If vt100 is not supported fall back to default_write.
  // See: https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences

  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  if (hOut == INVALID_HANDLE_VALUE) {
    p_write = default_write;
    return 1;
  }

  DWORD dwMode = 0;
  if (!GetConsoleMode(hOut, &dwMode)) {
    p_write = default_write;
    return 1;
  }

  dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  if (!SetConsoleMode(hOut, dwMode)) {
    p_write = default_write;
    return 1;
  }
}

void getTerminalSize(int *x, int *y) {
  *x = 0;
  *y = 0;
}

int getCursorPosition(int *rows, int *cols) {
  *rows = 0;
  *cols = 0;
  return 0;
}

void clearScreen(void) {
  // VT100: Move cursor to 1,1 and clear screen from cursor
  printf("\033[H\033[J");
  fflush(stdout);
}

void setTextColor(long fg, long bg) {
  // VT100: 3bit and 4 bit color
  if (fg < 8) {
    fg = 30 + fg;
  } else {
    fg = 90 - 8 + fg;
  }
  if (bg < 8) {
    bg = 40 + bg;
  } else {
    bg = 100 - 8 + bg;
  }
  printf("\033[%ld;%ldm", fg, bg);
  fflush(stdout);
}

void setCursorPosition(int x, int y) {
  printf("\033[%d;%dH", x, y);
  fflush(stdout);
}

void setForegroundColor(long color) {
  // VT100: 3bit and 4 bit color
  if (color < 8) {
    color = 30 + color;
  } else {
    color = 90 - 8 + color;
  }
  printf("\033[%ldm", color);
}
#else
void initTerminal(void) {}
void clearScreen(void) {}
void setTextColor(long fg, long bg) {}
void setCursorPosition(int x, int y) {}
void setForegroundColor(long color) {}
void getTerminalSize(int *x, int *y) {
  *x = 0;
  *y = 0;
}
int getCursorPosition(int *rows, int *cols) {
  *rows = 0;
  *cols = 0;
  return 0;
}
#endif

void readKey(void) {
  uint32_t c = terminalReadKey();
  if (c > 0) {
    dev_clrkb();
    dev_pushkey(c);
  }
}

//
// initialize driver
//
int osd_devinit() {
  p_arc = (arc_fn)plugin_get_func("sblib_arc");
  p_audio = (audio_fn)plugin_get_func("sblib_audio");
  p_beep = (beep_fn)plugin_get_func("sblib_beep");
  p_clear_sound_queue = (clear_sound_queue_fn)plugin_get_func("sblib_clear_sound_queue");
  p_cls = (cls_fn)plugin_get_func("sblib_cls");
  p_ellipse = (ellipse_fn)plugin_get_func("sblib_ellipse");
  p_events = (events_fn)plugin_get_func("sblib_events");
  p_getpen = (getpen_fn)plugin_get_func("sblib_getpen");
  p_getpixel = (getpixel_fn)plugin_get_func("sblib_getpixel");
  p_getx = (getx_fn)plugin_get_func("sblib_getx");
  p_gety = (gety_fn)plugin_get_func("sblib_gety");
  p_line = (line_fn)plugin_get_func("sblib_line");
  p_rect = (rect_fn)plugin_get_func("sblib_rect");
  p_refresh = (refresh_fn)plugin_get_func("sblib_refresh");
  p_setcolor = (setcolor_fn)plugin_get_func("sblib_setcolor");
  p_setpenmode = (setpenmode_fn)plugin_get_func("sblib_setpenmode");
  p_setpixel = (setpixel_fn)plugin_get_func("sblib_setpixel");
  p_settextcolor = (settextcolor_fn)plugin_get_func("sblib_settextcolor");
  p_setxy = (setxy_fn)plugin_get_func("sblib_setxy");
  p_sound = (sound_fn)plugin_get_func("sblib_sound");
  p_textheight = (textheight_fn)plugin_get_func("sblib_textheight");
  p_textwidth = (textwidth_fn)plugin_get_func("sblib_textwidth");
  p_write = (write_fn)plugin_get_func("sblib_write");

  init_fn devinit = (init_fn)plugin_get_func("sblib_devinit");
  if (devinit) {
    devinit(prog_file, opt_pref_width, opt_pref_height);
  } else {
    initTerminal();
  }
  os_graf_mx = opt_pref_width;
  os_graf_my = opt_pref_height;
  setsysvar_int(SYSVAR_XMAX, os_graf_mx);
  setsysvar_int(SYSVAR_YMAX, os_graf_my);

  if (p_write == NULL) {
    // Test if output is printed in a terminal. If output is piped into
    // a text file or sbasic is running as a cron job, use
    // default_write without vt100 support
    if (!isatty(STDOUT_FILENO)) {
      p_write = default_write;
      return 1;
    }
    p_write = vt100_write;
  }
  return 1;
}

//
// close driver
//
int osd_devrestore() {
  return 1;
}

//
// set foreground and background color
// a value of -1 means not change that color
// called by COLOR keyword
//
void osd_settextcolor(long fg, long bg) {
  if (p_settextcolor) {
    p_settextcolor(fg, bg);
  } else {
    setTextColor(fg, bg);
    foregroundColor = fg;
    backgroundColor = bg;
  }
}

//
// enable or disable PEN/MOUSE driver
//
void osd_setpenmode(int enable) {
  if (p_setpenmode) {
    p_setpenmode(enable);
  }
}

//
// return pen/mouse info ('code' is the rq, see doc)
//
int osd_getpen(int code) {
  int result;
  if (p_getpen) {
    result = p_getpen(code);
  } else {
    result = 0;
  }
  return result;
}

//
// clear screen
//
void osd_cls() {
  if (p_cls) {
    p_cls();
  } else {
    clearScreen();
  }
}

//
// returns the current x position (text-mode cursor)
//
int osd_getx() {
  int result;
  if (p_getx) {
    result = p_getx();
  } else {
    int cols = 0, rows = 0;
    getCursorPosition(&rows, &cols);
    result = cols;
  }
  return result;
}

//
// returns the current y position (text-mode cursor)
//
int osd_gety() {
  int result;
  if (p_gety) {
    result = p_gety();
  } else {
    int cols = 0, rows = 0;
    getCursorPosition(&rows, &cols);
    result = rows;
  }
  return result;
}

//
// set's text-mode (or graphics) cursor position
//
void osd_setxy(int x, int y) {
  if (p_setxy) {
    p_setxy(x, y);
  } else {
    setCursorPosition(x, y);
  }
}

//
// Basic output - print sans control codes
//
void osd_write(const char *str) {
  p_write(str);
}

//
// events loop (called from main, every 50ms)
//
int osd_events(int wait_flag) {
  int result = 0;
  int x = os_graf_mx;
  int y = os_graf_my;
  
  if (p_events) {
    result = p_events(wait_flag, &x, &y);
  } else {
    switch (wait_flag) {
    case 1:
      //Wait for events
      break;
    case 2:
      usleep(WAIT_INTERVAL * 1000);
      break;
    default:
      //poll events();
      break;
    }

    getTerminalSize(&x, &y);    // XMAX, YMAX
    readKey();                  // INKEY
  }

  if (x != os_graf_mx || y != os_graf_my) {
    dev_resize(x, y);
  }

  return result;
}

//
// sets foreground color for drawing routines like LINE or RECT
//
void osd_setcolor(long color) {
  if (p_setcolor) {
    p_setcolor(color);
  } else {
    setForegroundColor(color);
    foregroundColor = color;
  }
}

//
// draw a line
//
void osd_line(int x1, int y1, int x2, int y2) {
  if (p_line) {
    p_line(x1, y1, x2, y2);
  } else {
    drawLine(x1, y1, x2, y2);
  }
}

//
// draw an ellipse
//
void osd_ellipse(int xc, int yc, int xr, int yr, int fill) {
  if (p_ellipse) {
    p_ellipse(xc, yc, xr, yr, fill);
  }
}

//
// draw an arc
//
void osd_arc(int xc, int yc, double r, double as, double ae, double aspect) {
  if (p_arc) {
    p_arc(xc, yc, r, as, ae, aspect);
  }
}

//
// draw a pixel
//
void osd_setpixel(int x, int y) {
  if (p_setpixel) {
    p_setpixel(x, y);
  } else {
    drawPoint(x, y);
  }
}

//
// returns pixel's color
//
long osd_getpixel(int x, int y) {
  long result;
  if (p_getpixel) {
    result = p_getpixel(x, y);
  } else {
    result = 0;
  }
  return result;
}

//
// draw rectangle (parallelogram)
//
void osd_rect(int x1, int y1, int x2, int y2, int fill) {
  if (p_rect) {
    p_rect(x1, y1, x2, y2, fill);
  } else {
    drawRect(x1, y1, x2, y2, fill);
  }
}

//
// refresh/flush the screen/stdout
//
void osd_refresh() {
  if (p_refresh) {
    p_refresh();
  } else {
    osd_events(0);
    fflush(stdout);
  }
}

//
// just a beep
//
void osd_beep() {
  if (p_beep) {
    p_beep();
  } else {
    printf("\a");
  }
}

//
// play a sound
//
void osd_sound(int frq, int ms, int vol, int bgplay) {
  if (p_sound) {
    p_sound(frq, ms, vol, bgplay);
  }
}

//
// clears sound-queue (stop background sound)
//
void osd_clear_sound_queue() {
  if (p_clear_sound_queue) {
    p_clear_sound_queue();
  }
}

//
// play the given audio file
//
void osd_audio(const char *path) {
  if (p_audio) {
    p_audio(path);
  }
}

//
// text-width in pixels
//
int osd_textwidth(const char *str) {
  int result;
  if (p_textwidth) {
    result = p_textwidth(str);
  } else {
    result = strlen(str);
  }
  return result;
}

//
// text-height in pixels
//
int osd_textheight(const char *str) {
  int result;
  if (p_textheight) {
    result = p_textheight(str);
  } else {
    result = 1;
  }
  return result;
}

//
// delay while pumping events
//
void dev_delay(uint32_t timeout) {
  uint32_t slept;
  uint32_t now = dev_get_millisecond_count();
  while (1) {
    if (osd_events(0) < 0 || timeout == 0) {
      break;
    }
    slept = dev_get_millisecond_count() - now;
    if (timeout - slept > WAIT_INTERVAL) {
      usleep(WAIT_INTERVAL * 1000);
    } else {
      usleep((timeout - slept) * 1000);
      break;
    }
  }
}
//
// update terminal size and flush stdout
//
void dev_show_page() {
  osd_refresh();
}

//
// unused
//
void dev_log_stack(const char *keyword, int type, int line) {}
void v_create_form(var_p_t var) {}
void v_create_window(var_p_t var) {}

