#include "MapEditor.h"

#include <exec/exec.h>
#include <proto/exec.h>

#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <proto/intuition.h>

#include <libraries/gadtools.h>
#include <proto/gadtools.h>

#include <graphics/gfx.h>
#include <graphics/scale.h>
#include <proto/graphics.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "framework/menubuild.h"
#include "framework/screen.h"

#include "easystructs.h"
#include "EntityBrowser.h"
#include "globals.h"
#include "map.h"
#include "ProjectWindow.h"
#include "ProjectWindowData.h"
#include "TilesetPackage.h"
#include "TilesetRequester.h"

#define MAP_EDITOR_WIDTH  536
#define MAP_EDITOR_HEIGHT 384

static void newMapMenuItemClicked(FrameworkWindow*);
static void openMapMenuItemClicked(FrameworkWindow*);
static BOOL saveMap(FrameworkWindow*);
static BOOL saveMapAs(FrameworkWindow*);
static void revertMap(FrameworkWindow*);
static void closeMap(FrameworkWindow*);

static MenuSectionSpec newSection =
  { { "New", "N", MENU_ITEM_ENABLED, newMapMenuItemClicked },
    END_SECTION };

static MenuSectionSpec openSection =
  { { "New", "N", MENU_ITEM_ENABLED, openMapMenuItemClicked },
    END_SECTION };

static MenuSectionSpec saveSection =
  { { "Save",       "S",         MENU_ITEM_ENABLED,  (Handler)saveMap   },
    { "Save As...", "A",         MENU_ITEM_ENABLED,  (Handler)saveMapAs },
    { "Revert",     NO_SHORTKEY, MENU_ITEM_DISABLED,          revertMap },
  END_SECTION };

static MenuSectionSpec closeSection =
  { { "Close", "Q", MENU_ITEM_ENABLED, closeMap },
    END_SECTION };

static MenuSectionSpec *mapMenuSpec[] = {
  &newSection,
  &openSection,
  &saveSection,
  &closeSection,
  END_MENU
};

#define REVERT_MAP_MENU_ITEM (SHIFTMENU(0) | SHIFTITEM(6))

static MenuSpec mapEditorMenuSpec[] = {
  { "Map", &mapMenuSpec },
  END_MENUS
};

static struct Menu *menu = NULL;

static void newMapMenuItemClicked(FrameworkWindow *mapEditorWindow) {
  FrameworkWindow *projectWindow = mapEditorWindow->parent;
  newMap(projectWindow);
}

static void openMapMenuItemClicked(FrameworkWindow *mapEditorWindow) {
  FrameworkWindow *projectWindow = mapEditorWindow->parent;
  openMap(projectWindow);
}

static void enableMapRevert(FrameworkWindow *mapEditorWindow) {
  OnMenu(mapEditorWindow->intuitionWindow, REVERT_MAP_MENU_ITEM);
}

static void disableMapRevert(FrameworkWindow *mapEditorWindow) {
  OffMenu(mapEditorWindow->intuitionWindow, REVERT_MAP_MENU_ITEM);
}

static BOOL saveMapAs(FrameworkWindow *mapEditorWindow) {
  int selected = saveMapRequester(mapEditorWindow);
  if(!selected) {
    return FALSE;
  }

  if(1/* TODO: fix me: !currentProjectHasMap(selected - 1) */) {
    if(0/* TODO: fix me: !currentProjectSaveNewMap(mapEditor->map, selected - 1) */) {
      fprintf(stderr, "saveMapAs: failed to save map\n");
      return FALSE;
    }
  } else {
    int response = EasyRequest(
      mapEditorWindow->intuitionWindow,
      &saveIntoFullSlotEasyStruct,
      NULL,
      selected - 1, NULL /* FIXME currentProjectGetMapName(selected - 1) */);
    if(response) {
      /* TODO: fix me */
      /*currentProjectOverwriteMap(mapEditor->map, selected - 1);*/
    } else {
      return FALSE;
    }
  }

  mapEditorSetMapNum(mapEditorWindow, selected - 1);
  disableMapRevert(mapEditorWindow);

  mapEditorSetSaveStatus(mapEditorWindow, SAVED);

  /* TODO: fix me */
  /* updateCurrentProjectMapName(selected - 1, mapEditor->map); */

  return TRUE;
}

static BOOL saveMap(FrameworkWindow *mapEditorWindow) {
  MapEditorData *data = mapEditorWindow->data;
  if(!data->mapNum) {
    return saveMapAs(mapEditorWindow);
  } else {
    /* TODO: fix me */
    /* currentProjectOverwriteMap(mapEditor->map, mapEditor->mapNum - 1); */
    /* TODO: this is what sets the saved status, but that feels fragile */
    /* TODO: fix me */
    /* updateCurrentProjectMapName(mapEditor->mapNum - 1, mapEditor->map);*/
    mapEditorSetSaveStatus(mapEditorWindow, SAVED);
    return TRUE;
  }
}

static int confirmRevertMap(FrameworkWindow *mapEditorWindow) {
  MapEditorData *data = mapEditorWindow->data;
  return EasyRequest(
    mapEditorWindow->intuitionWindow,
    &confirmRevertMapEasyStruct,
    NULL,
    data->mapNum - 1,
    data->map->name);
}

static void revertMap(FrameworkWindow *mapEditorWindow) {
  if(confirmRevertMap(mapEditorWindow)) {
    FrameworkWindow *projectWindow = mapEditorWindow->parent;
    MapEditorData *data = mapEditorWindow->data;
    mapEditorWindow->closed = 1;
    openMapNum(projectWindow, data->mapNum - 1);
  }
}

static BOOL unsavedMapEditorAlert(FrameworkWindow *mapEditorWindow) {
  int response;
  MapEditorData *data = mapEditorWindow->data;

  if(data->mapNum) {
    response = EasyRequest(
      mapEditorWindow->intuitionWindow,
      &unsavedMapAlertEasyStructWithNum,
      NULL,
      data->mapNum - 1, data->map->name);
  } else {
    response = EasyRequest(
      mapEditorWindow->intuitionWindow,
      &unsavedMapAlertEasyStructNoNum,
      NULL,
      data->map->name);
  }

  switch(response) {
    case 0: return FALSE;                    /* cancel */
    case 1: return saveMap(mapEditorWindow); /* save */
    case 2: return TRUE;                     /* don't save */
    default:
      fprintf(stderr, "unsavedMapEditorAlert: unknown response %d\n", response);
      return FALSE;
    }
}

BOOL ensureMapEditorSaved(FrameworkWindow *mapEditorWindow) {
  MapEditorData *data = mapEditorWindow->data;
  return (BOOL)(data->saved || unsavedMapEditorAlert(mapEditorWindow));
}

static void closeMap(FrameworkWindow *mapEditorWindow) {
  MapEditorData *data = mapEditorWindow->data;
  if(data->saved || unsavedMapEditorAlert(mapEditorWindow)) {
    mapEditorWindow->closed = 1;
  }
}

#define TILE_WIDTH  16
#define TILE_HEIGHT 16

/* TODO: adjust based on screen */
#define MAP_BORDER_LEFT   28
#define MAP_BORDER_TOP    51
#define MAP_BORDER_WIDTH  (MAP_TILES_ACROSS * TILE_WIDTH  * 2 + 2)
#define MAP_BORDER_HEIGHT (MAP_TILES_HIGH   * TILE_HEIGHT * 2 + 2)

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

/* TODO: adjust based on titlebar height */
#define CURRENT_TILESET_LEFT   378
#define CURRENT_TILESET_TOP    36
#define CURRENT_TILESET_WIDTH  144
#define CURRENT_TILESET_HEIGHT 12

#define CHOOSE_TILESET_LEFT    CURRENT_TILESET_LEFT
#define CHOOSE_TILESET_TOP     CURRENT_TILESET_TOP + CURRENT_TILESET_HEIGHT
#define CHOOSE_TILESET_HEIGHT  12
#define CHOOSE_TILESET_WIDTH   CURRENT_TILESET_WIDTH

#define TILESET_SCROLL_HEIGHT  TILE_HEIGHT * TILESET_PALETTE_TILES_HIGH * 2 + 2
#define TILESET_SCROLL_WIDTH   CHOOSE_TILESET_WIDTH - (TILE_WIDTH * TILESET_PALETTE_TILES_ACROSS * 2)
#define TILESET_SCROLL_LEFT    CURRENT_TILESET_LEFT + TILE_WIDTH * TILESET_PALETTE_TILES_ACROSS * 2 + 1
#define TILESET_SCROLL_TOP     CHOOSE_TILESET_TOP + CHOOSE_TILESET_HEIGHT + 8

#define TILESET_BORDER_LEFT   CURRENT_TILESET_LEFT
#define TILESET_BORDER_TOP    (TILESET_SCROLL_TOP + 1)
#define TILESET_BORDER_WIDTH  (TILE_WIDTH * TILESET_PALETTE_TILES_ACROSS * 2 + 2)
#define TILESET_BORDER_HEIGHT TILESET_SCROLL_HEIGHT

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

static void drawBorders(struct RastPort *rport) {
  DrawBorder(rport, &mapBorder, MAP_BORDER_LEFT, MAP_BORDER_TOP);
  DrawBorder(rport, &tilesetBorder,
    TILESET_BORDER_LEFT, TILESET_BORDER_TOP);
}

static void refreshMapEditor(FrameworkWindow *mapEditorWindow) {
  drawBorders(mapEditorWindow->intuitionWindow->RPort);
}

static void attachTilesetRequesterToMapEditor
(MapEditorData *data, TilesetRequester *tilesetRequester) {
  data->tilesetRequester = tilesetRequester;
}

static void handleChooseTilesetClicked(FrameworkWindow *mapEditorWindow) {
  TilesetRequester *tilesetRequester;
  char title[32];
  MapEditorData *data = mapEditorWindow->data;
  FrameworkWindow *projectWindow = mapEditorWindow->parent;
  ProjectWindowData *parentData = projectWindow->data;
  TilesetPackage *tilesetPackage = NULL /* TODO: fix me parentData->tilesetPackage */;

  if(data->tilesetRequester) {
    WindowToFront(data->tilesetRequester->window->intuitionWindow);
    goto done;
  }

  if(!tilesetPackage) {
    int choice = EasyRequest(
      mapEditorWindow->intuitionWindow,
      &noTilesetPackageLoadedEasyStruct,
      NULL);

    if(choice) {
      selectTilesetPackage(projectWindow);
    }
  }

  /* even after giving the user the opportunity to set the tileset
     package, we need to be sure they did so... */
  if(!tilesetPackage) {
    goto done;
  }

  if(data->mapNum) {
    sprintf(title, "Choose Tileset For Map %d", data->mapNum - 1);
  } else {
    strcpy(title, "Choose Tileset");
  }

  tilesetRequester = newTilesetRequester(title);
  if(!tilesetRequester) {
    fprintf(stderr, "handleChooseTilesetClicked: couldn't make requester\n");
    goto error;
  }

  attachTilesetRequesterToMapEditor(data, tilesetRequester);
  /* TODO: fix me */
  /* addWindowToSet(tilesetRequester->window); */
done:
  return;
error:
  return;
}

static void updateMapEditorMapName(FrameworkWindow *mapEditorWindow) {
  MapEditorData *data = mapEditorWindow->data;
  struct StringInfo *stringInfo = data->mapNameGadget->SpecialInfo;

  strcpy(data->map->name, stringInfo->Buffer);
  mapEditorSetSaveStatus(mapEditorWindow, UNSAVED);
}

static void attachSongRequesterToMapEditor
(MapEditorData *data, SongRequester *songRequester) {
  data->songRequester = songRequester;
}

static void handleChangeSongClicked(FrameworkWindow *mapEditorWindow) {
  MapEditorData *data = mapEditorWindow->data;

  if(!data->songRequester) {
    char title[32];
    SongRequester *songRequester;

    if(data->mapNum) {
      sprintf(title, "Change Soundtrack For Map %d", data->mapNum - 1);
    } else {
      strcpy(title, "Change Soundtrack");
    }

    songRequester = newSongRequester(title);
    if(songRequester) {
      attachSongRequesterToMapEditor(data, songRequester);
      /* TODO: fix me */
      /* addWindowToSet(songRequester->window); */
    }
  } else {
    WindowToFront(data->songRequester->window->intuitionWindow);
  }
}

static void mapEditorClearSongUpdateUI(FrameworkWindow *mapEditorWindow) {
  MapEditorData *data = mapEditorWindow->data;
  GT_SetGadgetAttrs(data->songNameGadget, mapEditorWindow->intuitionWindow, NULL,
    GTTX_Text, "N/A",
    TAG_END);
}

static void mapEditorClearSong(FrameworkWindow *mapEditorWindow) {
  MapEditorData *data = mapEditorWindow->data;
  data->map->songNum = 0;
  mapEditorClearSongUpdateUI(mapEditorWindow);
  mapEditorSetSaveStatus(mapEditorWindow, UNSAVED);
}

static void moveToMap(FrameworkWindow *mapEditorWindow, int mapNum) {
  MapEditorData *data = mapEditorWindow->data;
  FrameworkWindow *projectWindow = mapEditorWindow->parent;

  if(data->saved || unsavedMapEditorAlert(mapEditorWindow)) {
    if(openMapNum(projectWindow, mapNum - 1)) {
      mapEditorWindow->closed = 1;
    }
  }
}

static void handleMapUp(FrameworkWindow *mapEditorWindow) {
  MapEditorData *data = mapEditorWindow->data;
  moveToMap(mapEditorWindow, data->mapNum - 16);
}

static void handleMapDown(FrameworkWindow *mapEditorWindow) {
  MapEditorData *data = mapEditorWindow->data;
  moveToMap(mapEditorWindow, data->mapNum + 16);
}

static void handleMapLeft(FrameworkWindow *mapEditorWindow) {
  MapEditorData *data = mapEditorWindow->data;
  moveToMap(mapEditorWindow, data->mapNum - 1);
}

static void handleMapRight(FrameworkWindow *mapEditorWindow) {
  MapEditorData *data = mapEditorWindow->data;
  moveToMap(mapEditorWindow, data->mapNum + 1);
}

static void attachEntityBrowserToMapEditor
(MapEditorData *data, EntityBrowser *entityBrowser) {
  data->entityBrowser = entityBrowser;
}

static void openNewEntityBrowser(FrameworkWindow *mapEditorWindow) {
  char title[32];
  EntityBrowser *entityBrowser;
  MapEditorData *data = mapEditorWindow->data;

  if(data->mapNum) {
    sprintf(title, "Entities (Map %d)", data->mapNum - 1);
  } else {
    strcpy(title, "Entities");
  }

  entityBrowser = newEntityBrowser(title, data->map->entities, data->map->entityCnt);
  if(!entityBrowser) {
    fprintf(stderr, "openNewEntityBrowser: couldn't create new entity browser\n");
    goto error;
  }

  attachEntityBrowserToMapEditor(data, entityBrowser);
  /* TODO: fix me */
  /* addWindowToSet(entityBrowser->window); */

  done:
    return;

  error:
    return;
}

static void handleEntitiesClicked(FrameworkWindow *mapEditorWindow) {
  MapEditorData *data = mapEditorWindow->data;

  if(!data->entityBrowser) {
    openNewEntityBrowser(mapEditorWindow);
  } else {
    WindowToFront(data->entityBrowser->window->intuitionWindow);
  }
}

static void handleMapEditorGadgetUp(FrameworkWindow *mapEditorWindow, struct Gadget *gadget) {
  switch(gadget->GadgetID) {
    case CHOOSE_TILESET_ID:
      handleChooseTilesetClicked(mapEditorWindow);
      break;
    case MAP_NAME_ID:
      updateMapEditorMapName(mapEditorWindow);
      break;
    case SONG_CHANGE_ID:
      handleChangeSongClicked(mapEditorWindow);
      break;
    case SONG_CLEAR_ID:
      mapEditorClearSong(mapEditorWindow);
      break;
    case MAP_LEFT_ID:
      handleMapLeft(mapEditorWindow);
      break;
    case MAP_RIGHT_ID:
      handleMapRight(mapEditorWindow);
      break;
    case MAP_UP_ID:
      handleMapUp(mapEditorWindow);
      break;
    case MAP_DOWN_ID:
      handleMapDown(mapEditorWindow);
      break;
    case ENTITIES_ID:
      handleEntitiesClicked(mapEditorWindow);
      break;
    }
}


static int mapEditorClickInPalette(WORD x, WORD y) {
  return ((x > TILESET_BORDER_LEFT                        ) &&
          (x < TILESET_BORDER_LEFT + TILESET_BORDER_WIDTH ) &&
          (y > TILESET_BORDER_TOP                         ) &&
          (y < TILESET_BORDER_TOP  + TILESET_BORDER_HEIGHT));
}

static int mapEditorClickInMap(WORD x, WORD y) {
  return ((x > MAP_BORDER_LEFT                    ) &&
          (x < MAP_BORDER_LEFT + MAP_BORDER_WIDTH ) &&
          (y > MAP_BORDER_TOP                     ) &&
          (y < MAP_BORDER_TOP  + MAP_BORDER_HEIGHT));
}

static unsigned int mapEditorGetPaletteTileClicked(WORD x, WORD y) {
  unsigned int row = y;
  unsigned int col = x;

  row -= TILESET_BORDER_TOP;
  col -= TILESET_BORDER_LEFT;

  row >>= 5;
  col >>= 5;

  return (row << 2) + col;
}

static unsigned int mapEditorGetMapTileClicked(WORD x, WORD y) {
  unsigned int row = y;
  unsigned int col = x;

  row -= MAP_BORDER_TOP;
  col -= MAP_BORDER_LEFT;

  row >>= 5;
  col >>= 5;

  return (row * 10) + col;
}

static void redrawPaletteTile(FrameworkWindow *mapEditorWindow, unsigned int tile) {
  MapEditorData *data = mapEditorWindow->data;
  struct Image *image = &data->paletteImages[tile];
  struct Image *next = image->NextImage;
  image->NextImage = NULL;
  DrawImage(mapEditorWindow->intuitionWindow->RPort, image,
    TILESET_BORDER_LEFT,
    TILESET_BORDER_TOP);
  image->NextImage = next;
}

static WORD tileBorderPoints[] = {
  0,  0,
  31, 0,
  31, 31,
  0,  31,
  0,  0
};
static struct Border tileBorder = {
  0, 0,
  0, 0,
  COMPLEMENT,
  5, tileBorderPoints,
  NULL
};

static void mapEditorSetSelected(FrameworkWindow *mapEditorWindow, unsigned int selected) {
  long row;
  long col;
  MapEditorData *data = mapEditorWindow->data;

  if(data->selected >= 0) {
    redrawPaletteTile(mapEditorWindow, data->selected);
  }

  data->selected = (int)selected;

  row = selected >> 2;
  col = selected & 0x03;

  DrawBorder(mapEditorWindow->intuitionWindow->RPort, &tileBorder,
    TILESET_BORDER_LEFT + (col * 32),
    TILESET_BORDER_TOP  + (row * 32));
}

static void handleMapEditorPaletteClick(FrameworkWindow *mapEditorWindow, WORD x, WORD y) {
  int tile = mapEditorGetPaletteTileClicked(x, y);
  mapEditorSetSelected(mapEditorWindow, tile);
}

static void copyScaledTileset(UWORD *src, UWORD *dst) {
  struct BitMap srcBitMap;
  struct BitMap dstBitMap;
  struct BitScaleArgs scaleArgs;
  int tileNum;

  srcBitMap.BytesPerRow = 2;
  srcBitMap.Rows = 16;
  srcBitMap.Flags = 0;
  srcBitMap.Depth = 2;

  dstBitMap.BytesPerRow = 4;
  dstBitMap.Rows = 32;
  dstBitMap.Flags = 0;
  dstBitMap.Depth = 2;

  scaleArgs.bsa_SrcX = 0;
  scaleArgs.bsa_SrcY = 0;
  scaleArgs.bsa_SrcWidth = 16;
  scaleArgs.bsa_SrcHeight = 16;
  scaleArgs.bsa_XSrcFactor = 1;
  scaleArgs.bsa_YSrcFactor = 1;
  scaleArgs.bsa_DestX = 0;
  scaleArgs.bsa_DestY = 0;
  scaleArgs.bsa_XDestFactor = 2;
  scaleArgs.bsa_YDestFactor = 2;
  scaleArgs.bsa_SrcBitMap = &srcBitMap;
  scaleArgs.bsa_DestBitMap = &dstBitMap;
  scaleArgs.bsa_Flags = 0;

  for(tileNum = 0; tileNum < TILES_PER_SET; tileNum++) {
    srcBitMap.Planes[0] = (PLANEPTR)src;
    srcBitMap.Planes[1] = (PLANEPTR)(src + 16);

    dstBitMap.Planes[0] = (PLANEPTR)dst;
    dstBitMap.Planes[1] = (PLANEPTR)(dst + 64);

    BitMapScale(&scaleArgs);

    src += 32;
    dst += 128;
  }
}

static void drawEntity(struct RastPort *rport, Entity *entity, int entityNum) {
  char             text[2];
  struct IntuiText itext;

  text[0] = '0' + entityNum;
  text[1] = '\0';

  itext.DrawMode  = COMPLEMENT;
  itext.ITextFont = NULL;
  itext.NextText  = NULL;
  itext.IText     = text;
  itext.LeftEdge  = entity->col * 32;
  itext.TopEdge   = entity->row * 32;

  PrintIText(rport, &itext, MAP_BORDER_LEFT + 1, MAP_BORDER_TOP + 1);

}

/* IN A DREAM WORLD: store the IntuiTexts in the map editor and render them all at once */
static void drawEntities(FrameworkWindow *mapEditorWindow) {
  int i;
  MapEditorData *data = mapEditorWindow->data;
  Entity *entity = &data->map->entities[0];
  struct RastPort *rport = mapEditorWindow->intuitionWindow->RPort;

  for(i = 0; i < data->map->entityCnt; i++) {
    drawEntity(rport, entity, i);
    entity++;
  }
}

static void mapEditorSetTilesetUpdateUI(FrameworkWindow *mapEditorWindow, UWORD tilesetNumber) {
  MapEditorData *data = mapEditorWindow->data;
  ProjectWindowData *parentData = mapEditorWindow->parent->data;
  TilesetPackage *tilesetPackage = NULL /* TODO: fix me parentData->tilesetPackage */;

  GT_SetGadgetAttrs(data->tilesetNameGadget, mapEditorWindow->intuitionWindow, NULL,
    GTTX_Text, tilesetPackage->tilesetPackageFile.tilesetNames[tilesetNumber],
    TAG_END);

  copyScaledTileset(
    (UWORD*)tilesetPackage->tilesetPackageFile.tilesetImgs[tilesetNumber],
    data->imageData);

  DrawImage(mapEditorWindow->intuitionWindow->RPort, data->paletteImages,
    TILESET_BORDER_LEFT,
    TILESET_BORDER_TOP);

  DrawImage(mapEditorWindow->intuitionWindow->RPort, data->mapImages,
    MAP_BORDER_LEFT,
    MAP_BORDER_TOP);

  drawEntities(mapEditorWindow);
}

static void mapEditorSetTileTo(FrameworkWindow *mapEditorWindow, unsigned int tile, UBYTE to) {
  MapEditorData *data = mapEditorWindow->data;
  data->map->tiles[tile] = to;
  data->mapImages[tile].ImageData = data->imageData + (to << 7);
  mapEditorSetSaveStatus(mapEditorWindow, UNSAVED);
}

static void redrawMapTile(FrameworkWindow *mapEditorWindow, unsigned int tile) {
  MapEditorData *data = mapEditorWindow->data;
  struct Image *image = &data->mapImages[tile];
  struct Image *next = image->NextImage;
  int entity_i;

  image->NextImage = NULL;
  DrawImage(mapEditorWindow->intuitionWindow->RPort, image,
    MAP_BORDER_LEFT,
    MAP_BORDER_TOP);
  image->NextImage = next;

  if(entity_i = mapFindEntity(data->map, tile / 10, tile % 10)) {
    entity_i--;
    drawEntity(mapEditorWindow->intuitionWindow->RPort, &data->map->entities[entity_i], entity_i);
  }
}

static void mapEditorSetTile(FrameworkWindow *mapEditorWindow, unsigned int tile) {
  MapEditorData *data = mapEditorWindow->data;
  mapEditorSetTileTo(mapEditorWindow, tile, data->selected);
  redrawMapTile(mapEditorWindow, tile);
}

static void handleMapEditorMapClick(FrameworkWindow *mapEditorWindow, WORD x, WORD y) {
  unsigned int tile = mapEditorGetMapTileClicked(x, y);
  mapEditorSetTile(mapEditorWindow, tile);
}

/* TODO: maybe we can use buttons for this... */
static void handleMapEditorClick(FrameworkWindow *mapEditorWindow, WORD x, WORD y) {
  MapEditorData *data = mapEditorWindow->data;

  if(data->map->tilesetNum) {
    if(mapEditorClickInPalette(x, y)) {
      handleMapEditorPaletteClick(mapEditorWindow, x, y);
    } else if(mapEditorClickInMap(x, y)) {
      handleMapEditorMapClick(mapEditorWindow, x, y);
    }
  }
}

static WindowKind mapEditorWindowKind = {
  {
    40, 40, MAP_EDITOR_WIDTH, MAP_EDITOR_HEIGHT,
    0xFF, 0xFF,
    CLOSEWINDOW|REFRESHWINDOW|GADGETUP|MOUSEBUTTONS|MENUPICK,
    WINDOWCLOSE|WINDOWDEPTH|WINDOWDRAG|ACTIVATE,
    NULL,
    NULL,
    "Map Editor",
    NULL,
    NULL,
    MAP_EDITOR_WIDTH,MAP_EDITOR_HEIGHT,
    MAP_EDITOR_WIDTH,MAP_EDITOR_HEIGHT,
    CUSTOMSCREEN
  },
  (MenuSpec*)        NULL,
  (RefreshFunction)  refreshMapEditor,
  (CanCloseFunction) NULL, /* TODO: check if map is saved here */
  (CloseFunction)    NULL,
  (GadgetUpFunction) handleMapEditorGadgetUp,
  (ClickFunction)    handleMapEditorClick
};

BOOL isMapEditorWindow(FrameworkWindow *window) {
  return (BOOL)(window->kind == &mapEditorWindowKind);
}

#define MAP_NAME_LEFT   (MAP_BORDER_LEFT  + 80)
#define MAP_NAME_TOP    18
#define MAP_NAME_WIDTH  (MAP_BORDER_WIDTH - 81)
#define MAP_NAME_HEIGHT 14

#define ENTITIES_LEFT   (TILESET_BORDER_LEFT - 1)
#define ENTITIES_TOP    (TILESET_BORDER_TOP + TILESET_BORDER_HEIGHT + 10)
#define ENTITIES_WIDTH  (TILESET_BORDER_WIDTH + TILESET_SCROLL_WIDTH)
#define ENTITIES_HEIGHT 14

#define SONG_NAME_LEFT   (MAP_BORDER_LEFT + 95)
#define SONG_NAME_TOP    (MAP_BORDER_TOP + MAP_BORDER_HEIGHT + 20)
#define SONG_NAME_WIDTH  (MAP_BORDER_WIDTH - 182)
#define SONG_NAME_HEIGHT 14

#define SONG_CHANGE_LEFT   (SONG_NAME_LEFT + SONG_NAME_WIDTH - 2)
#define SONG_CHANGE_TOP    SONG_NAME_TOP
#define SONG_CHANGE_WIDTH  76
#define SONG_CHANGE_HEIGHT 14

#define SONG_CLEAR_LEFT   (SONG_CHANGE_LEFT + SONG_CHANGE_WIDTH - 2)
#define SONG_CLEAR_TOP    SONG_CHANGE_TOP
#define SONG_CLEAR_WIDTH  14
#define SONG_CLEAR_HEIGHT 14

#define MAP_UP_LEFT   (MAP_BORDER_LEFT - 3)
#define MAP_UP_TOP    (MAP_BORDER_TOP - 15)
#define MAP_UP_WIDTH  (MAP_BORDER_WIDTH + 4)
#define MAP_UP_HEIGHT 14

#define MAP_DOWN_LEFT   MAP_UP_LEFT
#define MAP_DOWN_TOP    (MAP_BORDER_TOP + MAP_BORDER_HEIGHT - 1)
#define MAP_DOWN_WIDTH  MAP_UP_WIDTH
#define MAP_DOWN_HEIGHT MAP_UP_HEIGHT

#define MAP_LEFT_LEFT   (MAP_BORDER_LEFT - 15)
#define MAP_LEFT_TOP    MAP_BORDER_TOP - 2
#define MAP_LEFT_WIDTH  14
#define MAP_LEFT_HEIGHT MAP_BORDER_HEIGHT + 2

#define MAP_RIGHT_LEFT   (MAP_BORDER_LEFT + MAP_BORDER_WIDTH - 1)
#define MAP_RIGHT_TOP    MAP_LEFT_TOP
#define MAP_RIGHT_WIDTH  MAP_LEFT_WIDTH
#define MAP_RIGHT_HEIGHT MAP_LEFT_HEIGHT

#define IMAGE_DATA_SIZE (TILES_PER_SET * 256)

static struct NewGadget mapNameNewGadget = {
  MAP_NAME_LEFT,  MAP_NAME_TOP,
  MAP_NAME_WIDTH, MAP_NAME_HEIGHT,
  "Map Name:",
  &Topaz80,
  MAP_NAME_ID,
  PLACETEXT_LEFT,
  NULL, /* visual info, filled in later */
  NULL  /* user data */
};

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

static struct NewGadget songNameNewGadget = {
  SONG_NAME_LEFT,  SONG_NAME_TOP,
  SONG_NAME_WIDTH, SONG_NAME_HEIGHT,
  "Soundtrack:",
  &Topaz80,
  SONG_NAME_LABEL_ID,
  PLACETEXT_LEFT,
  NULL,  /* visual info, filled in later */
  NULL   /* user data */
};

static struct NewGadget songChangeNewGadget = {
  SONG_CHANGE_LEFT,  SONG_CHANGE_TOP,
  SONG_CHANGE_WIDTH, SONG_CHANGE_HEIGHT,
  "Change...",
  &Topaz80,
  SONG_CHANGE_ID,
  0,
  NULL,
  NULL
};

static struct NewGadget songClearNewGadget = {
  SONG_CLEAR_LEFT,  SONG_CLEAR_TOP,
  SONG_CLEAR_WIDTH, SONG_CLEAR_HEIGHT,
  "X",
  &Topaz80,
  SONG_CLEAR_ID,
  0,
  NULL,
  NULL
};

static struct NewGadget mapLeftNewGadget = {
  MAP_LEFT_LEFT,  MAP_LEFT_TOP,
  MAP_LEFT_WIDTH, MAP_LEFT_HEIGHT,
  "<",
  &Topaz80,
  MAP_LEFT_ID,
  0,
  NULL,
  NULL
};

static struct NewGadget mapRightNewGadget = {
  MAP_RIGHT_LEFT,  MAP_RIGHT_TOP,
  MAP_RIGHT_WIDTH, MAP_RIGHT_HEIGHT,
  ">",
  &Topaz80,
  MAP_RIGHT_ID,
  0,
  NULL,
  NULL
};

static struct NewGadget mapUpNewGadget = {
  MAP_UP_LEFT,  MAP_UP_TOP,
  MAP_UP_WIDTH, MAP_UP_HEIGHT,
  "^",
  &Topaz80,
  MAP_UP_ID,
  0,
  NULL,
  NULL
};

static struct NewGadget mapDownNewGadget = {
  MAP_DOWN_LEFT,  MAP_DOWN_TOP,
  MAP_DOWN_WIDTH, MAP_DOWN_HEIGHT,
  "v",
  &Topaz80,
  MAP_DOWN_ID,
  0,
  NULL,
  NULL
};

static struct NewGadget entitiesNewGadget = {
  ENTITIES_LEFT,  ENTITIES_TOP,
  ENTITIES_WIDTH, ENTITIES_HEIGHT,
  "Entities...",
  &Topaz80,
  ENTITIES_ID,
  0,
  NULL,
  NULL
};

static struct NewGadget *allNewGadgets[] = {
  &mapNameNewGadget,
  &currentTilesetNewGadget,
  &chooseTilesetNewGadget,
  &tilesetScrollNewGadget,
  &songNameNewGadget,
  &songChangeNewGadget,
  &songClearNewGadget,
  &mapLeftNewGadget,
  &mapRightNewGadget,
  &mapUpNewGadget,
  &mapDownNewGadget,
  &entitiesNewGadget,
  NULL
};

static struct EasyStruct tilesetOutOfBoundsEasyStruct = {
  sizeof(struct EasyStruct),
  0,
  "Tileset Not In New Tileset Package",
  "This map had tileset %ld, which does not exist\nin the new package.\nThe tileset has been removed from this map.",
  "OK"
};

static void createMapEditorGadgets(MapEditorData *data) {
  struct Gadget *gad;
  struct Gadget *glist = NULL;

  gad = CreateContext(&glist);

  gad = CreateGadget(TEXT_KIND, gad, &currentTilesetNewGadget,
    GTTX_Text, "N/A",
    GTTX_Border, TRUE,
    TAG_END);
  data->tilesetNameGadget = gad;
	
  gad = CreateGadget(BUTTON_KIND, gad, &chooseTilesetNewGadget, TAG_END);

  gad = CreateGadget(SCROLLER_KIND, gad, &tilesetScrollNewGadget,
    PGA_Freedom, LORIENT_VERT,
    GA_Disabled, TRUE,
    TAG_END);
	
  gad = CreateGadget(STRING_KIND, gad, &mapNameNewGadget,
    TAG_END);
  data->mapNameGadget = gad;

  gad = CreateGadget(TEXT_KIND, gad, &songNameNewGadget,
    GTTX_Text, "N/A",
    GTTX_Border, TRUE,
    TAG_END);
  data->songNameGadget = gad;

  gad = CreateGadget(BUTTON_KIND, gad, &songChangeNewGadget, TAG_END);
  gad = CreateGadget(BUTTON_KIND, gad, &songClearNewGadget, TAG_END);

  gad = CreateGadget(BUTTON_KIND, gad, &mapLeftNewGadget,
    GA_Disabled, TRUE,
    TAG_END);
  data->leftGadget = gad;

  gad = CreateGadget(BUTTON_KIND, gad, &mapRightNewGadget,
    GA_Disabled, TRUE,
    TAG_END);
  data->rightGadget = gad;

  gad = CreateGadget(BUTTON_KIND, gad, &mapUpNewGadget,
    GA_Disabled, TRUE,
    TAG_END);
  data->upGadget = gad;

  gad = CreateGadget(BUTTON_KIND, gad, &mapDownNewGadget,
    GA_Disabled, TRUE,
    TAG_END);
  data->downGadget = gad;

  gad = CreateGadget(BUTTON_KIND, gad, &entitiesNewGadget, TAG_END);

  if(gad) {
    data->gadgets = glist;
  } else {
    data->tilesetNameGadget = NULL;
    data->mapNameGadget = NULL;
    data->songNameGadget = NULL;
    data->leftGadget = NULL;
    data->rightGadget = NULL;
    data->upGadget = NULL;
    data->downGadget = NULL;
    FreeGadgets(glist);
  }
}

static void initMapEditorPaletteImages(MapEditorData *data) {
  int top, left, row, col;
  struct Image *i = data->paletteImages;
  UWORD *imageData = data->imageData;

  top = 0;
  for(row = 0; row < TILESET_PALETTE_TILES_HIGH; row++) {
    left = 0;
    for(col = 0; col < TILESET_PALETTE_TILES_ACROSS; col++) {
      i->LeftEdge = left;
      i->TopEdge = top;
      i->Width = 32;
      i->Height = 32;
      i->Depth = 2;
      i->ImageData = imageData;
      i->PlanePick = 0x03;
      i->PlaneOnOff = 0;
      i->NextImage = i + 1;

      i++;
      left += 32;
      imageData += 128;
    }
    top += 32;
  }
  data->paletteImages[31].NextImage = NULL;
}

static void initMapEditorMapImages(MapEditorData *data) {
  int top, left, row, col;
  struct Image *i = data->mapImages;
  UWORD *imageData = data->imageData;

  top = 0;
  for(row = 0; row < MAP_TILES_HIGH; row++) {
    left = 0;
    for(col = 0; col < MAP_TILES_ACROSS; col++) {
      i->LeftEdge = left;
      i->TopEdge = top;
      i->Width = 32;
      i->Height = 32;
      i->Depth = 2;
      i->ImageData = imageData;
      i->PlanePick = 0x03;
      i->PlaneOnOff = 0;
      i->NextImage = i + 1;

      i++;
      left += 32;
    }
    top += 32;
  }
  data->mapImages[89].NextImage = NULL;
}

static void closeAttachedTilesetRequester(MapEditorData *data) {
  if(data->tilesetRequester) {
    closeTilesetRequester(data->tilesetRequester);
    data->tilesetRequester = NULL;
  }
}

static void closeAttachedSongRequester(MapEditorData *data) {
  if(data->songRequester) {
    freeSongRequester(data->songRequester);
    data->songRequester = NULL;
  }
}

static void closeAttachedEntityBrowser(MapEditorData *data) {
  if(data->entityBrowser) {
    freeEntityBrowser(data->entityBrowser);
    data->entityBrowser = NULL;
  }
}

void closeMapEditor(MapEditorData *data) {
  /* TODO: these should all be taken care of by the framework
  closeAttachedTilesetRequester(mapEditor);
  closeAttachedSongRequester(mapEditor);
  closeAttachedEntityBrowser(mapEditor);
  ClearMenuStrip(mapEditor->window);
  CloseWindow(mapEditor->window); */
  FreeGadgets(data->gadgets);
  FreeMem(data->imageData, IMAGE_DATA_SIZE);
  free(data->map);
  free(data);
}

void mapEditorDrawEntity(FrameworkWindow *mapEditorWindow, Entity *entity, int entityNum) {
  MapEditorData *data = mapEditorWindow->data;
  if(data->map->tilesetNum) {
    drawEntity(mapEditorWindow->intuitionWindow->RPort, entity, entityNum);
  }
}

static void mapEditorClearTilesetUI(FrameworkWindow *mapEditorWindow) {
  MapEditorData *data = mapEditorWindow->data;
  struct RastPort *rport = mapEditorWindow->intuitionWindow->RPort;

  GT_SetGadgetAttrs(data->tilesetNameGadget, mapEditorWindow->intuitionWindow, NULL,
    GTTX_Text, "N/A",
    TAG_END);

  SetAPen(rport, 0);
  SetDrMd(rport, JAM1);

  RectFill(rport,
    TILESET_BORDER_LEFT,
    TILESET_BORDER_TOP,
    TILESET_BORDER_LEFT + TILESET_BORDER_WIDTH  - 3,
    TILESET_BORDER_TOP  + TILESET_BORDER_HEIGHT - 3);

  RectFill(rport,
    MAP_BORDER_LEFT,
    MAP_BORDER_TOP,
    MAP_BORDER_LEFT + MAP_BORDER_WIDTH  - 3,
    MAP_BORDER_TOP  + MAP_BORDER_HEIGHT - 3);
}

static void updateMapEditorTitle(FrameworkWindow *mapEditorWindow) {
  MapEditorData *data = mapEditorWindow->data;

  char unsaved = data->saved ? '\0' : '*';
  if(data->mapNum) {
    sprintf(data->title, "Map %d%c", data->mapNum - 1, unsaved);
  } else {
    sprintf(data->title, "Map Editor%c", unsaved);
  }
  SetWindowTitles(mapEditorWindow->intuitionWindow, data->title, (STRPTR)-1);
}

void mapEditorSetSaveStatus(FrameworkWindow *mapEditorWindow, SaveStatus status) {
  MapEditorData *data = mapEditorWindow->data;
  data->saved = status;
  updateMapEditorTitle(mapEditorWindow);
}

void mapEditorRefreshTileset(FrameworkWindow *mapEditorWindow) {
  MapEditorData *data = mapEditorWindow->data;
  ProjectWindowData *parentData = mapEditorWindow->parent->data;
  TilesetPackage *tilesetPackage = NULL/* TODO: fix me parentData->tilesetPackage */;

  if(data->map->tilesetNum) {
    if(data->map->tilesetNum - 1 < tilesetPackage->tilesetPackageFile.tilesetCnt) {
       mapEditorSetTilesetUpdateUI(mapEditorWindow, data->map->tilesetNum - 1);
    } else {
      mapEditorClearTilesetUI(mapEditorWindow);
      EasyRequest(mapEditorWindow->intuitionWindow, &tilesetOutOfBoundsEasyStruct, NULL,
        data->map->tilesetNum - 1);
      data->map->tilesetNum = 0;
      mapEditorSetSaveStatus(mapEditorWindow, UNSAVED);
    }
  }
}

static void updateTilesetRequesterChildren(FrameworkWindow *mapEditorWindow) {
  FrameworkWindow *i = mapEditorWindow->children;
  while(i) {
    if(isTilesetRequesterWindow(i)) {
      /* TODO: pass in i, not i->data... */
      refreshTilesetRequesterList(i->data);
    }
    i = i->next;
  }
}

void mapEditorUpdateTileDisplays(FrameworkWindow *mapEditorWindow) {
  updateTilesetRequesterChildren(mapEditorWindow);
  mapEditorRefreshTileset(mapEditorWindow);
}

void mapEditorSetTileset(FrameworkWindow *mapEditorWindow, UWORD tilesetNumber) {
  MapEditorData *data = mapEditorWindow->data;
  data->map->tilesetNum = tilesetNumber + 1;
  mapEditorSetTilesetUpdateUI(mapEditorWindow, tilesetNumber);
  mapEditorSetSaveStatus(mapEditorWindow, UNSAVED);
}

static void mapEditorSetSongUpdateUI(FrameworkWindow *mapEditorWindow, UWORD songNumber) {
  MapEditorData *data = mapEditorWindow->data;
  GT_SetGadgetAttrs(data->songNameGadget, mapEditorWindow->intuitionWindow, NULL,
    GTTX_Text, projectDataGetSongName(mapEditorWindow->parent->data, songNumber),
    TAG_END);
}

void mapEditorSetSong(FrameworkWindow *mapEditorWindow, UWORD songNumber) {
  MapEditorData *data = mapEditorWindow->data;
  data->map->songNum = songNumber + 1;
  mapEditorSetSongUpdateUI(mapEditorWindow, songNumber);
  mapEditorSetSaveStatus(mapEditorWindow, UNSAVED);
}

void mapEditorRefreshSong(FrameworkWindow *mapEditorWindow) {
  MapEditorData *data = mapEditorWindow->data;
  if(data->map->songNum) {
     mapEditorSetSongUpdateUI(mapEditorWindow, data->map->songNum - 1);
  }
}

void mapEditorRedrawTile(FrameworkWindow *mapEditorWindow, int row, int col) {
  MapEditorData *data = mapEditorWindow->data;
  if(data->map->tilesetNum) {
    redrawMapTile(mapEditorWindow, row * 10 + col);
  }
}

void mapEditorSetMapNum(FrameworkWindow *mapEditorWindow, UWORD mapNum) {
  MapEditorData *data = mapEditorWindow->data;  

  data->mapNum = mapNum + 1;

  if(mapNum < 16) {
    GT_SetGadgetAttrs(data->upGadget, mapEditorWindow->intuitionWindow, NULL,
      GA_Disabled, TRUE,
      TAG_END);
  } else {
    GT_SetGadgetAttrs(data->upGadget, mapEditorWindow->intuitionWindow, NULL,
      GA_Disabled, FALSE,
      TAG_END);
  }

  if(mapNum >= 112) {
    GT_SetGadgetAttrs(data->downGadget, mapEditorWindow->intuitionWindow, NULL,
      GA_Disabled, TRUE,
      TAG_END);
    } else {
      GT_SetGadgetAttrs(data->downGadget, mapEditorWindow->intuitionWindow, NULL,
        GA_Disabled, FALSE,
        TAG_END);
    }

    if(mapNum % 16 == 0) {
      GT_SetGadgetAttrs(data->leftGadget, mapEditorWindow->intuitionWindow, NULL,
        GA_Disabled, TRUE,
        TAG_END);
    } else {
      GT_SetGadgetAttrs(data->leftGadget, mapEditorWindow->intuitionWindow, NULL,
        GA_Disabled, FALSE,
        TAG_END);
    }

    if(mapNum % 16 == 15) {
      GT_SetGadgetAttrs(data->rightGadget, mapEditorWindow->intuitionWindow, NULL,
        GA_Disabled, TRUE,
        TAG_END);
    } else {
      GT_SetGadgetAttrs(data->rightGadget, mapEditorWindow->intuitionWindow, NULL,
        GA_Disabled, FALSE,
        TAG_END);
    }

    updateMapEditorTitle(mapEditorWindow);
}

static FrameworkWindow *newMapEditor(void) {
  FrameworkWindow *mapEditorWindow;

  MapEditorData *data = malloc(sizeof(MapEditorData));
  if(!data) {
    fprintf(stderr, "newMapEditor: failed to allocate MapEditorData\n");
    goto error;
  }

  createMapEditorGadgets(data);
  if(!data->gadgets) {
    fprintf(stderr, "newMapEditor: failed to create gadgets\n");
    goto error_freeData;
  }
  mapEditorWindowKind.newWindow.FirstGadget = data->gadgets;

  data->imageData = AllocMem(IMAGE_DATA_SIZE, MEMF_CHIP);
  if(!data->imageData) {
    fprintf(stderr, "newMapEditor: failed to allocate image data\n");
    goto error_freeGadgets;
  }
  initMapEditorPaletteImages(data);
  initMapEditorMapImages(data);

  mapEditorWindow = openWindowOnGlobalScreen(&mapEditorWindowKind);
  if(!mapEditorWindow) {
    fprintf(stderr, "newMapEditor: failed to open window\n");
    goto error_freeImageData;
  }

  refreshMapEditor(mapEditorWindow);

  data->tilesetRequester = NULL;
  data->songRequester    = NULL;
  data->entityBrowser    = NULL;
  data->selected         = -1;

  mapEditorWindow->data = data;

  return mapEditorWindow;

error_freeImageData:
  FreeMem(data->imageData, IMAGE_DATA_SIZE);
error_freeGadgets:
  FreeGadgets(data->gadgets);
error_freeData:
  free(data);
error:
    return NULL;
}

FrameworkWindow *newMapEditorNewMap(void) {
  Map *map;
  MapEditorData *data;
  FrameworkWindow *mapEditorWindow;

  map = allocMap();
  if(!map) {
    goto error;
  }

  mapEditorWindow = newMapEditor();
  if(!mapEditorWindow) {
    goto error_freeMap;
  }

  
  data = mapEditorWindow->data;
  data->map = map;
  data->mapNum = 0;

  mapEditorSetSaveStatus(mapEditorWindow, SAVED);

  return mapEditorWindow;

error_freeMap:
  free(map);
error:
  return NULL;
}

FrameworkWindow *newMapEditorWithMap(Map *map, int mapNum) {
  Map *mapCopy;
  FrameworkWindow *mapEditorWindow;
  MapEditorData *data;

  mapCopy = map ? copyMap(map) : allocMap();

  if(!mapCopy) {
    goto error;
  }

  mapEditorWindow = newMapEditor();
  if(!mapEditorWindow) {
    goto error_freeMap;
  }

  data = mapEditorWindow->data;

  GT_SetGadgetAttrs(data->mapNameGadget, mapEditorWindow->intuitionWindow, NULL,
    GTST_String, map->name,
    TAG_END);

  data->map = mapCopy;

  if(data->map->tilesetNum) {
    int i;
    for(i = 0; i < MAP_TILES_WIDE * MAP_TILES_HIGH; i++) {
      mapEditorSetTileTo(mapEditorWindow, i, map->tiles[i]);
    }
    mapEditorSetTilesetUpdateUI(mapEditorWindow, map->tilesetNum - 1);
  }

  if(map->songNum) {
    mapEditorSetSong(mapEditorWindow, map->songNum - 1);
  }

  mapEditorSetMapNum(mapEditorWindow, mapNum);
  mapEditorSetSaveStatus(mapEditorWindow, SAVED);
  return mapEditorWindow;

error_freeMap:
  free(mapCopy);
error:
  return NULL;
}
