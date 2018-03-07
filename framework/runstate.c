#include "runstate.h"

#include <exec/types.h>

#include <proto/exec.h>

#include "window.h"
#include "windowset.h"

static void cleanupDeadWindows(FrameworkWindow*);

static void cleanupDeadChildWindows(FrameworkWindow *window) {
  FrameworkWindow *i, *next;

  i = window->children;
  while(i) {
    next = i->next;
    cleanupDeadWindows(i);
    i = next;
  }
}

static void cleanupDeadWindows(FrameworkWindow *window) {
  cleanupDeadChildWindows(window);

  if(window->closed) {
    closeWindow(window);
  }
}

void runMainLoop(FrameworkWindow *root) {
  BOOL running = TRUE;
  while(running) {
    long signalSet = Wait(windowSetSigMask());
    handleWindowEvents(root, signalSet);
    running = !root->closed;
    cleanupDeadWindows(root);
  }
}
