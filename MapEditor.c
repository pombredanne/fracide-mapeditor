#include "MapEditor.h"

#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <proto/intuition.h>

#include <libraries/gadtools.h>
#include <proto/gadtools.h>

#include <stdlib.h>

#include "TilesetRequester.h"
#include "globals.h"

#define MAP_EDITOR_WIDTH  520
#define MAP_EDITOR_HEIGHT 336

static struct NewWindow mapEditorNewWindow = {
	40, 40, MAP_EDITOR_WIDTH, MAP_EDITOR_HEIGHT,
	0xFF, 0xFF,
	CLOSEWINDOW|REFRESHWINDOW|GADGETUP,
	WINDOWCLOSE|WINDOWDEPTH|WINDOWDRAG|WINDOWSIZING|ACTIVATE,
	NULL,
	NULL,
	"Map Editor",
	NULL,
	NULL,
	MAP_EDITOR_WIDTH,MAP_EDITOR_HEIGHT,
	MAP_EDITOR_WIDTH,MAP_EDITOR_HEIGHT,
	CUSTOMSCREEN
};

/* TODO: get the font from the system preferences */
static struct TextAttr Topaz80 = { "topaz.font", 8, 0, 0 };

#define TILE_WIDTH  16
#define TILE_HEIGHT 16

/* TODO: adjust based on titlebar height */
#define CURRENT_TILESET_LEFT   352
#define CURRENT_TILESET_TOP    36
#define CURRENT_TILESET_WIDTH  144
#define CURRENT_TILESET_HEIGHT 12

#define CHOOSE_TILESET_LEFT    CURRENT_TILESET_LEFT
#define CHOOSE_TILESET_TOP     CURRENT_TILESET_TOP + CURRENT_TILESET_HEIGHT
#define CHOOSE_TILESET_HEIGHT  12
#define CHOOSE_TILESET_WIDTH   CURRENT_TILESET_WIDTH

#define TILESET_SCROLL_HEIGHT  TILE_HEIGHT * TILESET_PALETTE_TILES_HIGH * 2
#define TILESET_SCROLL_WIDTH   CHOOSE_TILESET_WIDTH - (TILE_WIDTH * TILESET_PALETTE_TILES_ACROSS * 2)
#define TILESET_SCROLL_LEFT    CURRENT_TILESET_LEFT + TILE_WIDTH * TILESET_PALETTE_TILES_ACROSS * 2 - 1
#define TILESET_SCROLL_TOP     CHOOSE_TILESET_TOP + CHOOSE_TILESET_HEIGHT + 8

/* TODO: adjust based on screen */
#define MAP_BORDER_LEFT   20
#define MAP_BORDER_TOP    37
#define MAP_BORDER_WIDTH  320
#define MAP_BORDER_HEIGHT 288

#define TILESET_BORDER_LEFT   CURRENT_TILESET_LEFT
#define TILESET_BORDER_TOP    TILESET_SCROLL_TOP + 1
#define TILESET_BORDER_WIDTH  TILE_WIDTH * TILESET_PALETTE_TILES_ACROSS * 2
#define TILESET_BORDER_HEIGHT TILESET_SCROLL_HEIGHT

static struct NewGadget currentTilesetNewGadget = {
	CURRENT_TILESET_LEFT,  CURRENT_TILESET_TOP,
	CURRENT_TILESET_WIDTH, CURRENT_TILESET_HEIGHT,
	"Current Tileset",
	&Topaz80,
	CURRENT_TILESET_ID,
	PLACETEXT_ABOVE,
	NULL, /* visual info, filled in later */
	NULL  /* user data */
};

static struct NewGadget chooseTilesetNewGadget = {
	CHOOSE_TILESET_LEFT, CHOOSE_TILESET_TOP,
	CHOOSE_TILESET_WIDTH, CHOOSE_TILESET_HEIGHT,
	"Choose Tileset...",
	&Topaz80,
	CHOOSE_TILESET_ID,
	PLACETEXT_IN,
	NULL, /* visual info, filled in later */
	NULL  /* user data */
};

static struct NewGadget tilesetScrollNewGadget = {
	TILESET_SCROLL_LEFT,  TILESET_SCROLL_TOP,
	TILESET_SCROLL_WIDTH, TILESET_SCROLL_HEIGHT,
	NULL, /* no text */
	NULL,
	TILESET_SCROLL_ID,
	0,    /* flags */
	NULL, /* visual info, filled in later */
	NULL  /* user data */
};

static struct NewGadget *allNewGadgets[] = {
	&currentTilesetNewGadget,
	&chooseTilesetNewGadget,
	&tilesetScrollNewGadget,
	NULL
};

/* TODO: generate these dynamically */
static WORD mapBorderPoints[] = {
	0,                  0,
	MAP_BORDER_WIDTH-1, 0,
	MAP_BORDER_WIDTH-1, MAP_BORDER_HEIGHT-1,
	0,                  MAP_BORDER_HEIGHT-1,
	0,                  0
};

static struct Border mapBorder = {
	-1, -1,
	1, 1,
	JAM1,
	5, mapBorderPoints,
	NULL
};

static WORD tilesetBorderPoints[] = {
	0,                      0,
	TILESET_BORDER_WIDTH-1, 0,
	TILESET_BORDER_WIDTH-1, TILESET_BORDER_HEIGHT-1,
	0,                      TILESET_BORDER_HEIGHT-1,
	0,                      0
};

static struct Border tilesetBorder = {
	-1, -1,
	1, 1,
	JAM1,
	5, tilesetBorderPoints,
	NULL
};

void initMapEditorScreen(void) {
	mapEditorNewWindow.Screen = screen;
}

void initMapEditorVi(void) {
	struct NewGadget **i = allNewGadgets;
	while(*i) {
		(*i)->ng_VisualInfo = vi;
		i++;
	}
}

static void createMapEditorGadgets(MapEditor *mapEditor) {
	struct Gadget *gad;
	struct Gadget *glist = NULL;

	gad = CreateContext(&glist);

	gad = CreateGadget(TEXT_KIND, gad, &currentTilesetNewGadget,
		GTTX_Text, "N/A",
		GTTX_Border, TRUE,
		TAG_END);
	mapEditor->tilesetNameGadget = gad;
	
	gad = CreateGadget(BUTTON_KIND, gad, &chooseTilesetNewGadget, TAG_END);

	gad = CreateGadget(SCROLLER_KIND, gad, &tilesetScrollNewGadget,
		PGA_Freedom, LORIENT_VERT,
		GA_Disabled, TRUE,
		TAG_END);

	if(gad) {
		mapEditor->gadgets = glist;
	} else {
		mapEditor->tilesetNameGadget = NULL;
		FreeGadgets(glist);
	}
}

/* TODO: make a list and iterate */
static void drawBorders(struct RastPort *rport) {
	DrawBorder(rport, &mapBorder,     MAP_BORDER_LEFT, MAP_BORDER_TOP);
	DrawBorder(rport, &tilesetBorder,
		TILESET_BORDER_LEFT, TILESET_BORDER_TOP);
}

void refreshMapEditor(MapEditor *mapEditor) {
	drawBorders(mapEditor->window->RPort);
}

MapEditor *newMapEditor(void) {
	MapEditor *mapEditor = malloc(sizeof(MapEditor));
	if(!mapEditor) {
		goto error;
	}

	createMapEditorGadgets(mapEditor);
	if(!mapEditor->gadgets) {
		goto error_freeEditor;
	}
	mapEditorNewWindow.FirstGadget = mapEditor->gadgets;

	mapEditor->window = OpenWindow(&mapEditorNewWindow);
	if(!mapEditor->window) {
		goto error_freeGadgets;
	}

	GT_RefreshWindow(mapEditor->window, NULL);
	refreshMapEditor(mapEditor);

	mapEditor->prev             = NULL;
	mapEditor->next             = NULL;
	mapEditor->tilesetRequester = NULL;
	mapEditor->closed           = 0;

	return mapEditor;

error_freeGadgets:
	FreeGadgets(mapEditor->gadgets);
error_freeEditor:
	free(mapEditor);
error:
	return NULL;
}

static void closeAttachedTilesetRequester(MapEditor *mapEditor) {
	if(mapEditor->tilesetRequester) {
		closeTilesetRequester(mapEditor->tilesetRequester);
		mapEditor->tilesetRequester = NULL;
	}
}

void closeMapEditor(MapEditor *mapEditor) {
	closeAttachedTilesetRequester(mapEditor);
	CloseWindow(mapEditor->window);
	FreeGadgets(mapEditor->gadgets);
	free(mapEditor);
}

void attachTilesetRequesterToMapEditor
(MapEditor *mapEditor, TilesetRequester *tilesetRequester) {
	mapEditor->tilesetRequester = tilesetRequester;
}

void mapEditorSetTileset(MapEditor *mapEditor, UWORD tilesetNumber) {
	GT_SetGadgetAttrs(mapEditor->tilesetNameGadget, mapEditor->window, NULL,
		GTTX_Text, tilesetPackage->tilesetPackageFile.tilesetNames[tilesetNumber],
		TAG_END);
}
