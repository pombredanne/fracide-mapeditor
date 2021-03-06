#ifndef FRAMEWORK_GADGET_EVENTS_H
#define FRAMEWORK_GADGET_EVENTS_H

#include <libraries/gadtools.h>

#include "window.h"

typedef void (*GadgetUpHandler)(FrameworkWindow*, UWORD code);

GadgetUpHandler findHandlerForGadgetUp(struct Gadget*);

#endif
