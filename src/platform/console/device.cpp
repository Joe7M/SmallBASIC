// This file is part of SmallBASIC
//
// Copyright(C) 2001-2018 Chris Warren-Smith.
// Copyright(C) 2000 Nicholas Christopoulos
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//

// Thanks to: https://viewsourcecode.org/snaptoken/kilo        -> Linux RAW mode
//            https://en.wikipedia.org/wiki/ANSI_escape_code   -> Escape codes
//            https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h2-Mouse-Tracking  -> Mouse in terminal

#include "config.h"
#include "include/osd.h"
#include "common/device.h"
#include "common/plugins.h"
#include "common/smbas.h"
#include "common/keymap.h"
#include "common/sberr.h"
#include <stdio.h>
#include "vt100.h"

#if USE_TERM_IO
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <signal.h>
#elif defined(_Win32)
#include <windows.h>
#include <signal.h>
#endif

#define WAIT_INTERVAL 5

typedef void (*settextcolor_fn)(long fg, long bg);
typedef void (*setpenmode_fn)(int enable);
typedef int  (*getpen_fn)(int code);
typedef void (*cls_fn)();
typedef int  (*getx_fn)();
typedef int  (*gety_fn)();
typedef void (*setxy_fn)(int x, int y);
typedef void (*write_fn)(const char *str);
typedef int  (*events_fn)(int wait_flag, int *x, int *y);
typedef void (*setcolor_fn)(long color);
typedef void (*line_fn)(int x1, int y1, int x2, int y2);
typedef void (*ellipse_fn)(int xc, int yc, int xr, int yr, int fill);
typedef void (*arc_fn)(int xc, int yc, double r, double as, double ae, double aspect);
typedef void (*rect_fn)(int x1, int y1, int x2, int y2, int fill);
typedef void (*setpixel_fn)(int x, int y);
typedef long (*getpixel_fn)(int x, int y);
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

void console_init() {
  p_write = default_write;
}

#if USE_TERM_IO
struct winsize consoleSize;
struct termios original_termios;

/**
 * @brief Handler for Ctrl+C
 */
void handle_signal(int sig) {
  brun_break();
}

/**
 * @brief Sets the terminal to non-canonical (raw) mode.
 */
void terminal_init(void) {
  // 1. Get current terminal attributes
  tcgetattr(STDIN_FILENO, &original_termios);

  // 2. Copy them for restoration later
  struct termios raw = original_termios;

  // 3. Modify settings:
  //    Disable canonical mode (ICANON)
  //    Disable echo (ECHO)
  //    Disable Ctrl-S and Ctrl-Q (IXON)
  //    Disable Ctrl-V (IEXTEN)
  //    Fix Ctrl-M (ICRNL)
  //    Disable break condition (BRKINT)
  //    Disable parity checking (INPCK)
  //    Disable stripping of 8th bit of each input (ISTRIP)
  //    Set character size to 8 bit (CS8)
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN);

  // 4. Set timeout and minimum number of bytes for read()
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 0;

  // 5. Apply the new settings
  tcsetattr(STDIN_FILENO, TCSANOW, &raw);

  // 6. Register signal handlers
  struct sigaction sa = {0};
  sa.sa_handler = handle_signal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESETHAND;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGHUP, &sa, NULL);
}

void terminal_getSize(int *x, int *y) {
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &consoleSize);
  *x = consoleSize.ws_col;
  *y = consoleSize.ws_row;
}

/**
 * @brief Restores the terminal to its original settings.
 */
void terminal_close() {
  static volatile sig_atomic_t cleaned_up = 0;

  if (!cleaned_up) {
    cleaned_up = 1;
    int col = 0, row = 0;

    // disable "All Motion Mouse Tracking" and SGR if somehow still enabled
    write(STDOUT_FILENO, "\033[?1003l", 8);
    write(STDOUT_FILENO, "\033[?1006l", 8);

    // leave alt screen if somehow entered
    // this messes up cursor position
    vt100_getCursorPosition(&row, &col);
    write(STDOUT_FILENO, "\033[?1049l", 8);
    vt100_setCursorPosition(row, col);

    write(STDOUT_FILENO, "\033[0m", 4);     // reset colors
    write(STDOUT_FILENO, "\033[?25h", 6);   // restore cursor
    write(STDOUT_FILENO, "\033[J", 3);      // clear screen from cursor down

    tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);  // restore terminal settings
  }
}

#elif defined (_Win32)
CONSOLE_SCREEN_BUFFER_INFO screenBufferInfo;
HANDLE hIn;
HANDLE hOut;
DWORD original_dwMode_Out;
DWORD original_dwMode_In;

void terminal_init(void) {
  // Enable vt100 support in newer versions of Windows 10 or 11.
  // If vt100 is not supported fall back to default_write.
  // See: https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences

  hIn = GetStdHandle(STD_INPUT_HANDLE);
  if (hIn == INVALID_HANDLE_VALUE) {
    p_write = default_write;
    return;
  }

  if (!GetConsoleMode(hIn, &original_dwMode_In)) {
    p_write = default_write;
    return;
  }

  DWORD dwMode = (ENABLE_VIRTUAL_TERMINAL_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_WINDOW_INPUT)
    & ~ENABLE_ECHO_INPUT
    & ~ENABLE_INSERT_MODE
    & ~ENABLE_LINE_INPUT
    & ~ENABLE_QUICK_EDIT_MODE;
  if (!SetConsoleMode(hIn, dwMode)) {
    p_write = default_write;
    return;
  }

  hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  if (hOut == INVALID_HANDLE_VALUE) {
    p_write = default_write;
    return;
  }

  if (!GetConsoleMode(hOut, &original_dwMode_Out)) {
    p_write = default_write;
    return;
  }

  dwMode = ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  if (!SetConsoleMode(hOut, dwMode)) {
    p_write = default_write;
    return;
  }
}

void terminal_getSize(int *cols, int *rows) {
  if (GetConsoleScreenBufferInfo(hOut, &screenBufferInfo)) {
    *cols = screenBufferInfo.srWindow.Right - screenBufferInfo.srWindow.Left + 1;
    *rows = screenBufferInfo.srWindow.Bottom - screenBufferInfo.srWindow.Top + 1;
  } else {
    *cols = 0;
    *rows = 0;
  }
}

void terminal_close(void) {
  static volatile sig_atomic_t cleaned_up = 0;

  if (!cleaned_up) {
    cleaned_up = 1;
    int col = 0, row = 0;

    // disable "All Motion Mouse Tracking" and SGR if somehow still enabled
    WriteConsole(hOut, "\033[?1003l", 8, NULL, NULL);
    WriteConsole(hOut, "\033[?1006l", 8, NULL, NULL);

    // leave alt screen if somehow entered
    // this messes up cursor position
    vt100_getCursorPosition(&row, &col);
    WriteConsole(hOut, "\033[?1049l", 8, NULL, NULL);
    vt100_setCursorPosition(row, col);

    WriteConsole(hOut, "\033[0m", 4, NULL, NULL);     // reset colors
    WriteConsole(hOut, "\033[?25h", 6, NULL, NULL);   // restore cursor
    WriteConsole(hOut, "\033[J", 3, NULL, NULL);      // clear screen from cursor down

    SetConsoleMode(hOut, original_dwMode_Out);
    SetConsoleMode(hIn, original_dwMode_In);
  }
};
#endif

static void exit_handler(void) {
  terminal_close();
  if (count_tasks()) {
    err_abnormal_exit();
  }
}

/*
 * Default functions will be used, if
 * 1. not USE_TERM_IO
 * 2. not _Win32
 * 3. isatty() -> false
*/
void default_drawArc(int xc, int yc, double r, double as, double ae, double aspect) {}
void default_playAudio(const char *path) {}
void default_playSound(int frq, int ms, int vol, int bgplay) {}
void default_playBeep(void) {}
void default_playClearSoundQueue(void) {}
void default_clearScreen(void) {}
void default_drawEllipse(int xc, int yc, int xr, int yr, int fill) {}
int default_terminalEvents(int wait_flag, int *x, int *y) {return 0;}
int default_getMouse(int code) {return 0;}
long default_drawGetPixel(int x, int y) {return 0;}
int default_cursorGetx(void) {return 0;}
int default_cursorGety(void) {return 0;}
void default_drawLine(int x1, int y1, int x2, int y2) {}
void default_drawRect(int x1, int y1, int x2, int y2, int fill) {}
void default_drawPoint(int x, int y) {}
void default_refresh(void) {}
void default_setDrawingColor(long color) {}
void default_setMouse(int enable) {}
void default_setTextColor(long fg, long bg) {}
void default_setCursorPosition(int x, int y) {}
int default_cursorTextHeight(const char *str) {return 1;}
int default_cursorTextWidth(const char *str) {return 1;}

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
    os_graf_mx = opt_pref_width;
    os_graf_my = opt_pref_height;
    setsysvar_int(SYSVAR_XMAX, os_graf_mx);
    setsysvar_int(SYSVAR_YMAX, os_graf_my);
    devinit(prog_file, opt_pref_width, opt_pref_height);
  }

#if USE_TERM_IO || defined(_Win32)
  if (isatty(STDOUT_FILENO) && opt_vt100) {
    if(!devinit) {
      terminal_init();
      atexit(exit_handler);
      os_graphics = 1;
      terminal_getSize(&os_graf_mx, &os_graf_my);
      setsysvar_int(SYSVAR_XMAX, os_graf_mx);
      setsysvar_int(SYSVAR_YMAX, os_graf_my);
      dev_bgcolor = -1;
      dev_fgcolor = -1;
    }
    if (!p_arc) {
      p_arc = vt100_drawArc;
    };
    if (!p_audio) {
      p_audio = vt100_playAudio;
    };
    if (!p_beep) {
      p_beep = vt100_playBeep;
    };
    if (!p_clear_sound_queue) {
      p_clear_sound_queue = vt100_playClearSoundQueue;
    };
    if (!p_cls) {
      p_cls = vt100_clearScreen;
    };
    if (!p_ellipse) {
      p_ellipse = vt100_drawEllipse;
    };
    if (!p_events) {
      p_events = vt100_terminalEvents;
    };
    if (!p_getpen) {
      p_getpen = vt100_getMouse;
    };
    if (!p_getpixel) {
      p_getpixel = vt100_drawGetPixel;
    };
    if (!p_getx) {
      p_getx = vt100_cursorGetx;
    };
    if (!p_gety) {
      p_gety = vt100_cursorGety;
    };
    if (!p_line) {
      p_line = vt100_drawLine;
    };
    if (!p_rect) {
      p_rect = vt100_drawRect;
    };
    if (!p_refresh) {
      p_refresh = vt100_refresh;
    };
    if (!p_setcolor) {
      p_setcolor = vt100_setDrawingColor;
    };
    if (!p_setpenmode) {
      p_setpenmode = vt100_setMouse;
    };
    if (!p_setpixel) {
      p_setpixel = vt100_drawPoint;
    };
    if (!p_settextcolor) {
      p_settextcolor = vt100_setTextColor;
    };
    if (!p_setxy) {
      p_setxy = vt100_setCursorPosition;
    };
    if (!p_sound) {
      p_sound = vt100_playSound;
    };
    if (!p_textheight) {
      p_textheight = vt100_cursorTextHeight;
    };
    if (!p_textwidth) {
      p_textwidth = vt100_cursorTextWidth;
    };
    if (!p_write) {
      p_write = vt100_write;
    };
  } else {
#endif
    if(!devinit) {
      os_graphics = 0;
      os_graf_mx = 0;
      os_graf_my = 0;
      setsysvar_int(SYSVAR_XMAX, os_graf_mx);
      setsysvar_int(SYSVAR_YMAX, os_graf_my);
      dev_bgcolor = -1;
      dev_fgcolor = -1;
    }
    if (!p_arc) {
      p_arc = default_drawArc;
    };
    if (!p_audio) {
      p_audio = default_playAudio;
    };
    if (!p_beep) {
      p_beep = default_playBeep;
    };
    if (!p_clear_sound_queue) {
      p_clear_sound_queue = default_playClearSoundQueue;
    };
    if (!p_cls) {
      p_cls = default_clearScreen;
    };
    if (!p_ellipse) {
      p_ellipse = default_drawEllipse;
    };
    if (!p_events) {
      p_events = default_terminalEvents;
    };
    if (!p_getpen) {
      p_getpen = default_getMouse;
    };
    if (!p_getpixel) {
      p_getpixel = default_drawGetPixel;
    };
    if (!p_getx) {
      p_getx = default_cursorGetx;
    };
    if (!p_gety) {
      p_gety = default_cursorGety;
    };
    if (!p_line) {
      p_line = default_drawLine;
    };
    if (!p_rect) {
      p_rect = default_drawRect;
    };
    if (!p_refresh) {
      p_refresh = default_refresh;
    };
    if (!p_setcolor) {
      p_setcolor = default_setDrawingColor;
    };
    if (!p_setpenmode) {
      p_setpenmode = default_setMouse;
    };
    if (!p_setpixel) {
      p_setpixel = default_drawPoint;
    };
    if (!p_settextcolor) {
      p_settextcolor = default_setTextColor;
    };
    if (!p_setxy) {
      p_setxy = default_setCursorPosition;
    };
    if (!p_sound) {
      p_sound = default_playSound;
    };
    if (!p_textheight) {
      p_textheight = default_cursorTextHeight;
    };
    if (!p_textwidth) {
      p_textwidth = default_cursorTextWidth;
    };
    if (!p_write) {
      p_write = default_write;
    };
#if USE_TERM_IO || defined(_Win32)
  }
#endif
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
  p_settextcolor(fg, bg);
}

//
// enable or disable PEN/MOUSE driver
//
void osd_setpenmode(int enable) {
  p_setpenmode(enable);
}

//
// return pen/mouse info ('code' is the rq, see doc)
//
int osd_getpen(int code) {
  return p_getpen(code);
}

//
// clear screen
//
void osd_cls() {
  p_cls();
}

//
// returns the current x position (text-mode cursor)
//
int osd_getx() {
  return p_getx();
}

//
// returns the current y position (text-mode cursor)
//
int osd_gety() {
  return p_gety();
}

//
// set's text-mode (or graphics) cursor position
//
void osd_setxy(int y, int x) {
  p_setxy(x, y);
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

  result = p_events(wait_flag, &x, &y);

  if (x != os_graf_mx || y != os_graf_my) {
    dev_resize(x, y);
  }

  return result;
}

//
// sets color for drawing routines like LINE or RECT
//
void osd_setcolor(long color) {
  p_setcolor(color);

}

//
// draw a line
//
void osd_line(int x1, int y1, int x2, int y2) {
  p_line(x1, y1, x2, y2);
}

//
// draw an ellipse
//
void osd_ellipse(int xc, int yc, int xr, int yr, int fill) {
  p_ellipse(xc, yc, xr, yr, fill);
}

//
// draw an arc
//
void osd_arc(int xc, int yc, double r, double as, double ae, double aspect) {
  p_arc(xc, yc, r, as, ae, aspect);
}

//
// draw a pixel
//
void osd_setpixel(int x, int y) {
  p_setpixel(x, y);
}

//
// returns pixel's color
//
long osd_getpixel(int x, int y) {
  return p_getpixel(x, y);
}

//
// draw rectangle (parallelogram)
//
void osd_rect(int x1, int y1, int x2, int y2, int fill) {
  p_rect(x1, y1, x2, y2, fill);
}

//
// refresh/flush the screen/stdout
//
void osd_refresh() {
  p_refresh();
}

//
// just a beep
//
void osd_beep() {
  p_beep();
}

//
// play a sound
//
void osd_sound(int frq, int ms, int vol, int bgplay) {
  p_sound(frq, ms, vol, bgplay);
}

//
// clears sound-queue (stop background sound)
//
void osd_clear_sound_queue() {
  p_clear_sound_queue();
}

//
// play the given audio file
//
void osd_audio(const char *path) {
  p_audio(path);
}

//
// text-width in pixels
//
int osd_textwidth(const char *str) {
  return p_textwidth(str);
}

//
// text-height in pixels
//
int osd_textheight(const char *str) {
  return p_textheight(str);
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
    if (slept > timeout) {
      break;
    } else if (timeout - slept > WAIT_INTERVAL) {
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
