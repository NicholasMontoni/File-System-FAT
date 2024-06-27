#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "file-system.h"

int main(int argc, char *argv[]) {
    
    //Initialize the filesystem
    FATFileSystem* fs = (FATFileSystem*) malloc (sizeof (FATFileSystem));
    memset (fs->FAT, FREE, sizeof(fs->FAT));
    int fd = open ("filesystem", O_RDWR | O_CREAT, 0666);
    fs->data = mmap(NULL, NUM_BLOCKS*BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    
    fs->root = (Entry*) malloc (sizeof(Entry));
    fs->root->name = "root";
    fs->root->start_index = 0;
    fs->FAT[0] = EOF;
    fs->root->type = TYPE_DIRECTORY;
    fs->root->num_files = 0;
    fs->root->num_directories = 0;
    fs->root->parent = NULL;
    fs->root->entries = NULL;


}
