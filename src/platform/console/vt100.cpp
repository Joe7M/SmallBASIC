// This file is part of SmallBASIC
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//

#include "config.h"
#include "include/osd.h"
#include <stdio.h>
#include <stdlib.h>
#include "common/device.h"
#include "vt100.h"
#include "common/smbas.h"
#include "common/keymap.h"

#define swap(a, b) { int t = a; a = b; b = t; }
#define WAIT_INTERVAL 5
#define BUFFER_LENGTH 64

void terminal_getSize(int *x, int *y);

int mouseX = 0;
int mouseY = 0;
int mouseButton = 3;
int mouseLastButtonX = -1;
int mouseLastButtonY = -1;
int mouseEvent = 0;
long old_dev_bgcolor = -1;
char printBuffer[BUFFER_LENGTH];

const uint32_t color_map[] = {
  0x000000, // 0 black
  0x000080, // 1 blue
  0x008000, // 2 green
  0x008080, // 3 cyan
  0x800000, // 4 red
  0x800080, // 5 magenta
  0x808000, // 6 yellow
  0xC0C0C0, // 7 white
  0x808080, // 8 gray
  0x0000FF, // 9 light blue
  0x00FF00, // 10 light green
  0x00FFFF, // 11 light cyan
  0xFF0000, // 12 light red
  0xFF00FF, // 13 light magenta
  0xFFFF00, // 14 light yellow
  0xFFFFFF  // 15 bright white
};

long convertColor(long color) {
  // convert from internal format
  if (color < 0) {
    color = -color;
  } else if (color < 16) {
    color = color_map[color];
  }

  // Extract RGB (ignore alpha)
  int r = (color >> 16) & 0xFF;
  int g = (color >> 8) & 0xFF;
  int b = color & 0xFF;

  // Check if it's close to gray
  if (r == g && g == b) {
    if (r < 8) return 16;
    if (r > 248) return 231;

    // Map to grayscale (232–255)
    return 232 + (r - 8) / 10;
  }

  // Map RGB to 6x6x6 cube (values 0–5)
  int r6 = (r * 5) / 255;
  int g6 = (g * 5) / 255;
  int b6 = (b * 5) / 255;

  // Compute final index
  return 16 + (36 * r6) + (6 * g6) + b6;
}

#if USE_TERM_IO
void vt100_write(const char *str) {
  write(STDOUT_FILENO, str, strlen(str));
  fflush(stdout);
}

int getch() {
  char c;
  if (read(STDIN_FILENO, &c, 1) == 1) {
    return (int)c;
  }
  return EOF;
}

uint32_t vt100_inputReadKey(void) {
  char c = getch();

  if (c == EOF)  return 0;
  if (c == 127)  return SB_KEY_BACKSPACE;
  if (c != '\x1b') return c;

  /* Escape sequence */
  unsigned char seq[5];
  if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
  if (read(STDIN_FILENO, &seq[1], 1) != 1) return 0;

  /* SS3 sequences -> ESC 0 x1 */
  if (seq[0] == 'O') {
    switch (seq[1]) {
      case 'H': return SB_KEY_HOME;
      case 'F': return SB_KEY_END;
      case 'P': return SB_KEY_F(1);
      case 'Q': return SB_KEY_F(2);
      case 'R': return SB_KEY_F(3);
      case 'S': return SB_KEY_F(4);
    }
    return 0;
  }

  /* CSI and SGR sequences
   * - ESC [ x1 x2 x3 x4
   * - ESC [  < Cb  ; Cx ; Cy m
  */
  if (seq[0] != '[') return 0;

  if (seq[1] >= '0' && seq[1] <= '9') {
    /* 3- or 4-byte CSI sequence */
    if (read(STDIN_FILENO, &seq[2], 1) != 1) return 0;

    if (seq[2] == '~') {
      switch (seq[1]) {
        case '1': return SB_KEY_HOME;
        case '2': return SB_KEY_INSERT;
        case '3': return SB_KEY_DELETE;
        case '4': return SB_KEY_END;
        case '5': return SB_KEY_PGUP;
        case '6': return SB_KEY_PGDN;
        case '7': return SB_KEY_HOME;
        case '8': return SB_KEY_END;
      }
    } else if (seq[2] == ';') {
      // 5-byte sequence CTRL + arrow keys
      if (read(STDIN_FILENO, &seq[3], 1) != 1) return 0;
      if (read(STDIN_FILENO, &seq[4], 1) != 1) return 0;
      if (seq[3] == '5') {
        switch (seq[4]) {
          case 'A': return SB_KEY_CTRL(SB_KEY_UP);
          case 'B': return SB_KEY_CTRL(SB_KEY_DOWN);
          case 'D': return SB_KEY_CTRL(SB_KEY_LEFT);
          case 'C': return SB_KEY_CTRL(SB_KEY_RIGHT);
        }
      }
    } else {
      /* 4-byte CSI sequence (function keys) */
      if (read(STDIN_FILENO, &seq[3], 1) != 1) return 0;
      if (seq[3] != '~') return 0;

      switch (seq[1]) {
        case '1':
          switch (seq[2]) {
            case '5': return SB_KEY_F(5);
            case '7': return SB_KEY_F(6);
            case '8': return SB_KEY_F(7);
            case '9': return SB_KEY_F(8);
          }
          break;
        case '2':
          switch (seq[2]) {
            case '0': return SB_KEY_F(9);
            case '1': return SB_KEY_F(10);
            case '2': return SB_KEY_F(11);
            case '4': return SB_KEY_F(12);
          }
      }
    }
  } else if (seq[1] == '<') {
    // SGR 1006 for extended mouse position (>223 in x and y) and buttons
    // Sequence if button is pressed:  ESC [ < Cb ; Cx ; Cy m
    // Sequence if button is released: ESC [ < Cb ; Cx ; Cy M
    // Cb, Cx and Cy are one or multiple bytes long. Each byte is an ASCII integer ('0' to '9').
    int mouseButtonNew = 0;
    mouseX = 0;
    mouseY = 0;
    mouseEvent = 1;

    // Mouse button
    while (1) {
      if (read(STDIN_FILENO, &seq[2], 1) != 1) return 0;
      if (seq[2] == ';') break;
      mouseButtonNew = 10*mouseButtonNew + (seq[2] - 48);
    }

    // x position
    while (1) {
      if (read(STDIN_FILENO, &seq[2], 1) != 1) return 0;
      if (seq[2] == ';') break;
      mouseX = 10*mouseX + (seq[2] - 48);
    }

    // y position
    while (1) {
      if (read(STDIN_FILENO, &seq[2], 1) != 1) return 0;
      if (seq[2] == 'm' || seq[2] == 'M') break;
      mouseY = 10*mouseY + (seq[2] - 48);
    }

    // button is pressed
    if (seq[2] == 'M') {
      // remove modifier keys and motion indicator
      mouseButtonNew = mouseButtonNew & 11;
      if (mouseButtonNew < 3 && mouseButtonNew != mouseButton) {
        mouseLastButtonX = mouseX;
        mouseLastButtonY = mouseY;
        mouseButton = mouseButtonNew;
      }
    } else {
      // button was released
      mouseButton = 3;
    }
  } else {
    /* 2-byte CSI sequence */
    switch (seq[1]) {
      case 'A': return SB_KEY_UP;
      case 'B': return SB_KEY_DOWN;
      case 'C': return SB_KEY_RIGHT;
      case 'D': return SB_KEY_LEFT;
      case 'H': return SB_KEY_HOME;
      case 'F': return SB_KEY_END;
    }
  }
  return 0;
}

int vt100_getCursorPosition(int *rows, int *cols) {
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

void vt100_refresh(void) {
  osd_events(0);
  fflush(stdout);
}

#elif defined (_Win32)
long unsigned int numberEvents;
INPUT_RECORD inputRecord[16];
extern HANDLE hIn;
extern HANDLE hOut;

void vt100_write(const char *str) {
  WriteConsole(hOut, str, strlen(str), NULL, NULL);
}

uint32_t vt100_inputReadKey(void) {
  PeekConsoleInput(hIn, inputRecord, 16, &numberEvents);
  if (numberEvents == 0) return 0;
  FlushConsoleInputBuffer(hIn);

  // normal key (1,2,3...A,B,C...+,#,..)
  // Sequence
  // MSYS2 Mingw64 terminal -> x x
  // CMD and Windows terminal -> x
  if (numberEvents <= 2) {
    if (inputRecord[0].Event.KeyEvent.uChar.AsciiChar == 127) return SB_KEY_BACKSPACE;
    return inputRecord[0].Event.KeyEvent.uChar.AsciiChar;
  }

  // shift + normal key (1,2,3...A,B,C...+,#,..)  -> Sequence: 0 x1 x2 0
  if (numberEvents == 4) {
    if (inputRecord[0].Event.KeyEvent.uChar.AsciiChar == 0) {
      if (inputRecord[3].Event.KeyEvent.uChar.AsciiChar == 0) {
        if (inputRecord[1].Event.KeyEvent.uChar.AsciiChar == inputRecord[2].Event.KeyEvent.uChar.AsciiChar) return inputRecord[1].Event.KeyEvent.uChar.AsciiChar;
      }
      return 0;
    }
  }

  // Escape sequences
  if (inputRecord[0].Event.KeyEvent.uChar.AsciiChar != '\x1b') return 0;

  /* SS3 sequences -> Sequence: ESC 0 x2 */
  if (numberEvents == 3) {
    if (inputRecord[1].Event.KeyEvent.uChar.AsciiChar == 'O') {
      switch (inputRecord[2].Event.KeyEvent.uChar.AsciiChar) {
        case 'H': return SB_KEY_HOME;
        case 'F': return SB_KEY_END;
        case 'P': return SB_KEY_F(1);
        case 'Q': return SB_KEY_F(2);
        case 'R': return SB_KEY_F(3);
        case 'S': return SB_KEY_F(4);
      }
      return 0;
    }
  }

  /* CSI and SGR sequences
   * - ESC [ x2 x3 x4 x5
   * - ESC [  < Cb  ; Cx ; Cy m
   * */
  if (inputRecord[1].Event.KeyEvent.uChar.AsciiChar != '[') return 0;

  if (numberEvents == 3) {
    switch (inputRecord[2].Event.KeyEvent.uChar.AsciiChar) {
      case 'A': return SB_KEY_UP;
      case 'B': return SB_KEY_DOWN;
      case 'C': return SB_KEY_RIGHT;
      case 'D': return SB_KEY_LEFT;
      case 'H': return SB_KEY_HOME;
      case 'F': return SB_KEY_END;
    }
    return 0;
  }

  if (numberEvents == 4) {
    if (inputRecord[2].Event.KeyEvent.uChar.AsciiChar >= '0' && inputRecord[2].Event.KeyEvent.uChar.AsciiChar < '9') {
      if (inputRecord[3].Event.KeyEvent.uChar.AsciiChar == '~') {
        switch (inputRecord[2].Event.KeyEvent.uChar.AsciiChar) {
          case '1': return SB_KEY_HOME;
          case '2': return SB_KEY_INSERT;
          case '3': return SB_KEY_DELETE;
          case '4': return SB_KEY_END;
          case '5': return SB_KEY_PGUP;
          case '6': return SB_KEY_PGDN;
          case '7': return SB_KEY_HOME;
          case '8': return SB_KEY_END;
        }
      }
    }
    return 0;
  }

  if (numberEvents == 5) {
    if (inputRecord[2].Event.KeyEvent.uChar.AsciiChar == '1') {
      if (inputRecord[4].Event.KeyEvent.uChar.AsciiChar == '~') {
        switch (inputRecord[3].Event.KeyEvent.uChar.AsciiChar) {
          case '5': return SB_KEY_F(5);
          case '7': return SB_KEY_F(6);
          case '8': return SB_KEY_F(7);
          case '9': return SB_KEY_F(8);
        }
      }
    } else if (inputRecord[2].Event.KeyEvent.uChar.AsciiChar == '2') {
        if (inputRecord[4].Event.KeyEvent.uChar.AsciiChar == '~') {
          switch (inputRecord[3].Event.KeyEvent.uChar.AsciiChar) {
            case '0': return SB_KEY_F(9);
            case '1': return SB_KEY_F(10);
            case '3': return SB_KEY_F(11);
            case '4': return SB_KEY_F(12);
          }
        }
    }
    return 0;
  }

  if (numberEvents == 6) {
    if (inputRecord[2].Event.KeyEvent.uChar.AsciiChar == '1') {
      if (inputRecord[3].Event.KeyEvent.uChar.AsciiChar == ';') {
        if (inputRecord[4].Event.KeyEvent.uChar.AsciiChar == '5') {
          switch (inputRecord[5].Event.KeyEvent.uChar.AsciiChar) {
            case 'A': return SB_KEY_CTRL(SB_KEY_UP);
            case 'B': return SB_KEY_CTRL(SB_KEY_DOWN);
            case 'D': return SB_KEY_CTRL(SB_KEY_LEFT);
            case 'C': return SB_KEY_CTRL(SB_KEY_RIGHT);
          }
        }
      }
      return 0;
    }
  }

  // SGR 1006 for extended mouse position (>96 in x and y) and buttons
  // Sequence if button is pressed:  ESC [ < Cb ; Cx ; Cy m
  // Sequence if button is released: ESC [ < Cb ; Cx ; Cy M
  // Cb Cy and Cy are one or multiple bytes long. Each byte is an ASCII integer ('0' to '9').
  // 16 events allow [9999,9999] max mouse position.
  if (numberEvents > 8) {
    if (inputRecord[2].Event.KeyEvent.uChar.AsciiChar == '<') {
      DWORD ii = 3;
      int mouseButtonNew = 0;
      mouseX = 0;
      mouseY = 0;
      mouseEvent = 1;

      // Mouse button
      while (ii < numberEvents ) {
        if (inputRecord[ii].Event.KeyEvent.uChar.AsciiChar == ';') break;
        mouseButtonNew = 10*mouseButtonNew + (inputRecord[ii].Event.KeyEvent.uChar.AsciiChar - 48);
        ii++;
      }

      // x position
      ii++;
      while (ii < numberEvents) {
        if (inputRecord[ii].Event.KeyEvent.uChar.AsciiChar == ';') break;
        mouseX = 10*mouseX + (inputRecord[ii].Event.KeyEvent.uChar.AsciiChar - 48);
        ii++;
      }

      // y position
      ii++;
      while (ii < numberEvents) {
        if (inputRecord[ii].Event.KeyEvent.uChar.AsciiChar == 'm' || inputRecord[ii].Event.KeyEvent.uChar.AsciiChar == 'M') break;
        mouseY = 10*mouseY + (inputRecord[ii].Event.KeyEvent.uChar.AsciiChar - 48);
        ii++;
      }

      // button is pressed
      if (inputRecord[ii].Event.KeyEvent.uChar.AsciiChar == 'M') {
        // remove modifier keys and motion indicator
        mouseButtonNew = mouseButtonNew & 11;
        if (mouseButtonNew < 3 && mouseButtonNew != mouseButton) {
          mouseLastButtonX = mouseX;
          mouseLastButtonY = mouseY;
          mouseButton = mouseButtonNew;
        }
      } else {
        // button was released
        mouseButton = 3;
      }
    }
  }

  return 0;
}

int vt100_getCursorPosition(int *rows, int *cols) {
  if (GetConsoleScreenBufferInfo(hOut, &screenBufferInfo)) {
    *cols = screenBufferInfo.dwCursorPosition.X + 1;
    *rows = screenBufferInfo.dwCursorPosition.Y + 1;
  } else {
    *cols = 0;
    *rows = 0;
  }
  return 0;
}

void vt100_refresh() {
  osd_events(0);
}
#endif

void readKey(void) {
  uint32_t c = vt100_inputReadKey();
  if (c > 0) {
    dev_clrkb();
    dev_pushkey(c);
  }
}

void vt100_setMouse(int enable) {
  if (enable) {
    vt100_write("\x1b[?1003h");    // enable "All Motion Mouse Tracking"
    vt100_write("\x1b[?1006h");    // enable SGR extended mouse mode
  } else {
    vt100_write("\x1b[?1003l");    // disable "All Motion Mouse Tracking"
    vt100_write("\x1b[?1006l");    // enable SGR extended mouse mode
  }
}

int vt100_getMouse(int code) {
  switch(code) {
    case 0:                 // new mouse event
      if (mouseEvent && (mouseButton & 3) == 0) {
        return (1);
      }
      mouseEvent = 0;
      return (0);
    case 1:                 // last mouse button down x
      return mouseLastButtonX;
    case 2:                 // last mouse button down y
      return mouseLastButtonY;
    case 3:                 // True if mouse left button is pressed
      return ((mouseButton & 3) == 0);
    case 4:                 // last mouse x if left button is pressed
      if ((mouseButton & 3) == 0) {
        return (mouseX);
      }
      return -1;
    case 5:                 //last mouse y if left button is pressed
      if  ((mouseButton & 3) == 0) {
        return (mouseY);
      }
      return -1;
    case 10:                // current mouse x position
      return mouseX;
    case 11:                // current mouse y pos
      return mouseY;
    case 12:                // true if the left mouse button is pressed
      return ((mouseButton & 3) == 0);
    case 13:                // true if the right mouse button is pressed
      return ((mouseButton & 3) == 2);
    case 14:                // true if the middle mouse button is pressed
      return ((mouseButton & 3) == 1);
  }
  return (0);
}

void vt100_setTextColor(long fg, long bg) {
   // VT100: 8bit color mode
  if (fg > 15) {
    // if color  > 15 use index to address color
    fg = fg & 0xFF;
  } else {
    // convert color to RGB
    fg = convertColor(fg);
  }

  // Prevent setting bgcolor if COLOR is called with one parameter
  if (bg != old_dev_bgcolor) {
    old_dev_bgcolor = bg;
    if (bg > 15) {
      bg = bg & 0xFF;
    } else {
      bg = convertColor(bg);
    }
    snprintf(printBuffer, BUFFER_LENGTH, "\033[38;5;%ldm\033[48;5;%ldm", fg, bg);
    vt100_write(printBuffer);
  } else {
    snprintf(printBuffer, BUFFER_LENGTH, "\033[38;5;%ldm", fg);
    vt100_write(printBuffer);
  }
}

void vt100_moveCursorRight(int numCharacters) {
  snprintf(printBuffer, BUFFER_LENGTH, "\x1b[%dC", numCharacters);
  vt100_write(printBuffer);
}

void vt100_moveCursorLeft(int numCharacters) {
  snprintf(printBuffer, BUFFER_LENGTH, "\x1b[%dD", numCharacters);
  vt100_write(printBuffer);
}

void vt100_setCursorPosition(int row, int col) {
  snprintf(printBuffer, BUFFER_LENGTH, "\033[%d;%dH", row, col);
  vt100_write(printBuffer);
}

int vt100_cursorGetx(void) {
  int rows = 0, cols = 0;
  vt100_getCursorPosition(&rows, &cols);
  return cols;
}

int vt100_cursorGety(void) {
  int rows = 0, cols = 0;
  vt100_getCursorPosition(&rows, &cols);
  return rows;
}

int vt100_cursorTextHeight(const char *str) {
  return 1;
}

int vt100_cursorTextWidth(const char *str) {
  return 1;
}

int vt100_terminalEvents(int wait_flag, int *x, int *y) {
  if (wait_flag) {
    usleep(WAIT_INTERVAL * 1000);
  }

  terminal_getSize(x, y);         // XMAX, YMAX
  readKey();                      // INKEY

  return 0;
}

void vt100_clearScreen(void) {
  // VT100: Move cursor to 1,1 and clear screen from cursor
  vt100_write("\033[H\033[J");
}

void vt100_playBeep(void) {
  vt100_write("\a");
};

long vt100_drawGetPixel(int x, int y) {
  return -1;
}

void vt100_setDrawingColor(long color) {
  dev_fgcolor = color;
  vt100_setTextColor(dev_fgcolor, dev_bgcolor);
}

void vt100_drawPoint(int x, int y) {
  vt100_setTextColor(dev_bgcolor, dev_fgcolor);
  vt100_setCursorPosition(y, x);
  vt100_write(" ");
  vt100_setTextColor(dev_fgcolor, dev_bgcolor);
}

void fastHLine(int x, int y, int l) {
  vt100_setCursorPosition(y, x);
  for (int ii = 0; ii < l; ii++) {
    vt100_write(" ");
  }
}

void fastVLine(int x, int y, int h) {
  for (int ii = y; ii < y + h; ii++) {
    vt100_setCursorPosition(ii, x);
    vt100_write(" ");
  }
}

void vt100_drawLine(int x0, int y0, int x1, int y1) {
  int steep;
  int ystep;
  int dx, dy;
  int err;

  vt100_setTextColor(dev_bgcolor, dev_fgcolor);

  if (x0 == x1) {
    if (y0 > y1) {
      swap(y0, y1);
    }
    fastVLine(x0, y0, y1 - y0 + 1);
    vt100_setTextColor(dev_fgcolor, dev_bgcolor);
    return;
  }
  if (y0 == y1) {
    if (x0 > x1) {
      swap(x0, x1);
    }
    fastHLine(x0, y0, x1 - x0 + 1);
    vt100_setTextColor(dev_fgcolor, dev_bgcolor);
    return;
  }

  steep = abs(y1 - y0) > abs(x1 - x0);

  if (steep) {
    swap(x0, y0);
    swap(x1, y1);
  }
  if (x0 > x1) {
    swap(x0, x1);
    swap(y0, y1);
  }

  dx = x1 - x0;
  dy = abs(y1 - y0);
  err = dx / 2;

  if (y0 < y1) {
      ystep = 1;
  } else {
      ystep = -1;
  }

  if (steep) {
    for (; x0 <= x1; x0++) {
      vt100_setCursorPosition(x0, y0);
      vt100_write(" ");
      err -= dy;
      if (err < 0) {
        y0  += ystep;
        err += dx;
      }
    }
  } else {
    for (; x0 <= x1; x0++) {
      vt100_setCursorPosition(y0, x0);
      vt100_write(" ");
      err -= dy;
      if (err < 0) {
        y0  += ystep;
        err += dx;
      }
    }
  }

  vt100_setTextColor(dev_fgcolor, dev_bgcolor);
}

void vt100_drawRect(int x1, int y1, int x2, int y2, int fill) {
  vt100_setTextColor(dev_bgcolor, dev_fgcolor);

  if (x2 < x1) {
    swap(x1, x2);
  }
  if (y2 < y1) {
    swap(x2, x1);
  }

  int w = x2 - x1;
  int h = y2 - y1;

  if (fill) {
    for (int i = y1; i < y1 + h + 1; i++)
    {
      fastHLine(x1, i, w);
    }
  } else {
    fastHLine(x1, y1, w);
    fastHLine(x1, y1 + h, w);
    fastVLine(x1, y1, h);
    fastVLine(x1 + w - 1, y1, h);
  }

  vt100_setTextColor(dev_fgcolor, dev_bgcolor);
}

void vt100_printInline(int x, int y, char *dest) {
  vt100_write("\x1b[s");          // Save cursor
  vt100_setCursorPosition(y, x);
  vt100_write("\x1b[K");          // Delete from cursor to end of line
  vt100_write(dest);
  vt100_write("\x1b[u");          // Restore cursor
}

void vt100_drawArc(int xc, int yc, double r, double as, double ae, double aspect) {};
void vt100_drawEllipse(int xc, int yc, int xr, int yr, int fill) {};
void vt100_playAudio(const char *path) {};
void vt100_playSound(int frq, int ms, int vol, int bgplay) {};
void vt100_playClearSoundQueue(void) {};
