#include "currenttiles.h"

#include <stdio.h>
#include <strings.h>

#include <dos/dos.h>
#include <proto/dos.h>

#include <intuition/intuition.h>
#include <proto/intuition.h>

#include "easystructs.h"
#include "globals.h"
#include "ProjectWindow.h"
#include "TilesetPackage.h"

int loadTilesetPackageFromFile(char *file) {
    TilesetPackage *newTilesetPackage;

    newTilesetPackage = tilesetPackageLoadFromFile(file);
    if(!newTilesetPackage) {
        EasyRequest(
            getProjectWindow(),
            &tilesetPackageLoadFailEasyStruct,
            NULL,
            file);
        goto error;
    }
    freeTilesetPackage(tilesetPackage);
    tilesetPackage = newTilesetPackage;
    strcpy(project.tilesetPackagePath, file);

    return 1;

error:
    return 0;
}

int loadTilesetPackageFromAsl(char *dir, char *file) {
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

    return loadTilesetPackageFromFile(buffer);

error:
    return 0;
}
