#ifndef MULTIDISK_H
#define MULTIDISK_H
#include "partiter.h"

void do_magic(void *);
int find_partition(struct part_iter **, int, int);

#endif /* MULTIDISK_H */
