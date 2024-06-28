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

