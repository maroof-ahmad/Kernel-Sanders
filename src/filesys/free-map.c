#include "filesys/free-map.h"
#include <bitmap.h>
#include <debug.h>
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include <stdio.h>

static struct file *free_map_file;   /*!< Free map file. */
static struct bitmap *free_map;      /*!< Free map, one bit per sector. */

/*! Initializes the free map. */
void free_map_init(void) {
    free_map = bitmap_create(block_size(fs_device) * 4);
    if (free_map == NULL)
        PANIC("bitmap creation failed--file system device is too large");
    bitmap_mark(free_map, DEBUG_SECTOR);
    bitmap_mark(free_map, FREE_MAP_SECTOR);
    bitmap_mark(free_map, ROOT_DIR_SECTOR);
}

/*! Allocates a sector and stores it into sectorp..
    Returns true if successful, false if no sectors were
    available or if the free_map file could not be written. */
bool free_map_allocate(block_sector_t *sectorp) {
    block_sector_t sector = bitmap_scan_and_flip(free_map, 0, 1, false);
    if (sector != BITMAP_ERROR && free_map_file != NULL &&
        !bitmap_write(free_map, free_map_file)) {
        bitmap_set_multiple(free_map, sector, 1, false); 
        sector = BITMAP_ERROR;
    }
    if (sector != BITMAP_ERROR)
        *sectorp = sector;
    return sector != BITMAP_ERROR;
}

/*! Makes sector at SECTOR available for use. */
void free_map_release(block_sector_t sector) {
    ASSERT(bitmap_all(free_map, sector, 1));
    bitmap_set_multiple(free_map, sector, 1, false);
    bitmap_write(free_map, free_map_file);
}

/*! Opens the free map file and reads it from disk. */
void free_map_open(void) {
    free_map_file = file_open(inode_open(FREE_MAP_SECTOR), false);
    if (free_map_file == NULL)
        PANIC("can't open free map");
    if (!bitmap_read(free_map, free_map_file))
        PANIC("can't read free map");
}

/*! Writes the free map to disk and closes the free map file. */
void free_map_close(void) {
    file_close(free_map_file);
}

/*! Creates a new free map file on disk and writes the free map to it. */
void free_map_create(void) {
    /* Create inode. */
    printf("Creating free map\n");
    if (!inode_create(FREE_MAP_SECTOR, bitmap_file_size(free_map)))
        PANIC("free map creation failed");

    /* Write bitmap to file. */
    free_map_file = file_open(inode_open(FREE_MAP_SECTOR), false);
    if (free_map_file == NULL)
        PANIC("can't open free map");
    if (!bitmap_write(free_map, free_map_file))
        PANIC("can't write free map");
}

