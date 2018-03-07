/* TODO: make all public functions take a this argument... */

#include "ProjectWindow.h"

#include <libraries/asl.h>
#include <proto/asl.h>
#include <proto/dos.h>
#include <proto/intuition.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "framework/menubuild.h"
#include "framework/screen.h"

#include "easystructs.h"
#include "entitiesmenu.h"
#include "MapEditor.h"
#include "mapmenu.h"
#include "musicmenu.h"
#include "Project.h"
#include "projectmenu.h"
#include "ProjectWindowData.h"

static MenuSpec mainMenuSpec[] = {
  { "Project",  &projectMenuSpec  },
  { "Maps",     &mapMenuSpec      },
  { "Entities", &entitiesMenuSpec },
  { "Music",    &musicMenuSpec    },
  END_MENUS
};

#define REVERT_PROJECT_MENU_ITEM (SHIFTMENU(0) | SHIFTITEM(6))

/* TODO: get rid of me if you can */
static FrameworkWindow *projectWindow = NULL;

static void onClose(FrameworkWindow *projectWindow) {
  freeProjectData(projectWindow->data);
}

static WindowKind projectWindowKind = {
  {
    0,0, -1, -1,
    0xFF,0xFF,
    MENUPICK,
    BORDERLESS|BACKDROP,
    NULL,
    NULL,
    "Project",
    NULL,
    NULL,
    -1, -1,
    0xFFFF,0xFFFF,
    CUSTOMSCREEN
  },
  (MenuSpec*)        NULL, /* set me later */
  (RefreshFunction)  NULL,
  (CanCloseFunction) NULL, /* TODO: check for saved/unsaved here */
  (CloseFunction)    onClose
};

FrameworkWindow *getProjectWindow(void) {
  return projectWindow;
}

static void makeWindowFullScreen(void) {
  struct NewWindow *newWindow = &projectWindowKind.newWindow;
  newWindow->MinWidth  = newWindow->Width  = getScreenWidth();
  newWindow->MinHeight = newWindow->Height = getScreenHeight();
}

FrameworkWindow *openProjectWindow(void) {
  ProjectWindowData *data;

  if(projectWindow) {
    fprintf(stderr, "openProjectWindow: cannot be called when project window already exists\n");
    goto error;
  }

  projectWindowKind.menuSpec = mainMenuSpec;

  makeWindowFullScreen();

  data = createProjectData();
  if(!data) {
    fprintf(stderr, "openProjectWindow: failed to allocate data\n");
    goto error;
  }

  projectWindow = openWindowOnGlobalScreen(&projectWindowKind);
  if(!projectWindow) {
    fprintf(stderr, "openProjectWindow: failed to open window!\n");
    goto error_freeData;
  }

  projectWindow->data = data;

  ActivateWindow(projectWindow->intuitionWindow);

  return projectWindow;

error_freeData:
  freeProjectData(data);
error:
  return NULL;
}

static void setProjectFilename(FrameworkWindow *projectWindow, char *filename) {
  setProjectDataFilename(projectWindow->data, filename);
  OnMenu(projectWindow->intuitionWindow, REVERT_PROJECT_MENU_ITEM);
}

static void clearProjectFilename(FrameworkWindow *projectWindow) {
  clearProjectDataFilename(projectWindow->data);
  OffMenu(projectWindow->intuitionWindow, REVERT_PROJECT_MENU_ITEM);
}

static BOOL saveProjectToAsl(FrameworkWindow *projectWindow, char *dir, char *file) {
  size_t bufferLen = strlen(dir) + strlen(file) + 2;
  char *buffer = malloc(bufferLen);

  if(!buffer) {
    fprintf(
      stderr,
      "saveProjectToAsl: failed to allocate buffer "
      "(dir: %s) (file: %s)\n",
      dir  ? dir  : "NULL",
      file ? file : "NULL");
    goto error;
  }

  strcpy(buffer, dir);
  if(!AddPart(buffer, file, (ULONG)bufferLen)) {
    fprintf(
      stderr,
      "saveProjectToAsl: failed to add part "
      "(buffer: %s) (file: %s) (len: %d)\n",
      buffer ? buffer : "NULL",
      file   ? file   : "NULL",
      bufferLen);
    goto error_freeBuffer;
  }

  if(!projectDataSaveProjectToFile(projectWindow->data, buffer)) {
    EasyRequest(
      projectWindow->intuitionWindow,
      &projectSaveFailEasyStruct,
      NULL,
      buffer);
    goto error_freeBuffer;
  }
  setProjectFilename(projectWindow, buffer);

freeBuffer:
  free(buffer);
done:
  return TRUE;

error_freeBuffer:
  free(buffer);
error:
  return FALSE;
}

BOOL saveProjectAs(FrameworkWindow *projectWindow) {
  BOOL result;
  struct FileRequester *request; 

  request = AllocAslRequestTags(ASL_FileRequest,
    ASL_Hail, "Save Project As",
    ASL_Window, projectWindow->intuitionWindow,
    ASL_FuncFlags, FILF_SAVE,
    TAG_END);
  if(!request) {
    fprintf(stderr, "saveProjectAs: couldn't allocate asl request tags\n");
    goto error;
  }

  result = AslRequest(request, NULL);
  if(result) {
    result = saveProjectToAsl(projectWindow, request->rf_Dir, request->rf_File);
  }

  FreeAslRequest(request);
done:
  return result;
error:
  return FALSE;
}

BOOL saveProject(FrameworkWindow *projectWindow) {
  ProjectWindowData *data = projectWindow->data;
  char *filename = projectDataGetFilename(data);

  if(filename) {
    if(!projectDataSaveProjectToFile(data, filename)) {
      EasyRequest(
        projectWindow->intuitionWindow,
        &projectSaveFailEasyStruct,
        NULL,
        filename);
      goto error;
    }
  } else {
    return saveProjectAs(projectWindow);
  }

done:
  return TRUE;

error:
  return FALSE;
}

static BOOL unsavedProjectAlert(FrameworkWindow *projectWindow) {
  int response;

  response = EasyRequest(
    projectWindow->intuitionWindow,
    &unsavedProjectAlertEasyStruct,
    NULL);

  switch(response) {
    case 0: return FALSE;
    case 1: return saveProject(projectWindow);
    case 2: return TRUE;
    default:
      fprintf(stderr, "unsavedProjectAlert: unknown response %d\n", response);
      goto error;
  }

error:
  return FALSE;
}

static BOOL ensureProjectSaved(FrameworkWindow *projectWindow) {
  return (BOOL)(projectDataIsSaved(projectWindow->data) || unsavedProjectAlert(projectWindow));
}

static BOOL ensureEverythingSaved(FrameworkWindow *projectWindow) {
  return (BOOL)(ensureMapEditorsSaved() && ensureProjectSaved(projectWindow));
}

static BOOL loadTilesetPackageFromFile(FrameworkWindow *projectWindow, char *filename) {
  if(!projectDataLoadTilesetPackage(projectWindow->data, filename)) {
    EasyRequest(
      projectWindow->intuitionWindow,
      &tilesetPackageLoadFailEasyStruct,
      NULL,
      filename);
    goto error;
  }

done:
  return TRUE;

error:
  return FALSE;
}

static void openProjectFromFile(FrameworkWindow *projectWindow, char *filename) {
  switch(projectDataLoadProjectFromFile(projectWindow->data, filename)) {
    case PROJECT_LOAD_OK:
      break;
    case PROJECT_LOAD_OK_TILESET_ERROR:
      EasyRequest(
        projectWindow->intuitionWindow,
        &tilesetPackageLoadFailEasyStruct,
        NULL,
        projectDataGetTilesetPath(projectWindow->data));
      break;
    case PROJECT_LOAD_ERROR:
      EasyRequest(
        projectWindow->intuitionWindow,
        &projectLoadFailEasyStruct,
        NULL,
        filename);
      goto error;
  }

  setProjectFilename(projectWindow, filename);

done:
    return;

error:
    return;
}

static void openProjectFromAsl(FrameworkWindow *projectWindow, char *dir, char *file) {
  size_t bufferLen = strlen(dir) + strlen(file) + 2;
  char *buffer = malloc(bufferLen);

  if(!buffer) {
    fprintf(
      stderr,
      "openProjectFromAsl: failed to allocate buffer "
      "(dir: %s) (file: %s)\n",
      dir  ? dir  : "NULL",
      file ? file : "NULL");
    goto done;
  }

  strcpy(buffer, dir);
  if(!AddPart(buffer, file, (ULONG)bufferLen)) {
    fprintf(
      stderr,
      "openProjectFromAsl: failed to add part "
      "(buffer: %s) (file: %s) (len: %d)\n",
      buffer ? buffer : "NULL",
      file   ? file   : "NULL",
      bufferLen);
    goto freeBuffer;
  }

  openProjectFromFile(projectWindow, buffer);

freeBuffer:
    free(buffer);
done:
    return;
}

void newProject(FrameworkWindow *projectWindow) {
  if(ensureEverythingSaved(projectWindow)) {
    projectDataInitProject(projectWindow->data);
    clearProjectFilename(projectWindow);
  }
}

void openProject(FrameworkWindow *projectWindow) {
  struct FileRequester *request;

  if(!ensureEverythingSaved(projectWindow)) {
    goto done;
  }

  request = AllocAslRequestTags(ASL_FileRequest,
    ASL_Hail, "Open Project",
    ASL_Window, projectWindow->intuitionWindow,
    TAG_END);
  if(!request) {
    goto done;
  }

  if(AslRequest(request, NULL)) {
    openProjectFromAsl(projectWindow, request->rf_Dir, request->rf_File);
  }

  FreeAslRequest(request);
done:
  return;
error:
  return;
}

static int confirmRevertProject(FrameworkWindow *projectWindow) {
  return EasyRequest(
    projectWindow->intuitionWindow,
    &confirmRevertProjectEasyStruct,
    NULL);
}

void revertProject(FrameworkWindow *projectWindow) {
  if(!confirmRevertProject(projectWindow)) {
    goto done;
  }

  openProjectFromFile(projectWindow, projectDataGetFilename(projectWindow->data));

done:
  return;
}

static void updateAllTileDisplays(void) {
/*  MapEditor *i = mapEditorSetFirst();
  while(i) {
    if(i->tilesetRequester) {
      refreshTilesetRequesterList(i->tilesetRequester);
    }
    mapEditorRefreshTileset(i);
    i = i->next;
  } */
/* TODO: fix me */
}

static BOOL loadTilesetPackageFromAsl(char *dir, char *file) {
  char buffer[TILESET_PACKAGE_PATH_SIZE];

  if(strlen(dir) >= sizeof(buffer)) {
    fprintf(stderr, "loadTilesetPackageFromAsl: dir %s file %s doesn't fit in buffer\n", dir, file);
    goto error;
  }

  strcpy(buffer, dir);
  if(!AddPart(buffer, file, TILESET_PACKAGE_PATH_SIZE)) {
    fprintf(stderr, "loadTilesetPackageFromAsl: dir %s file %s doesn't fit in buffer\n", dir, file);
    goto error;
  }

  return loadTilesetPackageFromFile(projectWindow, buffer);

error:
  return FALSE;
}

void selectTilesetPackage(FrameworkWindow *projectWindow) {
  struct FileRequester *request = AllocAslRequestTags(ASL_FileRequest,
    ASL_Hail, "Select Tileset Package",
    ASL_Window, projectWindow->intuitionWindow,
    TAG_END);
  if(!request) {
    fprintf(stderr, "selectTilesetPackage: failed to allocate requester\n");
    goto error;
  }

  if(AslRequest(request, NULL)) {
    if(loadTilesetPackageFromAsl(request->rf_Dir, request->rf_File)) {
      updateAllTileDisplays();
    }
  }

  FreeAslRequest(request);

done:
  return;
error:
  return;
}

void quit(FrameworkWindow *projectWindow) {
  if(ensureEverythingSaved(projectWindow)) {
    /* TODO: double check if this is the best way to do this. */
    projectWindow->closed = TRUE;
  }
}

/* TODO: this is weird */
/* TODO: just pass in data? */
/* static BOOL currentProjectSaveNewMap(FrameworkWindow *projectWindow, Map *map, int mapNum) {
  ProjectWindowData *data = projectWindow->data;

  Map *mapCopy = copyMap(map);
  if(!mapCopy) {
    fprintf(stderr, "currentProjectSaveNewMap: couldn't allocate map copy\n");
    return FALSE;
  }
  data->project.mapCnt++;
  data->project.maps[mapNum] = mapCopy;
  return TRUE;
} */

/* TODO: just take data? */
/* static void currentProjectOverwriteMap(FrameworkWindow *projectWindow, Map *map, int mapNum) {
  ProjectWindowData *data = projectWindow->data;
  overwriteMap(map, data->project.maps[mapNum]);
} */

/* TODO: just pass in data? */
/* TODO: this is weird */
/* static void updateCurrentProjectMapName(FrameworkWindow *projectWindow, int mapNum, Map *map) {
  ProjectWindowData *data = projectWindow->data;
  updateProjectMapName(&data->project, mapNum, map);
} */

static int confirmCreateMap(FrameworkWindow *projectWindow, int mapNum) {
  return EasyRequest(
    projectWindow->intuitionWindow,
    &confirmCreateMapEasyStruct,
    NULL,
    mapNum);
}

/* TODO: I kind of feel that this belongs in MapEditor, maybe? */
BOOL openMapNum(FrameworkWindow *projectWindow, int mapNum) {
  MapEditor *mapEditor;

  if(!projectDataHasMap(projectWindow->data, mapNum)) {
    if(!confirmCreateMap(projectWindow, mapNum)) {
      goto error;
    }

    if(!projectDataCreateMap(projectWindow->data, mapNum)) {
      fprintf(stderr, "openMapNum: failed to create map\n");
      goto error;
    }
  }

  mapEditor = newMapEditorWithMap(projectDataGetMap(projectWindow->data, mapNum), mapNum);
  if(!mapEditor) {
    fprintf(stderr, "openMapNum: failed to create new map editor\n");
    goto error;
  }

  addToMapEditorSet(mapEditor);
  /* TODO: fix me */
  /* addWindowToSet(mapEditor->window); */
  enableMapRevert(mapEditor);

done:
  return TRUE;

error:
  return FALSE;
}

void openMap(FrameworkWindow *projectWindow) {
  MapEditor *mapEditor;

  int selected = openMapRequester(projectWindow);
  if(!selected) {
    return;
  }
  selected--;

  mapEditor = findMapEditor(selected);
  if(mapEditor) {
    WindowToFront(mapEditor->window->intuitionWindow);
  } else {
    openMapNum(projectWindow, selected);
  }
}
