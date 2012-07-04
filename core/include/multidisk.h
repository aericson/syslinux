#ifndef MULTIDISK_H
#define MULTIDISK_H
#include "partiter.h"

void do_magic(void *);
int find_partition(struct part_iter **_iter, int drive, int partition);
int add_fs(struct fs_info *fs, uint8_t disk, uint8_t partition);
struct fs_info *get_fs(uint8_t disk, uint8_t partition);

#endif /* MULTIDISK_H */
