#ifndef FRAMEWORK_GADGETS_H
#define FRAMEWORK_GADGETS_H

#include <libraries/gadtools.h>

typedef enum State_tag {
  DISABLED,
  ENABLED
} State;

typedef enum TextPlacement_tag {
  TEXT_ON_THE_LEFT,
  TEXT_ON_THE_RIGHT,
  TEXT_ABOVE,
  TEXT_BELOW,
  TEXT_INSIDE
} TextPlacement;

typedef struct ButtonSpec_tag {
  WORD left, top;
  WORD width, height;
  char *label;
  TextPlacement textPlacement;
  State state;
} ButtonSpec;

typedef struct GadgetSpec_tag {
  unsigned long kind;
  struct NewGadget *newGadget;
  struct TagItem *tags;
} GadgetSpec;

struct Gadget *buildGadgets(GadgetSpec*, struct Gadget**, ...);

#endif
