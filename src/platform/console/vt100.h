// This file is part of SmallBASIC
//
// lowlevel terminal I/O
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//

#ifndef VT100_H
#define VT100_H

#include "config.h"
#include <stdint.h>

void vt100_write(const char *str);
void vt100_clearScreen(void);
void vt100_setTextColor(long fg, long bg);
void vt100_playBeep(void);
void vt100_refresh(void);
void vt100_playAudio(const char *path);
void vt100_playSound(int frq, int ms, int vol, int bgplay);
void vt100_playClearSoundQueue(void);
int vt100_cursorGetx(void);
int vt100_cursorGety(void);
int vt100_cursorTextHeight(const char *str);
int vt100_cursorTextWidth(const char *str);
int vt100_terminalEvents(int wait_flag, int *x, int *y);
uint32_t vt100_inputReadKey(void);
void vt100_setMouse(int enable);
int vt100_getCursorPosition(int *rows, int *cols);
void vt100_setCursorPosition(int row, int col);
void vt100_moveCursorRight(int numCharacters);
void vt100_moveCursorLeft(int numCharacters);
void vt100_printInline(int x, int y, char *dest) ;
int vt100_getMouse(int code);
void readKey(void);
void vt100_drawPoint(int x, int y);
void vt100_drawLine(int x0, int y0, int x1, int y1);
void vt100_drawRect(int x1, int y1, int x2, int y2, int fill);
void vt100_drawArc(int xc, int yc, double r, double as, double ae, double aspect);
void vt100_drawEllipse(int xc, int yc, int xr, int yr, int fill);
long vt100_drawGetPixel(int x, int y);
void vt100_setDrawingColor(long color);

#endif /* VT100_H */
