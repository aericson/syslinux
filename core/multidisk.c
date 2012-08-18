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
#include "fs.h"
#include "multidisk.h"

/* 0x80 - 0xFF
 * BIOS limitation */
#define DISKS_MAX 128

struct part_node {
    int partition;
    struct fs_info *fs;
    struct part_node *next;
};

struct queue_head {
    struct part_node *first;
    struct part_node *last;
};

struct queue_head *parts_info[DISKS_MAX];

int add_fs(struct fs_info *fs, uint8_t disk, uint8_t partition)
{
    struct queue_head *head = parts_info[disk];
    struct part_node *node;

    node = malloc(sizeof(struct part_node));
    if (!node)
        return -1;

    node->fs = fs;
    node->next = NULL;
    node->partition = partition;
    if (!head) {
        head = malloc(sizeof(struct queue_head));
        if(!head)
            goto bail;
        head->first = head->last = node;
        parts_info[disk] = head;
        return 0;
    }
    head->last->next = node;
    head->last = node;
    return 0;

bail:
    free(node);
    return -1;
}

struct fs_info *get_fs(uint8_t disk, uint8_t partition)
{
    struct part_node *i;

    for (i = parts_info[disk]->first; i; i = i->next) {
        if (i->partition == partition)
            return i->fs;
    }
    return NULL;
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

static uint8_t charhex_to_hex(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 0xa;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 0xa;
    return 0xff;
}

struct muldisk_path* hdd_part_from_mdpath(const char *path)
{
    struct muldisk_path *mpath;
    struct disk_dos_mbr *mbr = NULL;
    struct disk_info diskinfo;
    const char *c = path;
    char buff[4];
    uint8_t hdd = 0;
    uint8_t partition = 0;
    uint32_t mbr_sig = 0;
    int disk;
    int i = 0;
    int mult = 1;

    mpath = malloc(sizeof *mpath);
    if (!mpath)
        return NULL;

    if (path[1] == 'h' && path[2] == 'd') {
        c += 2;

        /* get hdd number */
        for (++c; *c && *c != ','; ++c)
            buff[i++] = *c;
        if (!*c)
            goto bail;
        buff[i] = '\0';

        /* str to uint8_t */
        while (i--) {
            if (buff[i] < '0' || buff[i] > '9')
                goto bail;
            hdd += (buff[i] - 48) * mult;
            mult *= 10;
        }
    } else if (!strncmp(path + 1, "mbr:0x", 6)) {
        c += 6;

        /* get mbr sig */
        for (++c; *c && *c != ','; ++c) {
            mbr_sig = mbr_sig << 4;
            i = charhex_to_hex(*c);
            if (i == 0xff)
                goto bail;
            mbr_sig += i;
        }

        /* find hdd from mbr */
        for (disk = 0x80; disk < 0xff; disk++) {
            if (disk_get_params(disk, &diskinfo))
                continue;
            if (!(mbr = disk_read_sectors(&diskinfo, 0, 1)))
                continue;
            if (mbr->sig != disk_mbr_sig_magic)
                continue;
            if (mbr_sig == mbr->disk_sig) {
                hdd = disk - 0x80;
                free(mbr);
                break;
            }
            free(mbr);
        }
        if (disk == 0xff)
            goto bail;

    } else {
        goto bail;
    }
    /* get partition number */
    i = 0;
    for (++c; *c && *c != ')'; ++c)
        buff[i++] = *c;
    if (!*c)
        goto bail;
    buff[i] = '\0';

    /* str to uint8_t */
    mult = 1;
    while (i--) {
        if (buff[i] < '0' || buff[i] > '9')
            goto bail;
        partition += (buff[i] - 48) * mult;
        mult *= 10;
    }
    mpath->partition = partition;
    mpath->hdd = hdd;
    i = 0;
    /* c was on ')' jump it and stop at beginning of path */
    for (c++; *c; c++)
        mpath->relative_name[i++] = *c;
    mpath->relative_name[i] = '\0';
    return mpath;

bail:
    free(mpath);
    return NULL;
}
