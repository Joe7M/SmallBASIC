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


#if USE_TERM_IO
void vt100_drawPoint(int x, int y) {
  vt100_setTextColor(dev_bgcolor, dev_fgcolor);
  vt100_setCursorPosition(y, x);
  putchar(' ');
  vt100_setTextColor(dev_fgcolor, dev_bgcolor);
}

void fastHLine(int x, int y, int l) {
  vt100_setCursorPosition(y, x);
  for (int ii = 0; ii < l; ii++) {
    putchar(' ');
  }
}

void fastVLine(int x, int y, int h) {
  for (int ii = y; ii < y + h; ii++) {
    vt100_setCursorPosition(ii, x);
    putchar(' ');
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
      putchar(' ');
      err -= dy;
      if (err < 0) {
        y0  += ystep;
        err += dx;
      }
    }
  } else {
    for (; x0 <= x1; x0++) {
      vt100_setCursorPosition(y0, x0);
      putchar(' ');
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

long vt100_drawGetPixel(int x, int y) {
  return -1;
}

void vt100_drawArc(int xc, int yc, double r, double as, double ae, double aspect) {};
void vt100_drawEllipse(int xc, int yc, int xr, int yr, int fill) {};

#elif defined (_Win32)
void vt100_drawPoint(int x, int y) {}
void vt100_drawLine(int x0, int y0, int x1, int y1) {}
void vt100_drawRect(int x0, int y0, int x1, int y1, int fill) {}
void vt100_drawArc(int xc, int yc, double r, double as, double ae, double aspect) {};
void vt100_drawEllipse(int xc, int yc, int xr, int yr, int fill) {};
long vt100_drawGetPixel(int x, int y) {};
#endif

