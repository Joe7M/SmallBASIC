// This file is part of SmallBASIC
//
// Copyright(C) 2001-2012 Chris Warren-Smith.
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//

#include <ma.h>
#include <MAUI/Engine.h>
#include <MAUI/Font.h>

#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "MAHeaders.h"

#include "platform/mosync/controller.h"
#include "platform/mosync/utils.h"

Controller::Controller() : MAUtil::Environment() {
  MAExtent screenSize = maGetScrSize();
  output = new AnsiWidget(EXTENT_X(screenSize), EXTENT_Y(screenSize));
  output->construct();

  // install the default font
  MAUI::Engine& engine = MAUI::Engine::getSingleton();
  engine.setDefaultFont(new MAUI::Font(RES_FONT));

  runMode = init_state;
  opt_ide = IDE_NONE;
  opt_graphics = true;
  opt_pref_bpp = 0;
  opt_nosave = true;
  opt_interactive = true;
  opt_verbose = false;
  opt_quiet = true;
  opt_command[0] = 0;
  os_graphics = 1;
}

Controller::~Controller() {
  delete output;
}

// process events while in modal state
void Controller::modalLoop() {
  runMode = modal_state;
  while (runMode == modal_state) {
    processEvents(-1);
  }
}

// pause for the given number of milliseconds
void Controller::pause(int ms) {
  if (runMode == run_state) {
    int msWait = ms / 2;
    int msStart = maGetMilliSecondCount();
    runMode = modal_state;
    while (runMode == modal_state) {
      if (maGetMilliSecondCount() - msStart >= ms) {
        runMode = run_state;
        break;
      }
      processEvents(msWait);
    }
  }
}

// process events on the system event queue
void Controller::processEvents(int ms) {
  MAEvent event;
  MAExtent screenSize;

  while (maGetEvent(&event)) {
    switch (event.type) {
    case EVENT_TYPE_SCREEN_CHANGED:
      screenSize = maGetScrSize();
      output->resize(EXTENT_X(screenSize), EXTENT_Y(screenSize));
      os_graf_mx = output->getWidth();
      os_graf_my = output->getHeight();
      break;
    case EVENT_TYPE_CLOSE:
      fireCloseEvent();
      runMode = exit_state;
      break;
    case EVENT_TYPE_FOCUS_GAINED:
      fireFocusGainedEvent();
      break;
    case EVENT_TYPE_FOCUS_LOST:
      fireFocusLostEvent();
      break;
    case EVENT_TYPE_KEY_PRESSED:
      switch (event.key) {
      case MAK_FIRE:
      case MAK_5:
        break;
      case MAK_SOFTRIGHT:
      case MAK_0:
      case MAK_BACK:
        runMode = exit_state;
        break;
      }
      fireKeyPressEvent(event.key, event.nativeKey);
      break;
    case EVENT_TYPE_KEY_RELEASED:
      fireKeyReleaseEvent(event.key, event.nativeKey);
      break;
    case EVENT_TYPE_CHAR:
      fireCharEvent(event.character);
      break;
    case EVENT_TYPE_POINTER_PRESSED:
      if (event.touchId == 0) {
        firePointerPressEvent(event.point);
      }
      fireMultitouchPressEvent(event.point, event.touchId);
      break;
    case EVENT_TYPE_POINTER_DRAGGED:
      if (event.touchId == 0) {
        firePointerMoveEvent(event.point);
      }
      fireMultitouchMoveEvent(event.point, event.touchId);
      break;
    case EVENT_TYPE_POINTER_RELEASED:
      if (event.touchId == 0) {
        firePointerReleaseEvent(event.point);
      }
      fireMultitouchReleaseEvent(event.point, event.touchId);
      break;
    case EVENT_TYPE_CONN:
      fireConnEvent(event.conn);
      break;
    case EVENT_TYPE_BT:
      fireBluetoothEvent(event.state);
      break;
    case EVENT_TYPE_TEXTBOX:
      fireTextBoxListeners(event.textboxResult, event.textboxLength);
      break;
    case EVENT_TYPE_SENSOR:
      fireSensorListeners(event.sensor);
      break;
    default:
      fireCustomEventListeners(event);
      break;
    }
  }

  if (runMode == exit_state) {
    // terminate the running program
    ui_reset();
    brun_break();
  }
  else {
    // pump messages into the engine
    runIdleListeners();
    maWait(ms);
  }
}

void trace(const char *format, ...) {
  char buf[4096], *p = buf;
  va_list args;

  va_start(args, format);
  p += vsnprintf(p, sizeof buf - 1, format, args);
  va_end(args);

  while (p > buf && isspace(p[-1])) {
    *--p = '\0';
  }

  *p++ = '\r';
  *p++ = '\n';
  *p = '\0';

  maWriteLog(buf, strlen(buf));
}
