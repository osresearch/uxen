/* Copyright (c) 2013 Bromium Inc.
 * All rights reserved
 * Author: Jacob Gorm Hansen
 *
 * Deletes a disk image, using the storage backend's own API.
 * For .swap this means freeing the snapshot ID and its blocks
 * in the shared database.
*/

#include <err.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "libimg.h"

#if defined(_WIN32)
#include "sys.h"
#include <windows.h>
DECLARE_PROGNAME;
#endif	/* _WIN32 */

int main(int argc, char **argv)
{
    BlockDriverState *bs;
    const char *img;

#ifdef _WIN32
    setprogname(argv[0]);
    convert_args(argc, argv);
#endif

    if ( argc != 2 ) {
        fprintf(stderr, "usage: %s <protocol>:<image>\n", argv[0]);
        exit(-1);
    }
    img = argv[1];

    ioh_init();
    bh_init();
    aio_init();
    bdrv_init();

    if (!(bs = bdrv_new(""))) {
        fprintf(stderr, "unable to allocate block backend\n");
        return -1;
    }

    if (bdrv_open(bs, img, BDRV_O_RDWR) < 0) {
        fprintf(stderr, "unable to open %s\n", img);
        return -1;
    }

    if (bdrv_remove(bs) < 0) {
        fprintf(stderr, "unable to remove %s\n", img);
        return -1;
    }

    return 0;
}
