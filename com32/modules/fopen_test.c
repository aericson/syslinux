/*
 * fopen_test.c - A simple ELF module to test fopen on a different partition.
 *
 *  Created on: Jun 30, 2012
 *      Author: Andre Ericson <de.ericson@gmail.com>
 */
#include <stdio.h>
#include <stdlib.h>
#define FILENAME "(0 2):/andre/hello_mate"

/* should have this syntax: (hd part):/path/to/file */
const char *paths [] = {
    "(0 2):/andre/fat_info",
    "(0 3):/andre/ntfs_info",
    NULL
};

int main(int argc __unused, char **argv __unused)
{
    FILE *f;
    char buff[50];
    int i;
    const char **c;

    for (c = paths; *c; c++) {
        f = fopen(*c, "rt");
        i = 0;
        printf("Reading file: %s, content:\n", *c);
        if (!f)
            printf("File not found.\n"
                    "For multidisk files use (hd part):/path/to/file\n");
        else {
            while ((buff[i++] = fgetc(f)) && i < 50);
            buff[i < 49 ? i : 49] = '\0';

            printf("read: %s\n", buff);
        }
    }
    return 0;
}
