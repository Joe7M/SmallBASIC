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
#include "input.h"
#include "input_history.h"

int mouseX = 0;
int mouseY = 0;
int mouseButton = 3;
int mouseLastButtonX = -1;
int mouseLastButtonY = -1;
int mouseEvent = 0;

InputHistory history;

#if defined(USE_TERM_IO)
// Structure to hold original terminal settings
struct termios original_termios;

/**
 * @brief Sets the terminal to non-canonical (raw) mode.
 */
void input_init() {
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
  //raw.c_oflag &= ~(OPOST);   // turns off post processing \n -> \n\r

  // 4. Set timeout and minimum number of bytes for read()
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 0;

  // 5. Apply the new settings
  tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

/**
 * @brief Restores the terminal to its original settings.
 */
void input_close() {
  tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
}

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

uint32_t terminalReadKey(void) {
  char c = getch();

  if (c == EOF)  return 0;
  if (c == 127)  return SB_KEY_BACKSPACE;
  if (c != '\x1b') return c;

  /* Escape sequence */
  unsigned char seq[4];
  if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
  if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

  if (seq[0] == '[') {
    if (seq[1] >= '0' && seq[1] <= '9') {
      /* 3- or 4-byte CSI sequence */
      if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';

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
      } else {
        /* 4-byte CSI sequence (function keys) */
        if (read(STDIN_FILENO, &seq[3], 1) != 1) return '\x1b';
        if (seq[3] != '~') return '\x1b';

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
    } else {
      /* 2-byte CSI sequence */
      switch (seq[1]) {
        case 'A': return SB_KEY_UP; 
        case 'B': return SB_KEY_DOWN;
        case 'C': return SB_KEY_RIGHT;
        case 'D': return SB_KEY_LEFT;
        case 'H': return SB_KEY_HOME;
        case 'F': return SB_KEY_END;
        case 'M': 
          /* X10 mouse event: button, x, y */
          if (read(STDIN_FILENO, &seq, 3) != 3) return '\x1b';
          mouseEvent = 1;
          mouseX = seq[1] - 32;
          mouseY = seq[2] - 32;
          if ((mouseButton & 3) != 0 && (seq[0] & 3) == 0) {
            mouseLastButtonX = mouseX;
            mouseLastButtonY = mouseY;
          }
          mouseButton = seq[0];
          return 0;
      }
    }
  } else if (seq[0] == 'O') {
    /* SS3 sequences */
    switch (seq[1]) {
      case 'H': return SB_KEY_HOME;
      case 'F': return SB_KEY_END;
      case 'P': return SB_KEY_F(1);
      case 'Q': return SB_KEY_F(2);
      case 'R': return SB_KEY_F(3);
      case 'S': return SB_KEY_F(4);
    }
  }
  return '\x1b';
}

long int getCharacter(void) {
  return terminalReadKey();
}

void setMouse(int enable) {
  if (enable) {
    printf("\x1b[?1003h");    // enable "All Motion Mouse Tracking"
  } else {
    printf("\x1b[?1003l");    // disable "All Motion Mouse Tracking"
  }
}

int getMouse(int code) {
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

#elif defined (_Win32)
void input_init() {
  set_terminal_raw();
}

void input_close() {
  reset_terminal();
}

uint32_t terminalReadKey(void) {
  return 0;
}

long int getCharacter(void) {
  return fgetc(stdin);
}

void setMouse(int enable) {}
int getMouse(int code) {
  return 0;
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

/**
 * gets a string (INPUT)
 */
char *dev_gets(char *dest, int size) {
  long int ch = 0;
  uint16_t pos, len = 0;
  int replace_mode = 0;
  int cursorX = 0;
  int cursorY = 0;

  getCursorPosition(&cursorY, &cursorX);

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
      setCursorPosition(cursorY, cursorX);
      break;
    case SB_KEY_END:
      pos = len;
      setCursorPosition(cursorY, cursorX + len);
      break;
    case SB_KEY_BACKSPACE:   // backspace
      if (pos > 0) {
        int old_pos = pos;
        pos -= dev_input_remove_char(dest, pos - 1);
        len = strlen(dest);
        printInline(cursorX, cursorY, dest);
        moveCursorLeft(old_pos - pos);
      } else {
        dev_beep();
      }
      break;
    case SB_KEY_DELETE:      // delete
      if (pos < len) {
        dev_input_remove_char(dest, pos);
        len = strlen(dest);
        printInline(cursorX, cursorY, dest);
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
        moveCursorLeft(old_pos - pos);
      } else {
        dev_beep();
      }
      break;
    case SB_KEY_RIGHT:
      if (pos < len) {
        int old_pos = pos;
        pos += dev_input_count_char((byte *)dest, pos);
        moveCursorRight(pos - old_pos);
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
    default:
      if ((ch & 0xFF00) != 0xFF00) {
        // Not an hardware key
        int old_pos = pos;
        pos += dev_input_insert_char(ch, dest, pos, replace_mode);
        printInline(cursorX, cursorY, dest);
        moveCursorRight(pos - old_pos);
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
#if USE_TERM_IO
  printf("\n");
#elif defined(_Win32)
  //TODO
#endif
  return dest;
}
