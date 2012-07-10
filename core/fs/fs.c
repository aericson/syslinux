#include <sys/file.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <dprintf.h>
#include "core.h"
#include "dev.h"
#include "fs.h"
#include "multidisk.h"
#include "cache.h"

char *PATH;

/* The currently mounted filesystem */
struct fs_info *this_fs = NULL;		/* Root filesystem */

/* Actual file structures (we don't have malloc yet...) */
struct file files[MAX_OPEN];

/* Symlink hard limits */
#define MAX_SYMLINK_CNT	20
#define MAX_SYMLINK_BUF 4096

/*
 * Get a new inode structure
 */
struct inode *alloc_inode(struct fs_info *fs, uint32_t ino, size_t data)
{
    struct inode *inode = zalloc(sizeof(struct inode) + data);
    if (inode) {
	inode->fs = fs;
	inode->ino = ino;
	inode->refcnt = 1;
    }
    return inode;
}

extern const struct fs_ops vfat_fs_ops;
extern const struct fs_ops ext2_fs_ops;
extern const struct fs_ops btrfs_fs_ops;
extern const struct fs_ops ntfs_fs_ops;

const struct fs_ops *fs_ops_array [4] = {
    &vfat_fs_ops,
    &ext2_fs_ops,
    /* TODO: add btrfs */
    &ntfs_fs_ops,
    NULL
};

/*
 * Free a refcounted inode
 */
void put_inode(struct inode *inode)
{
    while (inode && --inode->refcnt == 0) {
	struct inode *dead = inode;
	inode = inode->parent;
	if (dead->name)
	    free((char *)dead->name);
	free(dead);
    }
}

/*
 * Get an empty file structure
 */
static struct file *alloc_file(void)
{
    int i;
    struct file *file = files;

    for (i = 0; i < MAX_OPEN; i++) {
	if (!file->fs)
	    return file;
	file++;
    }

    return NULL;
}

/*
 * Close and free a file structure
 */
static inline void free_file(struct file *file)
{
    memset(file, 0, sizeof *file);
}

void _close_file(struct file *file)
{
    if (file->fs)
	file->fs->fs_ops->close_file(file);
    free_file(file);
}

/*
 * Find and open the configuration file
 */
int open_config(void)
{
    int fd, handle;
    struct file_info *fp;

    fd = opendev(&__file_dev, NULL, O_RDONLY);
    if (fd < 0)
	return -1;

    fp = &__file_info[fd];

    handle = this_fs->fs_ops->open_config(&fp->i.fd);
    if (handle < 0) {
	close(fd);
	errno = ENOENT;
	return -1;
    }

    fp->i.offset = 0;
    fp->i.nbytes = 0;

    return fd;
}

void pm_mangle_name(com32sys_t *regs)
{
    const char *src = MK_PTR(regs->ds, regs->esi.w[0]);
    char       *dst = MK_PTR(regs->es, regs->edi.w[0]);

    mangle_name(dst, src, NULL);
}

void mangle_name(char *dst, const char *src, struct fs_info *fp)
{
    if (fp)
        fp->fs_ops->mangle_name(dst, src);
    else
        this_fs->fs_ops->mangle_name(dst, src);
}

void getfssec(com32sys_t *regs)
{
    int sectors;
    bool have_more;
    uint32_t bytes_read;
    char *buf;
    struct file *file;
    uint16_t handle;

    sectors = regs->ecx.w[0];

    handle = regs->esi.w[0];
    file = handle_to_file(handle);

    buf = MK_PTR(regs->es, regs->ebx.w[0]);
    bytes_read = file->fs->fs_ops->getfssec(file, buf, sectors, &have_more);

    /*
     * If we reach EOF, the filesystem driver will have already closed
     * the underlying file... this really should be cleaner.
     */
    if (!have_more) {
	_close_file(file);
        regs->esi.w[0] = 0;
    }

    regs->ecx.l = bytes_read;
}

void getfsbytes(com32sys_t *regs)
{
    int sectors;
    bool have_more;
    uint32_t bytes_read;
    char *buf;
    struct file *file;
    uint16_t handle;

    handle = regs->esi.w[0];
    file = handle_to_file(handle);

    sectors = regs->ecx.w[0] >> SECTOR_SHIFT(file->fs);

    buf = MK_PTR(regs->es, regs->ebx.w[0]);
    bytes_read = file->fs->fs_ops->getfssec(file, buf, sectors, &have_more);

    /*
     * If we reach EOF, the filesystem driver will have already closed
     * the underlying file... this really should be cleaner.
     */
    if (!have_more) {
	_close_file(file);
        regs->esi.w[0] = 0;
    }

    regs->ecx.l = bytes_read;
}

size_t pmapi_read_file(uint16_t *handle, void *buf, size_t sectors)
{
    bool have_more;
    size_t bytes_read;
    struct file *file;

    file = handle_to_file(*handle);
    bytes_read = file->fs->fs_ops->getfssec(file, buf, sectors, &have_more);

    /*
     * If we reach EOF, the filesystem driver will have already closed
     * the underlying file... this really should be cleaner.
     */
    if (!have_more) {
	_close_file(file);
	*handle = 0;
    }

    return bytes_read;
}

struct fs_info *get_fs_info(uint8_t hdd, uint8_t partition)
{
    struct fs_info *fsp;
    uint8_t disk_devno, disk_cdrom;
    sector_t disk_offset;
    uint16_t disk_heads, disk_sectors, maxtransfer;
    struct part_iter *iter = NULL;
    struct disk_info diskinfo;
    const struct fs_ops **ops;
    int blk_shift = -1;
    struct device *dev = NULL;

    fsp = get_fs(hdd, partition - 1);
    if (fsp)
        return fsp;

    fsp = malloc(sizeof(struct fs_info));
    if (!fsp)
        return NULL;

    disk_devno = 0x80 + hdd;


    if (find_partition(&iter, disk_devno, partition)) {
        printf("Failed to get partition\n");
        goto bail;
    }
    else
        dprintf("part_offset 0x%llx\n", iter->start_lba);

    disk_offset = iter->start_lba;

    if (disk_get_params(disk_devno, &diskinfo))
        goto bail;

    disk_heads = diskinfo.head;
    disk_sectors = diskinfo.spt;

    /* force those values for now 
     * TODO: fix this */
    disk_cdrom = 0;
    maxtransfer = 127;

    ops = fs_ops_array;


    /* Default name for the root directory */
    fsp->cwd_name[0] = '/';

    while ((blk_shift < 0) && *ops) {
        /* set up the fs stucture */
        fsp->fs_ops = *ops;

        /*
         * This boldly assumes that we don't mix FS_NODEV filesystems
         * with FS_DEV filesystems...
         */
        if (fsp->fs_ops->fs_flags & FS_NODEV) {
            fsp->fs_dev = NULL;
        } else {
            if (!dev)
            dev = device_init(disk_devno, disk_cdrom, disk_offset,
                      disk_heads, disk_sectors, maxtransfer);
            fsp->fs_dev = dev;
        }
        /* invoke the fs-specific init code */
        blk_shift = fsp->fs_ops->fs_init(fsp);
        ops++;
    }
    if (blk_shift < 0) {
        printf("No valid file system found!\n");
        while (1)
            ;
    }
    add_fs(fsp, hdd, partition - 1);

    /* initialize the cache */
    if (fsp->fs_dev && fsp->fs_dev->cache_data)
        cache_init(fsp->fs_dev, blk_shift);

    /* start out in the root directory */
    if (fsp->fs_ops->iget_root) {
        fsp->root = fsp->fs_ops->iget_root(fsp);
        fsp->cwd = get_inode(fsp->root);
    }

    if (fsp->fs_ops->chdir_start) {
        if (fsp->fs_ops->chdir_start(fsp) < 0)
            printf("Failed to chdir to start directory\n");
    }
    return fsp;

bail:
    free(fsp);
    return NULL;
}


void pm_searchdir(com32sys_t *regs)
{
    char *name = MK_PTR(regs->ds, regs->edi.w[0]);
    int rv;

    rv = searchdir(name, NULL);
    if (rv < 0) {
	regs->esi.w[0]  = 0;
	regs->eax.l     = 0;
	regs->eflags.l |= EFLAGS_ZF;
    } else {
	regs->esi.w[0]  = rv;
	regs->eax.l     = handle_to_file(rv)->inode->size;
	regs->eflags.l &= ~EFLAGS_ZF;
    }
}

int searchdir(const char *name, struct fs_info *fp)
{
    struct inode *inode = NULL;
    struct inode *parent = NULL;
    struct file *file;
    char *pathbuf = NULL;
    char *part, *p, echar;
    int symlink_count = MAX_SYMLINK_CNT;

    dprintf("searchdir: %s  root: %p  cwd: %p\n",
	    name, this_fs->root, this_fs->cwd);
    if (!fp)
        fp = this_fs;

    if (!(file = alloc_file()))
	goto err_no_close;

    file->fs = fp;

    /* if we have ->searchdir method, call it */
    if (file->fs->fs_ops->searchdir) {
	file->fs->fs_ops->searchdir(name, file);

	if (file->inode)
	    return file_to_handle(file);
	else
	    goto err;
    }

    /* else, try the generic-path-lookup method */

    parent = get_inode(fp->cwd);
    p = pathbuf = strdup(name);
    if (!pathbuf)
	goto err;

    do {
    got_link:
	if (*p == '/') {
	    put_inode(parent);
	    parent = get_inode(fp->root);
	}

	do {
	    inode = get_inode(parent);

	    while (*p == '/')
		p++;

	    if (!*p)
		break;

	    part = p;
	    while ((echar = *p) && echar != '/')
		p++;
	    *p++ = '\0';

	    if (part[0] == '.' && part[1] == '.' && part[2] == '\0') {
		if (inode->parent) {
		    put_inode(parent);
		    parent = get_inode(inode->parent);
		    put_inode(inode);
		    inode = NULL;
		    if (!echar) {
			/* Terminal double dots */
			inode = parent;
			parent = inode->parent ?
			    get_inode(inode->parent) : NULL;
		    }
		}
	    } else if (part[0] != '.' || part[1] != '\0') {
	    inode = fp->fs_ops->iget(part, parent);
	    if (!inode)
		    goto err;
		if (inode->mode == DT_LNK) {
		    char *linkbuf, *q;
		    int name_len = echar ? strlen(p) : 0;
		    int total_len = inode->size + name_len + 2;
		    int link_len;

		    if (!fp->fs_ops->readlink ||
			--symlink_count == 0       ||      /* limit check */
			total_len > MAX_SYMLINK_BUF)
			goto err;

		    linkbuf = malloc(total_len);
		    if (!linkbuf)
			goto err;

		    link_len = fp->fs_ops->readlink(inode, linkbuf);
		    if (link_len <= 0) {
			free(linkbuf);
			goto err;
		    }

		    q = linkbuf + link_len;

		    if (echar) {
			if (link_len > 0 && q[-1] != '/')
			    *q++ = '/';

			memcpy(q, p, name_len+1);
		    } else {
			*q = '\0';
		    }

		    free(pathbuf);
		    p = pathbuf = linkbuf;
		    put_inode(inode);
		    inode = NULL;
		    goto got_link;
		}

		inode->name = strdup(part);
		dprintf("path component: %s\n", inode->name);

		inode->parent = parent;
		parent = NULL;

		if (!echar)
		    break;

		if (inode->mode != DT_DIR)
		    goto err;

		parent = inode;
		inode = NULL;
	    }
	} while (echar);
    } while (0);

    free(pathbuf);
    pathbuf = NULL;
    put_inode(parent);
    parent = NULL;

    if (!inode)
	goto err;

    file->inode  = inode;
    file->offset = 0;

    return file_to_handle(file);

err:
    put_inode(inode);
    put_inode(parent);
    if (pathbuf)
	free(pathbuf);
    _close_file(file);
err_no_close:
    return -1;
}

int open_file(const char *name, struct com32_filedata *filedata)
{
    int rv;
    struct file *file;
    char mangled_name[FILENAME_MAX];
    uint8_t hdd = 0;
    uint8_t partition = 0;
    char buff[4];

    if (name[0] == '(') {
        char relative_name[FILENAME_MAX];
        const char *c = name;
        int i = 0;
        int mult = 1;

        /* get hdd number */
        for (++c; *c != ' '; ++c)
            buff[i++] = *c;
        buff[i] = '\0';

        /* str to uint8_t */
        while (i--) {
            hdd += (buff[i] - 48) * mult;
            mult *= 10;
        }

        /* get partition number */
        i = 0;
        for (++c; *c != ')'; ++c)
            buff[i++] = *c;
        buff[i] = '\0';

        /* str to uint8_t */
        mult = 1;
        while (i--) {
            partition += (buff[i] - 48) * mult;
            mult *= 10;
        }

        i = 0;
        /* c was on ')' jump ':' and stop at beginning of path */
        for (c += 2; *c; c++)
            relative_name[i++] = *c;
        relative_name[i] = '\0';

        mangle_name(mangled_name, relative_name, get_fs_info(hdd, partition));

        rv = searchdir(mangled_name, get_fs_info(hdd, partition));

        if (rv < 0)
            return rv;
        file = handle_to_file(rv);

        if (file->inode->mode != DT_REG) {
            _close_file(file);
            return -1;
        }

        filedata->size = file->inode->size;
        filedata->blocklg2 = SECTOR_SHIFT(file->fs);
        filedata->handle = rv;

        return rv;
    }

    dprintf("open_file %s\n", name);

    mangle_name(mangled_name, name, NULL);
    rv = searchdir(mangled_name, NULL);

    if (rv < 0)
	return rv;

    file = handle_to_file(rv);

    if (file->inode->mode != DT_REG) {
	_close_file(file);
	return -1;
    }

    filedata->size	= file->inode->size;
    filedata->blocklg2	= SECTOR_SHIFT(file->fs);
    filedata->handle	= rv;

    return rv;
}

void pm_open_file(com32sys_t *regs)
{
    int rv;
    struct file *file;
    const char *name = MK_PTR(regs->es, regs->esi.w[0]);
    char mangled_name[FILENAME_MAX];

    dprintf("pm_open_file %s\n", name);

    mangle_name(mangled_name, name, NULL);
    rv = searchdir(mangled_name, NULL);
    if (rv < 0) {
	regs->eflags.l |= EFLAGS_CF;
    } else {
	file = handle_to_file(rv);
	regs->eflags.l &= ~EFLAGS_CF;
	regs->eax.l = file->inode->size;
	regs->ecx.w[0] = SECTOR_SIZE(file->fs);
	regs->esi.w[0] = rv;
    }
}

void close_file(uint16_t handle)
{
    struct file *file;

    if (handle) {
	file = handle_to_file(handle);
	_close_file(file);
    }
}

void pm_close_file(com32sys_t *regs)
{
    close_file(regs->esi.w[0]);
}

/*
 * it will do:
 *    initialize the memory management function;
 *    set up the vfs fs structure;
 *    initialize the device structure;
 *    invoke the fs-specific init function;
 *    initialize the cache if we need one;
 *    finally, get the current inode for relative path looking.
 */
__bss16 uint16_t SectorSize, SectorShift;

void fs_init(com32sys_t *regs)
{
    static struct fs_info fs;	/* The actual filesystem buffer */
    uint8_t disk_devno = regs->edx.b[0];
    uint8_t disk_cdrom = regs->edx.b[1];
    sector_t disk_offset = regs->ecx.l | ((sector_t)regs->ebx.l << 32);
    uint16_t disk_heads = regs->esi.w[0];
    uint16_t disk_sectors = regs->edi.w[0];
    uint32_t maxtransfer = regs->ebp.l;
    int blk_shift = -1;
    struct device *dev = NULL;
    /* ops is a ptr list for several fs_ops */
    const struct fs_ops **ops = (const struct fs_ops **)regs->eax.l;

    /* Initialize malloc() */
    mem_init();

    /* Default name for the root directory */
    fs.cwd_name[0] = '/';

    while ((blk_shift < 0) && *ops) {
	/* set up the fs stucture */
	fs.fs_ops = *ops;

	/*
	 * This boldly assumes that we don't mix FS_NODEV filesystems
	 * with FS_DEV filesystems...
	 */
	if (fs.fs_ops->fs_flags & FS_NODEV) {
	    fs.fs_dev = NULL;
	} else {
	    if (!dev)
		dev = device_init(disk_devno, disk_cdrom, disk_offset,
				  disk_heads, disk_sectors, maxtransfer);
	    fs.fs_dev = dev;
	}
	/* invoke the fs-specific init code */
	blk_shift = fs.fs_ops->fs_init(&fs);
	ops++;
    }
    if (blk_shift < 0) {
	printf("No valid file system found!\n");
	while (1)
		;
    }
    this_fs = &fs;

    /* initialize the cache */
    if (fs.fs_dev && fs.fs_dev->cache_data)
        cache_init(fs.fs_dev, blk_shift);

    /* start out in the root directory */
    if (fs.fs_ops->iget_root) {
	fs.root = fs.fs_ops->iget_root(&fs);
	fs.cwd = get_inode(fs.root);
	dprintf("init: root inode %p, cwd inode %p\n", fs.root, fs.cwd);
    }

    if (fs.fs_ops->chdir_start) {
	    if (fs.fs_ops->chdir_start(NULL) < 0)
		    printf("Failed to chdir to start directory\n");
    }

    SectorShift = fs.sector_shift;
    SectorSize  = fs.sector_size;
}
