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
#include "input_history.h"
#include "vt100.h"
#if defined (_Win32)
#include "wincontypes.h"
#endif

InputHistory history;

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
    ch = vt100_inputReadKey();
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
