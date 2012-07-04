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

