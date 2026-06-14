// This file is part of SmallBASIC
//
// Copyright(C) 2001-2018 Chris Warren-Smith.
// Copyright(C) 2000 Nicholas Christopoulos
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//

#include "config.h"
#include "common/device.h"
#include "common/smbas.h"
#include "common/keymap.h"
#include <stdio.h>
#include "terminal.h"
#include "input_history.h"
#if defined (_Win32)
#include "wincontypes.h"
#endif

int mouseX = 0;
int mouseY = 0;
int mouseButton = 3;
int mouseLastButtonX = -1;
int mouseLastButtonY = -1;
int mouseEvent = 0;

InputHistory history;

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

void readKey(void) {
  uint32_t c = vt100_inputReadKey();
  if (c > 0) {
    dev_clrkb();
    dev_pushkey(c);
  }
}

long int getCharacter(void) {
  return vt100_inputReadKey();
}

#if USE_TERM_IO
/**
 * @brief Reads a single character from stdin in raw mode.
 * @return The character read, or EOF if an error occurs.
 */
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
      /*case 'M':
      *  // X10 mouse event: button, x, y
      *  if (read(STDIN_FILENO, &seq, 3) != 3) return 0;
      *  mouseEvent = 1;
      *  mouseX = seq[1] - 32;
      *  mouseY = seq[2] - 32;
      *  if ((mouseButton & 3) != 0 && (seq[0] & 3) == 0) {
      *    mouseLastButtonX = mouseX;
      *    mouseLastButtonY = mouseY;
      *  }
      *  mouseButton = seq[0];
      *  return 0;
      */
    }
  }
  return 0;
}

void vt100_setMouse(int enable) {
  if (enable) {
    printf("\x1b[?1003h");    // enable "All Motion Mouse Tracking"
    printf("\x1b[?1006h");    // enable SGR extended mouse mode
  } else {
    printf("\x1b[?1003l");    // disable "All Motion Mouse Tracking"
    printf("\x1b[?1006l");    // enable SGR extended mouse mode
  }
}

#elif defined (_Win32)
long unsigned int numberEvents;
INPUT_RECORD inputRecord[16];
extern HANDLE hIn;
extern HANDLE hOut;

uint32_t vt100_inputReadKey(void) {
  PeekConsoleInput(hIn, inputRecord, 16, &numberEvents);
  if (numberEvents == 0) return 0;
  FlushConsoleInputBuffer(hIn);

  /*
  printf("#:%ld|0:%d|1:%c|2:%c|3:%c|4:%c|5:%c|6:%c|7:%c|8:%c|9:%c|10:%c|11:%c|12:%c|13:%c|14:%c|15:%c|\n",
    numberEvents,
    inputRecord[0].Event.KeyEvent.uChar.AsciiChar,
    inputRecord[1].Event.KeyEvent.uChar.AsciiChar,
    inputRecord[2].Event.KeyEvent.uChar.AsciiChar,
    inputRecord[3].Event.KeyEvent.uChar.AsciiChar,
    inputRecord[4].Event.KeyEvent.uChar.AsciiChar,
    inputRecord[5].Event.KeyEvent.uChar.AsciiChar,
    inputRecord[6].Event.KeyEvent.uChar.AsciiChar,
    inputRecord[7].Event.KeyEvent.uChar.AsciiChar,
    inputRecord[8].Event.KeyEvent.uChar.AsciiChar,
    inputRecord[9].Event.KeyEvent.uChar.AsciiChar,
    inputRecord[10].Event.KeyEvent.uChar.AsciiChar,
    inputRecord[11].Event.KeyEvent.uChar.AsciiChar,
    inputRecord[12].Event.KeyEvent.uChar.AsciiChar,
    inputRecord[13].Event.KeyEvent.uChar.AsciiChar,
    inputRecord[14].Event.KeyEvent.uChar.AsciiChar,
    inputRecord[15].Event.KeyEvent.uChar.AsciiChar
  );
  */

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

void vt100_setMouse(int enable) {
  if (enable) {
    WriteConsole(hOut, "\x1b[?1003h", 8, NULL, NULL);    // enable "All Motion Mouse Tracking"
    WriteConsole(hOut, "\x1b[?1006h", 8, NULL, NULL);    // enable SGR extended mouse mode
  } else {
    WriteConsole(hOut, "\x1b[?1003l", 8, NULL, NULL);    // disable "All Motion Mouse Tracking"
    WriteConsole(hOut, "\x1b[?1006l", 8, NULL, NULL);    // disable SGR extended mouse mode
  }
}

#else
uint32_t terminalReadKey(void) {
  return 0;
}

long int getCharacter(void) {
  return fgetc(stdin);
}

int getMouse(int code) {
  return 0;
}
#endif



/**
 * return the character (multibyte charsets support)
 */
int dev_input_char2str(int ch, byte * cstr) {
  memset(cstr, 0, 3);

  switch (os_charset) {
  case enc_sjis:               // Japan
    if (IsJISFont(ch)) {
      cstr[0] = ch >> 8;
    }
    cstr[1] = ch & 0xFF;
    break;
  case enc_big5:               // China/Taiwan (there is another charset for
    // China but I don't know the difference)
    if (IsBig5Font(ch)) {
      cstr[0] = ch >> 8;
    }
    cstr[1] = ch & 0xFF;
    break;
  case enc_gmb:                // Generic multibyte
    if (IsGMBFont(ch)) {
      cstr[0] = ch >> 8;
    }
    cstr[1] = ch & 0xFF;
    break;
  case enc_unicode:            // Unicode
    cstr[0] = ch >> 8;
    cstr[1] = ch & 0xFF;
    break;
  default:
    cstr[0] = ch;
  };
  return strlen((char *)cstr);
}

/**
 * return the character size at pos! (multibyte charsets support)
 */
int dev_input_count_char(byte *buf, int pos) {
  int count, ch;
  byte cstr[3];

  if (os_charset != enc_utf8) {
    ch = buf[0];
    ch = ch << 8;
    ch = ch + buf[pos];
    count = dev_input_char2str(ch, cstr);
  } else {
    count = 1;
  }
  return count;
}

/**
 * stores a character at 'pos' position
 */
int dev_input_insert_char(int ch, char *dest, int pos, int replace_mode) {
  byte cstr[3];
  int count, remain;
  char *buf;

  count = dev_input_char2str(ch, cstr);

  // store character into buffer
  if (replace_mode) {
    // overwrite mode
    remain = strlen((dest + pos));
    buf = (char *)malloc(remain + 1);
    strcpy(buf, dest + pos);
    memcpy(dest + pos, cstr, count);
    dest[pos + count] = '\0';

    if (os_charset != enc_utf8) {
      count = dev_input_char2str(((buf[0] << 8) | buf[1]), cstr);
    } else {
      count = 1;
    }
    if (buf[0]) {                // not a '\0'
      strcat(dest, (buf + count));
    }
    free(buf);
  } else {
    // insert mode
    remain = strlen((dest + pos));
    buf = (char *)malloc(remain + 1);
    strcpy(buf, dest + pos);
    memcpy(dest + pos, cstr, count);
    dest[pos + count] = '\0';
    strcat(dest, buf);
    free(buf);
  }

  return count;
}

/**
 * removes the character at 'pos' position
 */
int dev_input_remove_char(char *dest, int pos) {
  byte cstr[3];
  int count, remain;
  char *buf;

  if (dest[pos]) {
    if (os_charset != enc_utf8) {
      count = dev_input_char2str(((dest[pos] << 8) | dest[pos + 1]), cstr);
    } else {
      count = 1;
    }
    remain = strlen((dest + pos + 1));
    buf = (char *)malloc(remain + 1);
    strcpy(buf, dest + pos + count);

    dest[pos] = '\0';
    strcat(dest, buf);
    free(buf);
    return count;
  }
  return 0;
}



#if USE_TERM_IO || defined (_Win32)
/**
 * gets a string (INPUT)
 */
char *dev_gets(char *dest, int size) {
  long int ch = 0;
  uint16_t pos, len = 0;
  int replace_mode = 0;
  int cursorX = 0;
  int cursorY = 0;

  vt100_getCursorPosition(&cursorY, &cursorX);

  *dest = '\0';
  pos = 0;
  do {
    len = strlen(dest);
    ch = getCharacter();
    switch (ch) {
    case -1:
    case -2:
    case 0xFFFF:
      dest[pos] = '\0';
      return dest;
    case 0:
    case 10:
    case 13:                 // ignore
      break;
    case SB_KEY_HOME:
      pos = 0;
      vt100_setCursorPosition(cursorY, cursorX);
      break;
    case SB_KEY_END:
      pos = len;
      vt100_setCursorPosition(cursorY, cursorX + len);
      break;
    case SB_KEY_BACKSPACE:   // backspace
      if (pos > 0) {
        int old_pos = pos;
        pos -= dev_input_remove_char(dest, pos - 1);
        len = strlen(dest);
        vt100_printInline(cursorX, cursorY, dest);
        vt100_moveCursorLeft(old_pos - pos);
      } else {
        dev_beep();
      }
      break;
    case SB_KEY_DELETE:      // delete
      if (pos < len) {
        dev_input_remove_char(dest, pos);
        len = strlen(dest);
        vt100_printInline(cursorX, cursorY, dest);
      } else
        dev_beep();
      break;
    case SB_KEY_INSERT:
      replace_mode = !replace_mode;
      break;
    case SB_KEY_LEFT:
      if (pos > 0) {
        int old_pos = pos;
        pos -= dev_input_count_char((byte *)dest, pos);
        vt100_moveCursorLeft(old_pos - pos);
      } else {
        dev_beep();
      }
      break;
    case SB_KEY_RIGHT:
      if (pos < len) {
        int old_pos = pos;
        pos += dev_input_count_char((byte *)dest, pos);
        vt100_moveCursorRight(pos - old_pos);
      } else {
        dev_beep();
      }
      break;
    case SB_KEY_UP:
      if (history.up(dest, size)) {
        pos = len = strlen(dest);
      }
      break;
    case SB_KEY_DOWN:
      if (history.down(dest, size)) {
        pos = len = strlen(dest);
      }
      break;
    case SB_KEY_CTRL(SB_KEY_LEFT):
      // find previous word
      if (pos > 0) {
        int old_pos = pos;
        while (pos > 0) {
          pos--;
          if (dest[pos] == ' ' && dest[pos - 1] != ' ') {
            break;
          }
        }
        vt100_moveCursorLeft(old_pos - pos);
      }
      break;
    case SB_KEY_CTRL(SB_KEY_RIGHT):
      // find next word
      if (pos < len) {
        int old_pos = pos;
        while (pos < len) {
          pos++;
          if (dest[pos] == ' ' && dest[pos + 1] != ' ') {
            pos++;
            break;
          }
        }
        vt100_moveCursorRight(pos - old_pos);
      }
      break;
    default:
      if ((ch & 0xFF00) != 0xFF00) {
        // Not an hardware key
        int old_pos = pos;
        pos += dev_input_insert_char(ch, dest, pos, replace_mode);
        vt100_printInline(cursorX, cursorY, dest);
        vt100_moveCursorRight(pos - old_pos);
      } else {
        ch = 0;
      }
      // check the size
      len = strlen(dest);
      if (len >= (size - 2)) {
        break;
      }
    }
  } while (ch != '\n' && ch != '\r');

  dest[len] = '\0';
  history.push(dest);
  printf("\n");
  return dest;
}
#else
/**
 * gets a string (INPUT) without VT100
 */
char *dev_gets(char *dest, int size) {
  long int ch = 0;
  uint16_t pos, len = 0;
  int replace_mode = 0;

  *dest = '\0';
  pos = 0;
  do {
    len = strlen(dest);
    ch = fgetc(stdin);
    switch (ch) {
    case 0:
    case 10:
    case 13:                 // ignore
      break;
    default:
      if ((ch & 0xFF00) != 0xFF00) { // Not an hardware key
        pos += dev_input_insert_char(ch, dest, pos, replace_mode);
      } else {
        ch = 0;
      }
      // check the size
      len = strlen(dest);
      if (len >= (size - 2)) {
        break;
      }
    }
  } while (ch != '\n' && ch != '\r');
  dest[len] = '\0';
  return dest;
}
#endif
