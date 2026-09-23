#include <fuse.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#define BLOCKSIZE 4096 // bytes
#define MAXFILENAME 48  // maximum filename that can be stored by BFS

#define MAXFILES 32
#define FREE_BLOCK 0x00000000
#define END_BLOCK  0xFFFFFFFF


/* Utility functions */
int read_block (int fd_disk, void *block, int k);
int write_block (int fd_disk, void *block, int k);

struct superblock {
    int num_blocks;
    int fat_start;
    int fat_length;
    int root_directory_block;
};

struct direnty {
    char filename[MAXFILENAME];
    int size;
    int first_block;
    int used;
};


// ********** Global Variables ***************************************
int  disk;
char diskname[64];
struct superblock sb;
struct direnty root_directory[MAXFILES];
int *fat;

int bfat_format() {
    char buffer[BLOCKSIZE];

    off_t size = lseek(disk, 0, SEEK_END);
    sb.num_blocks = size / BLOCKSIZE;

    int fat_entries = sb.num_blocks;
    int fat_bytes = fat_entries * 4;
    sb.fat_length = (fat_bytes + BLOCKSIZE - 1) / BLOCKSIZE;
    sb.fat_start = 1;
    sb.root_directory_block = sb.fat_start + sb.fat_length;

    bzero(buffer, BLOCKSIZE);
    memcpy(buffer, &sb, sizeof(struct superblock));
    write_block(disk, buffer, 0);

    fat = malloc(sb.fat_length * BLOCKSIZE);
    for (int i = 0; i < sb.num_blocks; ++i)
        fat[i] = FREE_BLOCK;

    for (int i = 0; i <= sb.root_directory_block; ++i)
        fat[i] = END_BLOCK;

    for (int i = 0; i < sb.fat_length; ++i)
        write_block(disk, (char *)fat + i * BLOCKSIZE, sb.fat_start + i);

    bzero(root_directory, sizeof(root_directory));

    // OPTIONAL TEST FILE:
    strcpy(root_directory[0].filename, "test.txt");
    root_directory[0].used = 1;
    root_directory[0].size = 0;
    root_directory[0].first_block = END_BLOCK;

    bzero(buffer, BLOCKSIZE);
    memcpy(buffer, root_directory, sizeof(root_directory));
    write_block(disk, buffer, sb.root_directory_block);

    fsync(disk);
    free(fat);

    printf("Formatted disk with %d blocks, FAT at block %d, root dir at block %d.\n",
           sb.num_blocks, sb.fat_start, sb.root_directory_block);
    return 0;
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("usage: make_bfat <diskname>\n");
        return 1;
    }

    strcpy(diskname, argv[1]);
    disk = open(diskname, O_RDWR);
    if (disk < 0) {
        perror("cannot open disk");
        return 1;
    }

    printf("opened disk...\n");
    bfat_format();
    printf("closing disk...\n");
    close(disk);

    return 0;
}


int read_block (int fd_disk, void *block, int k)
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


int write_block (int fd_disk, void *block, int k)
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