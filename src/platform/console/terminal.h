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

uint32_t terminalReadKey(void);
void drawPoint(int x, int y);
void drawLine(int x0, int y0, int x1, int y1);
void drawRect(int x1, int y1, int x2, int y2, int fill);
int getCursorPosition(int *rows, int *cols);
void setCursorPosition(int x, int y);
void setDrawingCharacter(long color);
void setTextColor(long fg, long bg);
void setMouse(int enable);
int getMouse(int code);

#endif /* TERMINAL_H */
