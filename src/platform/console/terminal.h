// This file is part of SmallBASIC
//
// lowlevel terminal I/O
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//

#ifndef TERMINAL_H
#define TERMINAL_H

#include "config.h"
#include <stdint.h>

uint32_t vt100_inputReadKey(void);
void vt100_drawPoint(int x, int y);
void vt100_drawLine(int x0, int y0, int x1, int y1);
void vt100_drawRect(int x1, int y1, int x2, int y2, int fill);
void vt100_drawArc(int xc, int yc, double r, double as, double ae, double aspect);
void vt100_drawEllipse(int xc, int yc, int xr, int yr, int fill);
int vt100_getCursorPosition(int *rows, int *cols);
void vt100_setCursorPosition(int x, int y);
void vt100_setTextColor(long fg, long bg);
void vt100_setMouse(int enable);
long vt100_drawGetPixel(int x, int y);
int vt100_getMouse(int code);
void vt100_moveCursorLeft(int numCharacters);
void vt100_moveCursorRight(int numCharacters);
void vt100_printInline(int x, int y, char *dest);
void readKey(void);
void input_init(void);


#endif /* TERMINAL_H */
