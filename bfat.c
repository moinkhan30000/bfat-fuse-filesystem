#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#define FUSE_USE_VERSION 25
#include <fuse.h>


#define BLOCKSIZE 4096 
#define MAXFILENAME 48  
#define MAXFILES 32

int bfat_getattr( const char *, struct stat * );
int bfat_readdir( const char *, void *, fuse_fill_dir_t, off_t,
                 struct fuse_file_info * );
int bfat_open( const char *, struct fuse_file_info * );
int bfat_read( const char *, char *, size_t, off_t,
              struct fuse_file_info * );
int bfat_release(const char *path, struct fuse_file_info *fi);
int bfat_mknod(const char *path, mode_t mode, dev_t dev);
int bfat_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int bfat_unlink(const char *path);
// ....


/* Utility functions */
int read_block (void *block, int k);
int write_block (void *block, int k);

static struct fuse_operations bfat_oper = {
	.getattr	= bfat_getattr,
	.readdir	= bfat_readdir,
	.mknod      = bfat_mknod,
    
	.mkdir = NULL,
	.unlink     = bfat_unlink,
	.rmdir = NULL,
	.truncate = NULL,
	.open	    = bfat_open,
	.read	    = bfat_read,
	.release    = bfat_release,
	.write      = bfat_write,
	.rename = NULL,
	.utime = NULL
};


struct superblock
{
    int numblocks;
    int fat_start;
    int fat_length;
    int root_directory_block;
};


struct direnty 
{
    char filename[MAXFILENAME];
    int size;
    int first_block;
    int used;
};

struct direnty root_directory[MAXFILES];
int *fat=NULL;
struct superblock sb;


// ********** Global Variables ***************************************
int fd_disk;


int bfat_getattr(const char *path, struct stat *stbuf) {
    printf("[GETATTR] path: %s\n", path);
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0 || strcmp(path, "/.") == 0 || strcmp(path, "/..") == 0) {
        printf("[GETATTR] Detected root or special directory: %s\n", path);
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }


    const char *name = path + 1;
    printf("[GETATTR] Looking for file: %s\n", name);

    for (int i = 0; i < MAXFILES; ++i) {
        if (root_directory[i].used) {
            printf("[GETATTR] Checking entry %d: %s\n", i, root_directory[i].filename);
            if (strcmp(root_directory[i].filename, name) == 0) {
                printf("[GETATTR] Match found: %s (size: %d)\n", root_directory[i].filename, root_directory[i].size);
                stbuf->st_mode = S_IFREG | 0777;
                stbuf->st_nlink = 1;
                stbuf->st_size = root_directory[i].size;
                return 0;
            }
        }
    }

    printf("[GETATTR] File not found: %s\n", name);
    return -ENOENT;
}

int bfat_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    (void) offset;
    (void) fi;

    printf("[READDIR] path: %s\n", path);
    if (strcmp(path, "/") != 0)
        return -ENOENT;

    printf("[READDIR] Adding entries: '.', '..'\n");
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    for (int i = 0; i < MAXFILES; ++i) {
        if (root_directory[i].used) {
            printf("[READDIR] Adding file: %s (entry %d)\n", root_directory[i].filename, i);
            filler(buf, root_directory[i].filename, NULL, 0);
        } else {
            printf("[READDIR] Skipping unused entry %d\n", i);
        }
    }

    return 0;
}
int bfat_open(const char *path, struct fuse_file_info *fi) {
    printf("open: (path=%s)\n", path);
    const char *name = path + 1;
    for (int i = 0; i < MAXFILES; ++i) {
        if (root_directory[i].used && strcmp(root_directory[i].filename, name) == 0)
            return 0;
    }
    return -ENOENT;
}

int bfat_read(const char *path, char *buf, size_t size, off_t offset,struct fuse_file_info *fi) {
    printf("read: (path=%s)\n", path);
    char *name = path + 1;
    for (int i = 0; i < MAXFILES; ++i) {
        if (!root_directory[i].used || strcmp(root_directory[i].filename, name) != 0)
            continue;

        int block = root_directory[i].first_block;
        int file_size = root_directory[i].size;
        if (offset >= file_size)
            return 0;

        int bytes_read = 0;
        int block_offset = offset / BLOCKSIZE;
        int byte_offset = offset % BLOCKSIZE;

        while (block_offset-- > 0 && block != 0xFFFFFFFF) {
            block = fat[block];
        }

        char data[BLOCKSIZE];
        while (block != 0xFFFFFFFF && bytes_read < size) {
            read_block(data, block);
            int to_copy = BLOCKSIZE - byte_offset;
            if (to_copy > size - bytes_read)
                to_copy = size - bytes_read;
            memcpy(buf + bytes_read, data + byte_offset, to_copy);
            bytes_read += to_copy;
            byte_offset = 0;
            block = fat[block];
        }
        return bytes_read;
    }
    return -ENOENT;
}

int bfat_release(const char *path, struct fuse_file_info *fi) {
	printf("release: (path=%s)\n", path);
	return 0;
}



int bfat_init() {
    char buffer[BLOCKSIZE];

    fd_disk = open("disk1", O_RDWR);
    if (fd_disk < 0) {
        perror("Disk open error");
        exit(1);
    }


    if (read_block(buffer, 0) != 0) {
        fprintf(stderr, "Failed to read superblock block\n");
        exit(1);
    }
    memcpy(&sb, buffer, sizeof(struct superblock));

    printf("BFAT Init: Superblock loaded\n");
    printf(" - Total blocks: %d\n", sb.numblocks);
    printf(" - FAT starts at block: %d\n", sb.fat_start);
    printf(" - FAT length (blocks): %d\n", sb.fat_length);
    printf(" - Root directory block: %d\n", sb.root_directory_block);


    int fat_bytes = sb.fat_length * BLOCKSIZE;
    fat = malloc(fat_bytes);
    if (!fat) {
        fprintf(stderr, "Failed to allocate memory for FAT\n");
        exit(1);
    }

    for (int i = 0; i < sb.fat_length; ++i) {
        if (read_block((char*)fat + i * BLOCKSIZE, sb.fat_start + i) != 0) {
            fprintf(stderr, "Failed to read FAT block %d\n", i);
            exit(1);
        }
    }
    printf("BFAT Init: FAT loaded\n");


    if (read_block(buffer, sb.root_directory_block) != 0) {
        fprintf(stderr, "Failed to read root directory block\n");
        exit(1);
    }
    memcpy(root_directory, buffer, sizeof(root_directory));
    printf("BFAT Init: Root directory loaded\n");


    for (int i = 0; i < MAXFILES; ++i) {
        if (root_directory[i].used) {
            printf(" - [%d] filename: %s, size: %d, first_block: %d\n",
                   i, root_directory[i].filename,
                   root_directory[i].size,
                   root_directory[i].first_block);
        }
    }

    return 0;
}





int bfat_mknod(const char *path, mode_t mode, dev_t dev) {
    printf("mknod: (path=%s)\n", path);
    const char *name = path + 1; 

    for (int i = 0; i < MAXFILES; ++i) {
        if (root_directory[i].used && strcmp(root_directory[i].filename, name) == 0)
            return -EEXIST;
    }

    
    int idx = -1;
    for (int i = 0; i < MAXFILES; ++i) {
        if (!root_directory[i].used) {
            idx = i;
            break;
        }
    }
    if (idx == -1) return -ENOSPC;


    int first_block = -1;
    for (int i = 0; i < sb.numblocks; ++i) {
        if (fat[i] == 0x00000000) {
            first_block = i;
            fat[i] = 0xFFFFFFFF;
            break;
        }
    }
    if (first_block == -1) return -ENOSPC;


    strncpy(root_directory[idx].filename, name, MAXFILENAME);
    root_directory[idx].used = 1;
    root_directory[idx].size = 0;
    root_directory[idx].first_block = first_block;

    char buffer[BLOCKSIZE];
    memcpy(buffer, root_directory, sizeof(root_directory));
    write_block(buffer, sb.root_directory_block);

    for (int i = 0; i < sb.fat_length; ++i) {
        write_block((char *)fat + i * BLOCKSIZE, sb.fat_start + i);
    }

    return 0;
}

int bfat_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    printf("write: (path=%s)\n", path);
    const char *name = path + 1;

    for (int i = 0; i < MAXFILES; ++i) {
        if (!root_directory[i].used || strcmp(root_directory[i].filename, name) != 0)
            continue;

        int block = root_directory[i].first_block;
        int file_offset = 0;
        int prev_block = -1;


        while (offset >= BLOCKSIZE) {
            if (fat[block] == 0xFFFFFFFF) {

                for (int j = 0; j < sb.numblocks; ++j) {
                    if (fat[j] == 0x00000000) {
                        fat[block] = j;
                        fat[j] = 0xFFFFFFFF;
                        break;
                    }
                }
            }
            block = fat[block];
            offset -= BLOCKSIZE;
        }

        size_t written = 0;
        char temp[BLOCKSIZE];

        while (written < size) {
            read_block(temp, block);
            size_t to_write = BLOCKSIZE - offset;
            if (to_write > size - written)
                to_write = size - written;

            memcpy(temp + offset, buf + written, to_write);
            write_block(temp, block);

            written += to_write;
            offset = 0;

            if (written < size) {
                if (fat[block] == 0xFFFFFFFF) {
                    for (int j = 0; j < sb.numblocks; ++j) {
                        if (fat[j] == 0x00000000) {
                            fat[block] = j;
                            fat[j] = 0xFFFFFFFF;
                            break;
                        }
                    }
                }
                block = fat[block];
            }
        }


        if ((int)(offset + written) > root_directory[i].size)
            root_directory[i].size = offset + written;


        char buffer[BLOCKSIZE];
        memcpy(buffer, root_directory, sizeof(root_directory));
        write_block(buffer, sb.root_directory_block);

        for (int k = 0; k < sb.fat_length; ++k) {
            write_block((char *)fat + k * BLOCKSIZE, sb.fat_start + k);
        }

        return written;
    }

    return -ENOENT;
}

int bfat_unlink(const char *path) {
    printf("unlink: (path=%s)\n", path);
    const char *name = path + 1;

    for (int i = 0; i < MAXFILES; ++i) {
        if (root_directory[i].used && strcmp(root_directory[i].filename, name) == 0) {
            
            int block = root_directory[i].first_block;
            while (block != 0xFFFFFFFF) {
                int next = fat[block];
                fat[block] = 0x00000000;
                block = next;
            }

            
            root_directory[i].used = 0;

            
            char buffer[BLOCKSIZE];
            memcpy(buffer, root_directory, sizeof(root_directory));
            write_block(buffer, sb.root_directory_block);

            
            for (int j = 0; j < sb.fat_length; ++j) {
                write_block((char *)fat + j * BLOCKSIZE, sb.fat_start + j);
            }

            return 0;
        }
    }

    return -ENOENT;
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <mountpoint> <diskfile>\n", argv[0]);
        return 1;
    }

    const char *mountpoint = argv[1];
    const char *diskfile = argv[2];

    fd_disk = open(diskfile, O_RDWR);
    if (fd_disk < 0) {
        perror("Could not open disk file");
        return 1;
    }

    if (bfat_init() != 0) {
        fprintf(stderr, "Failed to initialize BFAT\n");
        return 1;
    }

    printf("Initialized BFAT, launching FUSE...\n");

    
    char *fuse_argv[] = { argv[0], (char *)mountpoint };
    return fuse_main(2, fuse_argv, &bfat_oper);
}


int read_block (void *block, int k)
{
    int n;
    int offset;

    offset = k * BLOCKSIZE;
    lseek(fd_disk, (off_t) offset, SEEK_SET);
    n = read (fd_disk, block, BLOCKSIZE);
    if (n != BLOCKSIZE) {
        printf ("read error\n");
        return -1;
    }
    return (0);
}

int write_block (void *block, int k)
{
    int n;
    int offset;

    offset = k * BLOCKSIZE;
    lseek(fd_disk, (off_t) offset, SEEK_SET);
    n = write (fd_disk, block, BLOCKSIZE);
    if (n != BLOCKSIZE) {
        printf ("write error\n");
        return (-1);
    }
    return 0;
}