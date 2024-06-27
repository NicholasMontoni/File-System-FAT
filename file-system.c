#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "file-system.h"
 
FileHandle* createFile(FATFileSystem* fs, char *filename) {
    Entry* current_dir = fs->current_dir;
    if (current_dir->num_files == MAX_NUM_FILES) {
        printf("Current directory is full\n");
        return NULL;
    };

    for (int i=0; i<current_dir->num_files; i++) {
        if (strcmp(current_dir->files[i]->name, filename)==0) {
            printf("File already exists\n");
            return NULL;
        }
    }

    int block_found = -1;
    for (int i=0; i<NUM_BLOCKS; i++) {
        if (fs->FAT[i]==FREE) {
            block_found = i;
            fs->FAT[i]=EOF_BLOCK;
            break;
        }
    }

    if (block_found == -1) {
        printf("No more space\n");
        return NULL;
    }

    Entry* new_entry = (Entry*) malloc(sizeof(Entry));
    new_entry->name = filename;
    new_entry->start_index = block_found;
    new_entry->size = 0;
    new_entry->type = TYPE_FILE;
    new_entry->parent = current_dir;

    fs->current_dir->files[fs->current_dir->num_files] = new_entry;
    fs->current_dir->num_files++;
    if (fs->current_dir->start_index == -1) fs->current_dir->start_index = new_entry->start_index;

    FileHandle* entryfh = (FileHandle*) malloc (sizeof (FileHandle));
    entryfh->parent_name = current_dir->name;
    entryfh->pos = 0;

    return entryfh;
}

void eraseFile(FATFileSystem* fs, const char *filename) {
    Entry* current_dir = fs->current_dir;
    for (int i=0; i<current_dir->num_files; i++) {
        if (strcmp(current_dir->files[i]->name, filename)==0) {
            int b = current_dir->files[i]->start_index;
            do {
                int temp = fs->FAT[b];
                fs->FAT[b] = FREE;
                b = temp;
            } while (b != EOF_BLOCK);

            free(fs->current_dir->files[i]);

            for (int j = i; j < fs->current_dir->num_files - 1; j++) {
                fs->current_dir->files[j] = fs->current_dir->files[j + 1];
            }
            current_dir->num_files--; 
            return;
        }
    }
    printf("File not found\n");
}


int main(int argc, char *argv[]) {
    
    //Initialize the filesystem
    FATFileSystem* fs = (FATFileSystem*) malloc (sizeof (FATFileSystem));
    memset (fs->FAT, FREE, sizeof(fs->FAT));
    int fd = open ("filesystem", O_RDWR | O_CREAT, 0666);
    fs->data = mmap(NULL, NUM_BLOCKS*BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    fs->current_dir = (Entry*) malloc (sizeof(Entry));
    fs->current_dir->name = "root";
    fs->current_dir->start_index = -1;
    fs->current_dir->type = TYPE_DIRECTORY;
    fs->current_dir->num_files = 0;
    fs->current_dir->num_directories = 0;
    fs->current_dir->parent = NULL;
    fs->current_dir->directories = (Entry**) malloc(MAX_NUM_FILES * sizeof(Entry*));
    fs->current_dir->files = (Entry**) malloc(MAX_NUM_FILES * sizeof(Entry*));

    FileHandle* f1 = createFile(fs, "file1.txt");
    FileHandle* f2 = createFile(fs, "file2.txt");
    FileHandle* f3 = createFile(fs, "file3.txt");

    printf("[");
    for (int i=0; i<NUM_BLOCKS; i++) {
        printf("%d, ", fs->FAT[i]);
    }
    printf("]\n");

    printf("%s\n", f2->parent_name);

    eraseFile(fs, "file2.txt");
    eraseFile(fs, "file1.txt");

    printf("[");
    for (int i=0; i<NUM_BLOCKS; i++) {
        printf("%d, ", fs->FAT[i]);
    }
    printf("]\n");

    eraseFile(fs, "file4.txt");
}
