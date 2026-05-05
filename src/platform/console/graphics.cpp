// This file is part of SmallBASIC
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include "terminal.h"
#include "common/device.h"

#define swap(a, b) { int t = a; a = b; b = t; }

void point(int x, int y) {
  setTextColor(dev_bgcolor, dev_fgcolor);
  setCursorPosition(y, x);
  putchar(' ');
  setTextColor(dev_fgcolor, dev_bgcolor);
}

void fastHLine(int x, int y, int l) {
  setCursorPosition(y, x);
  for (int ii = 0; ii < l; ii++) {
    putchar(' ');
  }
}

void fastVLine(int x, int y, int h) {
  for (int ii = y; ii < y + h; ii++) {
    setCursorPosition(ii, x);
    putchar(' ');
  }
}

void line(int x0, int y0, int x1, int y1) {
  int steep;
  int ystep;
  int dx, dy;
  int err;

  setTextColor(dev_bgcolor, dev_fgcolor);

  if (x0 == x1) {
    if (y0 > y1) {
      swap(y0, y1);
    }
    fastVLine(x0, y0, y1 - y0 + 1);
    setTextColor(dev_fgcolor, dev_bgcolor);
    return;
  }
  if (y0 == y1) {
    if (x0 > x1) {
      swap(x0, x1);
    }
    fastHLine(x0, y0, x1 - x0 + 1);
    setTextColor(dev_fgcolor, dev_bgcolor);
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
      setCursorPosition(x0, y0);
      putchar(' ');
      err -= dy;
      if (err < 0) {
        y0  += ystep;
        err += dx;
      }
    }
  } else {
    for (; x0 <= x1; x0++) {
      setCursorPosition(y0, x0);
      putchar(' ');
      err -= dy;
      if (err < 0) {
        y0  += ystep;
        err += dx;
      }
    }
  }

  setTextColor(dev_fgcolor, dev_bgcolor);
}

void rect(int x1, int y1, int x2, int y2, int fill) {
  setTextColor(dev_bgcolor, dev_fgcolor);

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

  setTextColor(dev_fgcolor, dev_bgcolor);
}

#if USE_TERM_IO
void drawPoint(int x, int y) {
  point(x, y);
}

void drawLine(int x0, int y0, int x1 , int y1) {
  line(x0, y0, x1, y1);
}

void drawRect(int x0, int y0, int x1, int y1, int fill) {
  rect(x0, y0, x1, y1, fill); 
}
#elif defined (_Win32)
void drawPoint(int x, int y) {}
void drawLine(int x0, int y0, int x1, int y1) {}
void drawRect(int x0, int y0, int x1, int y1, int fill) {}
#else
void drawPoint(int x, int y) {}
void drawLine(int x0, int y0, int x1, int y1) {}
void drawRect(int x0, int y0, int x1, int y1, int fill) {}
#endif

