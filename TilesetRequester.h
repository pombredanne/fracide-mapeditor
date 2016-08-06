#ifndef TILESET_REQUESTER_H
#define TILESET_REQUESTER_H

#include <intuition/intuition.h>

#define TILESET_LIST_ID 0

typedef struct TilesetRequesterTag {
	struct Window *window;
	struct Gadget *gadgets;
	int closed;
} TilesetRequester;

void initTilesetRequesterScreen(void);
void initTilesetRequesterVi(void);

TilesetRequester *newTilesetRequester(void);
void closeTilesetRequester(TilesetRequester*);

void refreshTilesetRequester(TilesetRequester*);

#endif
