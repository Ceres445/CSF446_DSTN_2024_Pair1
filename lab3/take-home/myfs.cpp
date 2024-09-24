#define FUSE_USE_VERSION 31

#include "params.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse3/fuse.h>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

// DO NOT CHANGE
FILE *log_open(char *file_name)
{
    FILE *logfile;

    // very first thing, open up the logfile and mark that we got in
    // here.  If we can't open the logfile, we're dead.
    logfile = fopen(file_name, "w");
    if (logfile == NULL)
    {
        perror("logfile");
        exit(EXIT_FAILURE);
    }

    // set logfile to line buffering
    setvbuf(logfile, NULL, _IOLBF, 0);

    return logfile;
}

// DO NOT CHANGE
void log_char(char c)
{

    FILE *log_file = MYFS_DATA->logfile;
    if (c == '\n')
    {
        fprintf(log_file, "\\n"); // Prints the literal characters "\n"
    }
    else
    {
        fprintf(log_file, "%c", c);
    }
}

// DO NOT CHANGE
void log_fuse_context()
{
    struct myfs_state *myfs_data = MYFS_DATA;
    FILE *log_file = myfs_data->logfile;

    fprintf(log_file, "PATH_TO_INODE_MAP:\n");
    for (auto const &x : myfs_data->path_to_inode)
    {
        fprintf(log_file, "%s: %d\n", x.first.c_str(), x.second);
    }

    fprintf(log_file, "INODE_BITMAP: [");
    for (int i = 0; i < myfs_data->NUM_INODES; ++i)
    {
        bool b = myfs_data->inode_bitmap[i];
        fprintf(log_file, "%d", b);
        if (i != myfs_data->NUM_INODES - 1)
        {
            fprintf(log_file, ", ");
        }
    }
    fprintf(log_file, "]\n");

    fprintf(log_file, "DATA_BLOCK_BITMAP: [");
    for (int i = 0; i < myfs_data->NUM_DATA_BLOCKS; ++i)
    {
        bool b = myfs_data->data_block_bitmap[i];
        fprintf(log_file, "%d", b);
        if (i != myfs_data->NUM_DATA_BLOCKS - 1)
        {
            fprintf(log_file, ", ");
        }
    }
    fprintf(log_file, "]\n");

    for (int i = 0; i < myfs_data->NUM_INODES; ++i)
    {
        fprintf(log_file, "inode%d: ", i);
        int num_blocks = myfs_data->inodes[i]->blocks.size();
        for (int j = 0; j < num_blocks; ++j)
        {
            int block_index = myfs_data->inodes[i]->blocks[j];
            for (int k = 0; k < myfs_data->DATA_BLOCK_SIZE; ++k)
            {
                log_char(myfs_data->data_blocks[block_index]->data[k]);
            }
        }
        fprintf(log_file, "\n");
    }
}

// DO NOT CHANGE
void log_msg(const char *format, ...)
{
    // takes a format string and a variable number of arguments
    va_list ap;
    va_start(ap, format);

    vfprintf(MYFS_DATA->logfile, format, ap);
}

static void myfs_fullpath(char fpath[PATH_MAX], const char *path)
{
    struct fuse_context *temp = fuse_get_context();

    strcpy(fpath, MYFS_DATA->rootdir);
    strncat(fpath, path, PATH_MAX);
}

static int myfs_unlink(const char *path)
{
    int res;
    char fpath[PATH_MAX];
    myfs_fullpath(fpath, path);

    log_msg("DELETE %s\n", path);

    res = unlink(fpath);
    if (res == -1)
    {
        log_msg("ERROR: DELETE %s\n", path);
        log_fuse_context();
        return -errno;
    }

    // Get the inode index of the file
    int inode_index = -1;
    // use path_to_inode map to get the inode index
    if (MYFS_DATA->path_to_inode.find(path) != MYFS_DATA->path_to_inode.end())
    {
        inode_index = MYFS_DATA->path_to_inode[path];
    }

    if (inode_index == -1)
    {
        log_msg("ERROR: INODE NOT FOUND\n");
        log_fuse_context();
        return -1;
    }

    // free the data blocks of the file
    for (int i = 0; i < MYFS_DATA->inodes[inode_index]->blocks.size(); i++)
    {
        int block_index = MYFS_DATA->inodes[inode_index]->blocks[i];
        MYFS_DATA->data_block_bitmap[block_index] = false;
        // memset(MYFS_DATA->data_blocks[block_index]->data, 0,
        // MYFS_DATA->DATA_BLOCK_SIZE);

        // MYFS_DATA->data_blocks[block_index] = nullptr;

        // strcpy(MYFS_DATA->data_blocks[block_index]->data, "");
        // TODO: We need to delete properly, this is memsetting to 0, which is messing up diff
        MYFS_DATA->data_blocks[block_index] =
            new data_block(MYFS_DATA->DATA_BLOCK_SIZE);
    }

    // free the inode
    MYFS_DATA->inode_bitmap[inode_index] = false;
    MYFS_DATA->path_to_inode.erase(path);

    // Clear the inode
    MYFS_DATA->inodes[inode_index] = new inode();

    log_fuse_context();

    return 0;
}

static int myfs_create(const char *path, mode_t mode,
                       struct fuse_file_info *fi)
{
    int res;
    char fpath[PATH_MAX];
    myfs_fullpath(fpath, path);

    log_msg("CREATE %s\n", path);

    // Add code to allocate an inode and data block for the new file
    int inode_index = -1;

    for (int i = 0; i < MYFS_DATA->NUM_INODES; i++)
    {
        if (!MYFS_DATA->inode_bitmap[i])
        {
            inode_index = i;
            break;
        }
    }

    // for (int i = 0; i < MYFS_DATA->NUM_DATA_BLOCKS; i++) {
    // 	if (MYFS_DATA->data_block_bitmap[i]) {
    // 		data_block_index = i;
    // 		break;
    // 	}
    // }

    if (inode_index == -1)
    {
        log_msg("ERROR: INODES FULL\n");
        log_fuse_context();
        return -1;
    }

    // MYFS_DATA->inodes[inode_index]->blocks.push_back(data_block_index);
    // Update the inode bitmap and data block bitmap
    MYFS_DATA->inode_bitmap[inode_index] = true;
    // MYFS_DATA->data_block_bitmap[data_block_index] = false;

    // Update the path to inode map
    MYFS_DATA->path_to_inode[path] = inode_index;

    //    if (inodes full) {
    //    	log_msg("ERROR: INODES FULL\n");
    //      	log_fuse_context();
    //        return -1;
    //    }

    res = open(fpath, fi->flags, mode);
    if (res == -1)
    {
        log_msg("ERROR: CREATE %s\n", path);
        log_fuse_context();
        return -errno;
    }

    fi->fh = res;

    log_fuse_context();

    return 0;
}

static int myfs_read(const char *path, char *buf, size_t size, off_t offset,
                     struct fuse_file_info *fi)
{
    int fd;
    int res;
    char fpath[PATH_MAX];
    myfs_fullpath(fpath, path);

    log_msg("READ %s\n", path);

    if (fi == NULL)
        fd = open(fpath, O_RDONLY);
    else
        fd = fi->fh;

    if (fd == -1)
    {
        log_msg("ERROR: READ %s\n", path);
        log_fuse_context();
        return -errno;
    }

    res = pread(fd, buf, size, offset);
    if (res == -1)
    {
        log_msg("ERROR: READ %s\n", path);
        return -errno;
    }

    // Get the inode index of the file
    int inode_index = -1;
    // use path_to_inode map to get the inode index
    if (MYFS_DATA->path_to_inode.find(path) != MYFS_DATA->path_to_inode.end())
    {
        inode_index = MYFS_DATA->path_to_inode[path];
    }

    if (inode_index == -1)
    {
        log_msg("ERROR: INODE NOT FOUND\n");
        log_fuse_context();
        return -1;
    }

    // read the data blocks of the file
    for (int i = 0; i < MYFS_DATA->inodes[inode_index]->blocks.size(); i++)
    {
        int block_index = MYFS_DATA->inodes[inode_index]->blocks[i];

        log_msg("DATA BLOCK %d: ", block_index);
        for (int k = 0; k < strlen(MYFS_DATA->data_blocks[block_index]->data); ++k)
        {
            log_char(MYFS_DATA->data_blocks[block_index]->data[k]);
        }
        log_msg("\n");
    }

    if (fi == NULL)
        close(fd);

    log_fuse_context();

    return res;
}

static int myfs_write(const char *path, const char *buf, size_t size,
                      off_t offset, struct fuse_file_info *fi)
{
    int fd;
    int res;
    char fpath[PATH_MAX];
    myfs_fullpath(fpath, path);

    log_msg("WRITE %s\n", path);

    (void)fi;
    if (fi == NULL)
        fd = open(fpath, O_WRONLY);
    else
        fd = fi->fh;

    if (fd == -1)
    {
        log_msg("ERROR: WRITE %s\n", path);
        log_fuse_context();
        return -errno;
    }

    int offt = 0;
    // Try using the last data block if it exists
    int inode_index = -1;
    if (MYFS_DATA->path_to_inode.find(path) != MYFS_DATA->path_to_inode.end())
    {
        inode_index = MYFS_DATA->path_to_inode[path];
    }
    if (inode_index == -1)
    {
        log_msg("ERROR: INODE NOT FOUND\n");
        log_fuse_context();
        return -1;
    }

    if (MYFS_DATA->inodes[inode_index]->blocks.size() > 0)
    {
        int last_block_index = MYFS_DATA->inodes[inode_index]->blocks.back();
        if (MYFS_DATA->data_block_bitmap[last_block_index])
        {
            int remaining_space = MYFS_DATA->DATA_BLOCK_SIZE - strlen(MYFS_DATA->data_blocks[last_block_index]->data);
            strncpy(MYFS_DATA->data_blocks[last_block_index]->data + strlen(MYFS_DATA->data_blocks[last_block_index]->data), buf + offt, remaining_space);
            offt += remaining_space;
        }
    }
    std::vector<int> to_use_blocks;

    int num_blocks = (size - offt) / MYFS_DATA->DATA_BLOCK_SIZE + ((size - offt) % MYFS_DATA->DATA_BLOCK_SIZE > 0);
    for (int i = 0; i < num_blocks; i++)
    {
        int data_block_index = -1;
        for (int j = 0; j < MYFS_DATA->NUM_DATA_BLOCKS; j++)
        {
            if (!MYFS_DATA->data_block_bitmap[j])
            {
                data_block_index = j;
                break;
            }
        }
        if (data_block_index == -1)
        {
            // Free the blocks we were supposed to use
            for (int k = 0; k < to_use_blocks.size(); k++)
            {
                MYFS_DATA->data_block_bitmap[to_use_blocks[k]] = false;
            }
            log_msg("ERROR: NOT ENOUGH DATA BLOCKS\n");
            log_fuse_context();

            return -1;
        }
        to_use_blocks.push_back(data_block_index);
        MYFS_DATA->data_block_bitmap[data_block_index] = true;
    }
    int current_block_index = 0;
    while (current_block_index < to_use_blocks.size())
    {
        int data_block_index = to_use_blocks[current_block_index];
        MYFS_DATA->data_block_bitmap[data_block_index] = true;
        strncpy(MYFS_DATA->data_blocks[data_block_index]->data, buf + offt, MYFS_DATA->DATA_BLOCK_SIZE);

        // Add to the inode
        MYFS_DATA->inodes[inode_index]->blocks.push_back(data_block_index);

        offt += MYFS_DATA->DATA_BLOCK_SIZE;
        current_block_index++;
    }

    res = pwrite(fd, buf, size, offset);
    if (res == -1)
    {
        log_msg("ERROR: WRITE %s\n", path);
        log_fuse_context();
        return -errno;
    }

    if (fi == NULL)
        close(fd);

    log_fuse_context();

    return res;
}

static void *myfs_init(struct fuse_conn_info *conn, struct fuse_config *cfg)
{
    (void)conn;
    cfg->use_ino = 1;
    cfg->entry_timeout = 0;
    cfg->attr_timeout = 0;
    cfg->negative_timeout = 0;

    return MYFS_DATA;
}

static int myfs_getattr(const char *path, struct stat *stbuf,
                        struct fuse_file_info *fi)
{
    (void)fi;
    int res;
    char fpath[PATH_MAX];
    myfs_fullpath(fpath, path);

    res = lstat(fpath, stbuf);
    if (res == -1)
        return -errno;

    return 0;
}

static int myfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                        off_t offset, struct fuse_file_info *fi,
                        enum fuse_readdir_flags flags)
{
    DIR *dp;
    struct dirent *de;

    (void)offset;
    (void)fi;
    (void)flags;

    char fpath[PATH_MAX];
    myfs_fullpath(fpath, path);

    dp = opendir(fpath);
    if (dp == NULL)
        return -errno;

    while ((de = readdir(dp)) != NULL)
    {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;
        if (filler(buf, de->d_name, &st, 0,
                   static_cast<enum fuse_fill_dir_flags>(0)))
            break;
    }

    closedir(dp);
    return 0;
}

static int myfs_mkdir(const char *path, mode_t mode)
{
    int res;
    char fpath[PATH_MAX];
    myfs_fullpath(fpath, path);

    res = mkdir(fpath, mode);
    if (res == -1)
        return -errno;

    return 0;
}

static int myfs_rmdir(const char *path)
{
    int res;
    char fpath[PATH_MAX];
    myfs_fullpath(fpath, path);

    res = rmdir(fpath);
    if (res == -1)
        return -errno;

    return 0;
}

static int myfs_open(const char *path, struct fuse_file_info *fi)
{
    int res;

    char fpath[PATH_MAX];
    myfs_fullpath(fpath, path);

    // log_msg("OPEN %s\n", path);

    res = open(fpath, fi->flags);
    if (res == -1)
        return -errno;

    fi->fh = res;
    return 0;
}

static int myfs_release(const char *path, struct fuse_file_info *fi)
{
    (void)path;
    close(fi->fh);
    return 0;
}

static const struct fuse_operations myfs_oper = {
    .getattr = myfs_getattr,
    .mkdir = myfs_mkdir,
    .unlink = myfs_unlink,
    .rmdir = myfs_rmdir,
    .open = myfs_open,
    .read = myfs_read,
    .write = myfs_write,
    .release = myfs_release,
    .readdir = myfs_readdir,
    .init = myfs_init,
    .create = myfs_create,
};

void myfs_usage()
{
    fprintf(stderr,
            "usage:  myfs [FUSE and mount options] mount_point log_file "
            "root_dir num_inodes num_data_blocks data_block_size\n");
    abort();
}

int main(int argc, char *argv[])
{
    int fuse_stat;
    struct myfs_state *myfs_data;

    // bbfs doesn't do any access checking on its own (the comment
    // blocks in fuse.h mention some of the functions that need
    // accesses checked -- but note there are other functions, like
    // chown(), that also need checking!).  Since running bbfs as root
    // will therefore open Metrodome-sized holes in the system
    // security, we'll check if root is trying to mount the filesystem
    // and refuse if it is.  The somewhat smaller hole of an ordinary
    // user doing it with the allow_other flag is still there because
    // I don't want to parse the options string.
    if ((getuid() == 0) || (geteuid() == 0))
    {
        fprintf(stderr,
                "Running BBFS as root opens unnacceptable security holes\n");
        return 1;
    }

    // See which version of fuse we're running
    fprintf(stderr, "Fuse library version %d.%d\n", FUSE_MAJOR_VERSION,
            FUSE_MINOR_VERSION);

    // Perform some sanity checking on the command line:  make sure
    // there are enough arguments, and that neither of the last two
    // start with a hyphen (this will break if you actually have a
    // rootpoint or mountpoint whose name starts with a hyphen, but so
    // will a zillion other programs)
    if ((argc < 6) || (argv[argc - 6][0] == '-') ||
        (argv[argc - 5][0] == '-') || (argv[argc - 4][0] == '-'))
        myfs_usage();

    myfs_data = new myfs_state(
        log_open(argv[argc - 5]), realpath(argv[argc - 4], NULL),
        atoi(argv[argc - 3]), atoi(argv[argc - 2]), atoi(argv[argc - 1]));

    argc -= 5;

    // turn over control to fuse
    fprintf(stderr, "about to call fuse_main\n");
    fuse_stat = fuse_main(argc, argv, &myfs_oper, myfs_data);
    fprintf(stderr, "fuse_main returned %d\n", fuse_stat);

    return fuse_stat;
}
