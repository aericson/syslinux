#include <stdio.h>
#include <string.h>
#include <sys/dirent.h>
#include "fs.h"
#include "core.h"
#include "multidisk.h"

/* 
 * Open a directory
 */
DIR *opendir(const char *path)
{
    int rv;
    struct file *file;
    struct muldisk_path *mul_path;

    if (path[0] == '(') {

        mul_path = muldisk_path_parse(path);
        if (!mul_path)
            return NULL;

        rv = searchdir(mul_path->relative_name, get_fs_info(mul_path->hdd, mul_path->partition));
        free(mul_path);

        if (rv < 0)
            return NULL;

        file = handle_to_file(rv);

        if (file->inode->mode != DT_DIR) {
            _close_file(file);
            return NULL;
        }

        return (DIR *)file;
    }

    rv = searchdir(path, NULL);
    if (rv < 0)
        return NULL;

    file = handle_to_file(rv);

    if (file->inode->mode != DT_DIR) {
        _close_file(file);
        return NULL;
    }

    return (DIR *)file;
}

/*
 * Read one directory entry at one time. 
 */
struct dirent *readdir(DIR *dir)
{
    static struct dirent buf;
    struct file *dd_dir = (struct file *)dir;
    int rv = -1;

    if (dd_dir) {
        if (dd_dir->fs->fs_ops->readdir) {
	    rv = dd_dir->fs->fs_ops->readdir(dd_dir, &buf);
        }
    }

    return rv < 0 ? NULL : &buf;
}

/*
 * Close a directory
 */
int closedir(DIR *dir)
{
    struct file *dd_dir = (struct file *)dir;
    _close_file(dd_dir);
    return 0;
}


