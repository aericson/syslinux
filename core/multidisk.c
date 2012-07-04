/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2012 Andre Ericson <de.ericson@gmail.com>
 *
 *   Some parts borrowed from chain.c32:
 *
 *   Copyright 2003-2009 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009 Intel Corporation; author: H. Peter Anvin
 *
 *   This file is part of Syslinux, and is made available under
 *   the terms of the GNU General Public License version 2.
 *
 * ----------------------------------------------------------------------- */

#include <unistd.h>
#include <string.h>
#include "core.h"
#include "disk.h"
#include "partiter.h"

void do_magic(void *buff)
{

}

int find_partition(struct part_iter **_iter, int drive, int partition)
{
    struct part_iter *iter = NULL;
    struct disk_info diskinfo;

    if (disk_get_params(drive, &diskinfo))
        goto bail;
    if (!(iter = pi_begin(&diskinfo, 0)))
        goto bail;
    do {
        if (iter->index == partition)
            break;
    } while (!pi_next(&iter));
    if (iter->status) {
        printf("Request disk / partition combination not found.\n");
        goto bail;
    }
    dprintf("found 0x%llx at idex: %i and partition %i\n", iter->start_lba, iter->index, partition);

    *_iter = iter;

    return 0;
bail:
    pi_del(&iter);
    return -1;
}

