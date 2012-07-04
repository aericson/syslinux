#include <unistd.h>
#include <core.h>
#include "fs.h"

int generic_chdir_start(struct fs_info *fp)
{
    if (fp)
        return multidisk_chdir("/", fp);
	return chdir(CurrentDirName);
}
