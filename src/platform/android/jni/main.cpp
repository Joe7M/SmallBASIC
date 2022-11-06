/// This file is part of SmallBASIC
//
// Copyright(C) 2001-2015 Chris Warren-Smith.
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//

#include "config.h"
#include "platform/android/jni/runtime.h"
#include <endian.h>

#if _BYTE_ORDER != _LITTLE_ENDIAN || defined (__BIG_ENDIAN_BITFIELD)
  #error "Big-Endian Arch is not supported"
#endif

void android_main(android_app *app) {
  logEntered();

  auto *runtime = new Runtime(app);

  // pump events until startup has completed
  while (runtime->isInitial()) {
    runtime->pollEvents(true);
  }
  if (!runtime->isClosing()) {
    runtime->runShell();
  }

  ANativeActivity_finish(app->activity);
  delete runtime;
  logLeaving();

  // underscore version does not invoke atExit functions
  // if these are defined (as per kitkat) an error would occur
  _exit(0);
}
