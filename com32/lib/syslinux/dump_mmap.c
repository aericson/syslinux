/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 2007 H. Peter Anvin - All Rights Reserved
 *
 *   Permission is hereby granted, free of charge, to any person
 *   obtaining a copy of this software and associated documentation
 *   files (the "Software"), to deal in the Software without
 *   restriction, including without limitation the rights to use,
 *   copy, modify, merge, publish, distribute, sublicense, and/or
 *   sell copies of the Software, and to permit persons to whom
 *   the Software is furnished to do so, subject to the following
 *   conditions:
 *   
 *   The above copyright notice and this permission notice shall
 *   be included in all copies or substantial portions of the Software.
 *   
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *   OTHER DEALINGS IN THE SOFTWARE.
 *
 * ----------------------------------------------------------------------- */

/*
 * dump_mmap.c
 *
 * Writes a syslinux_memmap out to a specified file.  This is
 * intended for debugging.
 */

#include <stdio.h>
#include <syslinux/movebits.h>

void syslinux_dump_memmap(FILE *file, struct syslinux_memmap *memmap)
{
  fprintf(file, "%10s %10s %10s\n"
	  "--------------------------------\n",
	  "Start", "Length", "Type");
  while (memmap->type != SMT_END) {
    fprintf(file, "0x%08x 0x%08x %10d\n", memmap->start,
	    memmap->next->start - memmap->start, memmap->type);
    memmap = memmap->next;
  }
}