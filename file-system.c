#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
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
    new_entry->type = TYPE_FILE;
    new_entry->parent = current_dir;

    fs->current_dir->files[fs->current_dir->num_files] = new_entry;
    fs->current_dir->num_files++;
    if (fs->current_dir->start_index == -1) fs->current_dir->start_index = new_entry->start_index;

    FileHandle* entryfh = (FileHandle*) malloc (sizeof (FileHandle));
    entryfh->name = new_entry->name;
    entryfh->parent_name = current_dir->name;
    entryfh->last_block = block_found;
    entryfh->pos = 0;

    return entryfh;
}

void eraseFile(FATFileSystem* fs, FileHandle* fh) {
    Entry* current_dir = fs->current_dir;

    if (strcmp(current_dir->name, fh->parent_name) != 0) {
        printf("The file is not in this directory. Change to '%s' directory and try again\n", fh->parent_name);
        return;
    }

    for (int i=0; i<current_dir->num_files; i++) {
        if (strcmp(current_dir->files[i]->name, fh->name)==0) {
            int b = current_dir->files[i]->start_index;
            do {
                int temp = fs->FAT[b];
                memset(fs->data+(b*BLOCK_SIZE),'\0', BLOCK_SIZE);
                fs->FAT[b] = FREE;
                b = temp;
            } while (b != EOF_BLOCK);

            free(fs->current_dir->files[i]);

            for (int j = i; j < fs->current_dir->num_files - 1; j++) {
                fs->current_dir->files[j] = fs->current_dir->files[j + 1];
            }
            current_dir->num_files--;

            free(fh);

            return;
        }
    }
}

void writeFile(FATFileSystem* fs, FileHandle *fh, const void *buf, int size) {
    if (strcmp(fs->current_dir->name, fh->parent_name) != 0) {
        printf("The file is not in this directory. Change to '%s' directory and try again\n", fh->parent_name);
        return;
    }

    if (size < BLOCK_SIZE - (fh->pos % BLOCK_SIZE)) {
        memcpy (fs->data + (fh->last_block * BLOCK_SIZE), buf, size);
        fh->pos += size;
    }

    else {
        int written_bytes = 0;
        memcpy (fs->data + (fh->last_block * BLOCK_SIZE), buf, BLOCK_SIZE - (fh->pos % BLOCK_SIZE));
        written_bytes += (BLOCK_SIZE - (fh->pos % BLOCK_SIZE));
        fh->pos += written_bytes;
        size -= written_bytes;

        while (size > BLOCK_SIZE) {
            for (int i=0; i<NUM_BLOCKS; i++) {
                if (fs->FAT[i] == FREE) {
                    fs->FAT[fh->last_block] = i;
                    fs->FAT[i] = EOF_BLOCK;
                    fh->last_block = i;
                    break;
                }
            }
            memcpy (fs->data + (fh->last_block * BLOCK_SIZE), buf + written_bytes, BLOCK_SIZE);
            fh->pos += BLOCK_SIZE;
            written_bytes += BLOCK_SIZE;
            size -= BLOCK_SIZE;
        }

        memcpy (fs->data + (fh->last_block * BLOCK_SIZE), buf + written_bytes, size);
        fh->pos += size;
    }
}

int main(int argc, char *argv[]) {
    
    //Initialize the filesystem
    FATFileSystem* fs = (FATFileSystem*) malloc (sizeof (FATFileSystem));
    memset (fs->FAT, FREE, sizeof(fs->FAT));
    int fd = open ("filesystem", O_RDWR | O_CREAT, 0666);
    ftruncate(fd, BLOCK_SIZE*NUM_BLOCKS);
    fs->data = mmap(NULL, NUM_BLOCKS*BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close (fd);

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
   

    /*for (int i=0; i<NUM_BLOCKS*3; i++) {
        printf("%c, ", fs->data[i]);
    }
    printf("\n");*/
    
    FileHandle* f2 = createFile(fs, "file2.txt");

    writeFile(fs, f2, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", 180);

    
    FileHandle* f3 = createFile(fs, "file3.txt");

     writeFile(fs, f1, "questo è il mio primo file ciao a tuttiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii", 180);

    printf("[");
    for (int i=0; i<NUM_BLOCKS; i++) {
        printf("%d, ", fs->FAT[i]);
    }
    printf("]\n");

    for (int i=0; i<NUM_BLOCKS*20; i++) {
        printf("%c, ", fs->data[i]);
    }
    printf("\n");


     
    free(fs->current_dir->directories);
    free(fs->current_dir->files);
    free(fs->current_dir);
    munmap(fs->data, NUM_BLOCKS*BLOCK_SIZE);
    free(fs);
}
