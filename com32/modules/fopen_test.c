/*
 * fopen_test.c - A simple ELF module to test fopen on a different partition.
 *
 *  Created on: Jun 30, 2012
 *      Author: Andre Ericson <de.ericson@gmail.com>
 */

#include <stdio.h>
#include <stdlib.h>
#define FILENAME "(0 2):/andre/hello_mate"

int main(int argc __unused, char **argv __unused)
{
    /* should have this syntax: (hd part):/path/to/file */
    FILE *f = fopen(FILENAME, "rt");
    char buff[50];
    int i = 0;

    if (!f)
        printf("File not found.\n"
                "For multidisk files use (hd part):/path/to/file\n");
    else {
        while ((buff[i++] = fgetc(f)) && i < 50);
        buff[i < 49 ?i : 49] = '\0';

        printf("read %s\n", buff);
    }

    return 0;
}
