#ifndef TILESET_REQUESTER_H
#define TILESET_REQUESTER_H

#include <intuition/intuition.h>

#define TILESET_LIST_ID 0

typedef struct TilesetRequesterTag {
    struct Window *window;
    struct Gadget *gadgets;
    struct Gadget *tilesetList;
    int           closed;
    char          *title;
} TilesetRequester;

void initTilesetRequesterVi(void);

TilesetRequester *newTilesetRequester(char *title);
void closeTilesetRequester(TilesetRequester*);

void refreshTilesetRequesterList(TilesetRequester*);
void resizeTilesetRequester(TilesetRequester*);

#endif
