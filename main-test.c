#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "file-system.h"

int main(int argc, char *argv[]) {
    //Initialize the filesystem
    FATFileSystem* fs = (FATFileSystem*) malloc (sizeof (FATFileSystem));
    memset (fs->FAT, FREE, sizeof(fs->FAT));
    int fd = open ("filesystem", O_RDWR | O_CREAT, 0666);
    ftruncate(fd, BLOCK_SIZE*NUM_BLOCKS);
    fs->data = mmap(NULL, NUM_BLOCKS*BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    memset(fs->data, '\0', BLOCK_SIZE*NUM_BLOCKS);
    close (fd);

    fs->current_dir = (Entry*) malloc (sizeof(Entry));
    fs->current_dir->name = "root";
    fs->current_dir->start_index = -1;
    fs->current_dir->type = TYPE_DIRECTORY;
    fs->current_dir->num_files = 0;
    fs->current_dir->num_directories = 0;
    fs->current_dir->parent = NULL;
    fs->current_dir->directories = (Entry**) malloc(MAX_NUM_FILES * sizeof(Entry*));
    fs->current_dir->files_handlers = (FileHandle**) malloc(MAX_NUM_FILES * sizeof(Entry*));

    FileHandle* f1 = createFile(fs, "file1.txt");
   

    /*for (int i=0; i<NUM_BLOCKS*3; i++) {
        printf("%c, ", fs->data[i]);
    }
    printf("\n");*/
    
    FileHandle* f2 = createFile(fs, "file2.txt");

    writeFile(fs, f2, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", 180);

    
    FileHandle* f3 = createFile(fs, "file3.txt");

    writeFile(fs, f1, "questo e il mio primo file ciao a tuttiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii", 91);
    appendFile(fs, f1, "Ciao a tutti, questo e il mio primo file, volevo solo sapere come vaaaaaaaaaaaaaaaaa??", 87);

    /*printf("[");
    for (int i=0; i<NUM_BLOCKS; i++) {
        printf("%d, ", fs->FAT[i]);
    }
    printf("]\n");*/

    /*for (int i=0; i<NUM_BLOCKS*20; i++) {
        printf("%c, ", fs->data[i]);
    }
    printf("\n");*/

    //eraseFile(fs,f2);

    /*printf("[");
    for (int i=0; i<NUM_BLOCKS; i++) {
        printf("%d, ", fs->FAT[i]);
    }
    printf("]\n");

    for (int i=0; i<NUM_BLOCKS*20; i++) {
        printf("%c, ", fs->data[i]);
    }
    printf("\n"); */

    char* buf1 = (char*) malloc(300);
    //char* buf2 = (char*) malloc(200);
    
    //readFile(fs, f2, buf2, 100);
    seekFile(fs, f1, 90, 0);
    seekFile(fs, f1, -56, 1);
    readFile(fs, f1, buf1, 300);

    while(*buf1) {
        printf("%c", *buf1);
        buf1++;
    }
    printf("\n");


     
    free(fs->current_dir->directories);
    free(fs->current_dir->files);
    free(fs->current_dir);
    munmap(fs->data, NUM_BLOCKS*BLOCK_SIZE);
    free(fs);
}