#ifndef MULTIDISK_H
#define MULTIDISK_H
#include "partiter.h"

struct muldisk_path {
    char relative_name[FILENAME_MAX];
    uint8_t hdd;
    uint8_t partition;
};

int find_partition(struct part_iter **_iter, int drive, int partition);
int add_fs(struct fs_info *fs, uint8_t disk, uint8_t partition);
struct fs_info *get_fs(uint8_t disk, uint8_t partition);
struct muldisk_path* muldisk_path_parse(const char *path);

#endif /* MULTIDISK_H */
