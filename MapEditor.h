#ifndef MAP_EDITOR_H
#define MAP_EDITOR_H

#include "TilesetRequester.h"

#include <intuition/intuition.h>

#define CURRENT_TILESET_ID (0)
#define CHOOSE_TILESET_ID  (CURRENT_TILESET_ID + 1)
#define TILESET_SCROLL_ID  (CHOOSE_TILESET_ID  + 1)

typedef struct MapEditorTag {
	struct MapEditorTag *next;
	struct MapEditorTag *prev;
	struct Window *window;
	struct Gadget *gadgets;
	int closed;
	TilesetRequester *tilesetRequester;
} MapEditor;

void initMapEditorScreen(void);
void initMapEditorVi(void);

MapEditor *newMapEditor(void);
void closeMapEditor(MapEditor*);

void refreshMapEditor(MapEditor*);

void attachTilesetRequesterToMapEditor(MapEditor*, TilesetRequester*);

#endif
