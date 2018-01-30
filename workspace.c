#include "workspace.h"

#include <libraries/asl.h>
#include <proto/asl.h>

#include <intuition/intuition.h>
#include <proto/intuition.h>

#include <libraries/gadtools.h>
#include <proto/gadtools.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "framework/runstate.h"
#include "framework/windowset.h"

#include "currentproject.h"
#include "currenttiles.h"
#include "easystructs.h"
#include "globals.h"
#include "project.h"
#include "MapEditor.h"
#include "mapeditorset.h"
#include "MapRequester.h"
#include "ProjectWindow.h"

static int ensureEverythingSaved(void) {
    return ensureMapEditorsSaved() && ensureProjectSaved();
}

static void updateAllTileDisplays(void) {
    MapEditor *i = mapEditorSetFirst();
    while(i) {
        if(i->tilesetRequester) {
            refreshTilesetRequesterList(i->tilesetRequester);
        }
        mapEditorRefreshTileset(i);
        i = i->next;
    }
}

void refreshAllSongDisplays(void) {
    MapEditor *i = mapEditorSetFirst();
    while(i) {
        if(i->songRequester) {
            GT_RefreshWindow(i->songRequester->window, NULL);
        }
        mapEditorRefreshSong(i);
        i = i->next;
    }
}

void refreshAllEntityBrowsers(void) {
    MapEditor *i = mapEditorSetFirst();
    while(i) {
        if(i->entityBrowser) {
            GT_RefreshWindow(i->entityBrowser->window, NULL);
        }
        i = i->next;
    }
}

void openProject(void) {
    struct FileRequester *request;

    if(!ensureEverythingSaved()) {
        goto done;
    }

    request = AllocAslRequestTags(ASL_FileRequest,
        ASL_Hail, "Open Project",
        ASL_Window, getProjectWindow(),
        TAG_END);
    if(!request) {
        goto done;
    }

    if(AslRequest(request, NULL)) {
        openProjectFromAsl(request->rf_Dir, request->rf_File);
    }

    FreeAslRequest(request);
done:
    return;
}

void newProject(void) {
    if(ensureEverythingSaved()) {
        clearProject();
        initCurrentProject();
        setProjectFilename(NULL);
    }
}

static int confirmRevertProject(void) {
    return EasyRequest(
        getProjectWindow(),
        &confirmRevertProjectEasyStruct,
        NULL);
}

void revertProject(void) {
    if(!confirmRevertProject()) {
        goto done;
    }

    openProjectFromFile(getProjectFilename());

done:
    return;
}

void selectTilesetPackage(void) {
    struct FileRequester *request = AllocAslRequestTags(ASL_FileRequest,
        ASL_Hail, "Select Tileset Package",
        ASL_Window, getProjectWindow(),
        TAG_END);
    if(!request) {
        fprintf(stderr, "selectTilesetPackage: failed to allocate requester\n");
        goto done;
    }

    if(AslRequest(request, NULL)) {
        if(loadTilesetPackageFromAsl(request->rf_Dir, request->rf_File)) {
            updateAllTileDisplays();
        }
    }

    FreeAslRequest(request);

done:
    return;
}

void quit(void) {
    if(ensureEverythingSaved()) {
        stopRunning();
    }
}

void newMap(void) {
    MapEditor *mapEditor = newMapEditorNewMap();
    if(!mapEditor) {
        fprintf(stderr, "newMap: failed to create mapEditor\n");
        return;
    }
    addToMapEditorSet(mapEditor);
    /* TODO: fix me */
    /*addWindowToSet(mapEditor->window);*/
}

static int confirmCreateMap(int mapNum) {
    return EasyRequest(
        getProjectWindow(),
        &confirmCreateMapEasyStruct,
        NULL,
        mapNum);
}

int openMapNum(int mapNum) {
    MapEditor *mapEditor;

    if(!currentProjectHasMap(mapNum)) {
        if(!confirmCreateMap(mapNum)) {
            return 0;
        }

        if(!currentProjectCreateMap(mapNum)) {
            fprintf(stderr, "openMapNum: failed to create map\n");
            return 0;
        }
    }

    mapEditor = newMapEditorWithMap(currentProjectMap(mapNum), mapNum);
    if(!mapEditor) {
        fprintf(stderr, "openMapNum: failed to create new map editor\n");
        return 0;
    }

    addToMapEditorSet(mapEditor);
    /* TODO: fix me */
    /* addWindowToSet(mapEditor->window); */
    enableMapRevert(mapEditor);
    return 1;
}

void openMap(void) {
    MapEditor *mapEditor;

    int selected = openMapRequester();
    if(!selected) {
        return;
    }

    mapEditor = findMapEditor(selected - 1);
    if(mapEditor) {
        WindowToFront(mapEditor->window);
    } else {
        openMapNum(selected - 1);
    }
}

int saveMapAs(MapEditor *mapEditor) {
    int selected = saveMapRequester(mapEditor);
    if(!selected) {
        return 0;
    }

    if(!currentProjectHasMap(selected - 1)) {
        if(!currentProjectSaveNewMap(mapEditor->map, selected - 1)) {
            fprintf(stderr, "saveMapAs: failed to save map\n");
            return 0;
        }
    } else {
        int response = EasyRequest(
            mapEditor->window,
            &saveIntoFullSlotEasyStruct,
            NULL,
            selected - 1, currentProjectGetMapName(selected - 1));
        if(response) {
            currentProjectOverwriteMap(mapEditor->map, selected - 1);
        } else {
            return 0;
        }
    }

    mapEditorSetMapNum(mapEditor, selected - 1);
    enableMapRevert(mapEditor);

    mapEditorSetSaveStatus(mapEditor, SAVED);

    updateCurrentProjectMapName(selected - 1, mapEditor->map);

    return 1;
}

int saveMap(MapEditor *mapEditor) {
    if(!mapEditor->mapNum) {
        return saveMapAs(mapEditor);
    } else {
        currentProjectOverwriteMap(mapEditor->map, mapEditor->mapNum - 1);
        /* TODO: this is what sets the saved status, but that feels fragile */
        updateCurrentProjectMapName(mapEditor->mapNum - 1, mapEditor->map);
        mapEditorSetSaveStatus(mapEditor, SAVED);
        return 1;
    }
}

int unsavedMapEditorAlert(MapEditor *mapEditor) {
    int response;

    if(mapEditor->mapNum) {
        response = EasyRequest(
            mapEditor->window,
            &unsavedMapAlertEasyStructWithNum,
            NULL,
            mapEditor->mapNum - 1, mapEditor->map->name);
    } else {
        response = EasyRequest(
            mapEditor->window,
            &unsavedMapAlertEasyStructNoNum,
            NULL,
            mapEditor->map->name);
    }

    switch(response) {
        case 0: return 0;                  /* cancel */
        case 1: return saveMap(mapEditor); /* save */
        case 2: return 1;                  /* don't save */
        default:
            fprintf(stderr, "unsavedMapEditorAlert: unknown response %d\n", response);
            return 0;
    }
}