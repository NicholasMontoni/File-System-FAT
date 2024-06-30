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
    entryfh->first_block = block_found;
    entryfh->current_block = block_found;
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

    //Cleaning the file
    int i = fh->first_block;
    while (i != EOF_BLOCK) {
        memset(fs->data + (i * BLOCK_SIZE), '\0', BLOCK_SIZE);
        int temp = fs->FAT[i];
        fs->FAT[i] = (i == fh->first_block) ? EOF_BLOCK : FREE;
        i = temp;
    }

    fh->pos = 0;
    int initial_position = fh->pos;
    fh->last_block = fh->first_block;
    int written_bytes = 0;

    while (size > 0) {
        int left_bytes = (size > BLOCK_SIZE) ? BLOCK_SIZE : size;
        memcpy (fs->data + (fh->last_block * BLOCK_SIZE), buf + written_bytes, left_bytes);
        written_bytes += left_bytes;
        size -= left_bytes;
        fh->pos += left_bytes;

        if (size != 0) {
            int found = 0;
            for (int i=0; i<NUM_BLOCKS; i++) {
                if (fs->FAT[i] == FREE) {
                    found = 1;
                    fs->FAT[fh->last_block] = i;
                    fs->FAT[i] = EOF_BLOCK;
                    fh->current_block = i;
                    fh->last_block = i;
                    break;
                }
            }
            
            if (!found) {
                printf ("Buf was too big, no more space left. Only first %d bytes of buf have been written!\n",(fh->pos-initial_position));
                return;
            }
        }
    }
}

void appendFile(FATFileSystem* fs, FileHandle *fh, const void *buf, int size) {
    if (strcmp(fs->current_dir->name, fh->parent_name) != 0) {
        printf("The file is not in this directory. Change to '%s' directory and try again\n", fh->parent_name);
        return;
    }

    int initial_position = fh->pos;
    int offset = fh->pos % BLOCK_SIZE;

    //Cleaning the file after position 
    int i = fh->current_block;
    while (i != EOF_BLOCK) {
        (i == fh->current_block)? memset(fs->data + (i * BLOCK_SIZE) + offset, '\0', BLOCK_SIZE - offset) : memset(fs->data + (i * BLOCK_SIZE), '\0', BLOCK_SIZE);
        int temp = fs->FAT[i];
        fs->FAT[i] = (i == fh->current_block) ? EOF_BLOCK : FREE;
        i = temp;
    }

    //Writing 

    if (size < BLOCK_SIZE - offset) {
        memcpy (fs->data + (fh->current_block * BLOCK_SIZE) + offset, buf, size);
        fh->pos += size;
        return;
    }

    memcpy (fs->data + (fh->current_block * BLOCK_SIZE) + offset-1, buf, BLOCK_SIZE - offset);
    int written_bytes = (BLOCK_SIZE - offset);
    fh->pos += written_bytes;
    size -= written_bytes;

    while (size > 0) {
        int found = 0;
        for (int i=0; i<NUM_BLOCKS; i++) {
            if (fs->FAT[i] == FREE) {
                found = 1;
                fs->FAT[fh->last_block] = i;
                fs->FAT[i] = EOF_BLOCK;
                fh->current_block = i;
                fh->last_block = i;
                break;
            }
        }
        
        if (!found) {
            printf ("Buf was too big, no more space left. Only first %d bytes of buf have been written!\n",(fh->pos-initial_position));
            return;
        }

        int left_bytes = (size > BLOCK_SIZE) ? BLOCK_SIZE : size;
        memcpy (fs->data + (fh->last_block * BLOCK_SIZE)-1, buf + written_bytes, left_bytes);
        written_bytes += left_bytes;
        size -= left_bytes;
        fh->pos += left_bytes;
    }
}

void readFile(FATFileSystem* fs, FileHandle *fh, void *buf, int size) {
    if (strcmp(fs->current_dir->name, fh->parent_name) != 0) {
        printf("The file is not in this directory. Change to '%s' directory and try again\n", fh->parent_name);
        return;
    }
    
    int offset = fh->pos % BLOCK_SIZE;
    if (*(fs->data + (fh->current_block * BLOCK_SIZE) + offset) == '\0') {
        printf("Nothing to read\n");
        return;
    }

    if (size < BLOCK_SIZE - offset) {
        memcpy(buf, fs->data + (fh->current_block * BLOCK_SIZE) + offset, size);
        fh->pos += size;
        return;
    }

    memcpy(buf, fs->data + (fh->current_block * BLOCK_SIZE) + offset, BLOCK_SIZE - offset);
    int read_bytes = BLOCK_SIZE - offset;
    fh->pos += read_bytes;
    size -= read_bytes;

    while (size > 0) {   
        if (fs->FAT[fh->current_block] == EOF_BLOCK) return;   
        int block = fh->current_block;
        fh->current_block = fs->FAT[block];

        int left_bytes = (size > BLOCK_SIZE) ? BLOCK_SIZE : size;
        memcpy (buf + read_bytes, fs->data + (fh->current_block * BLOCK_SIZE), left_bytes);
        read_bytes += left_bytes;
        size -= left_bytes;
        fh->pos += left_bytes;
    }
}

void seekFile(FATFileSystem* fs, FileHandle *fh, int offset, int whence) {
    if (strcmp(fs->current_dir->name, fh->parent_name) != 0) {
        printf("The file is not in this directory. Change to '%s' directory and try again\n", fh->parent_name);
        return;
    }

    //whence == 0 -> initial_position
    if (whence == 0) {
        fh->pos = 0;
        fh->current_block = fh->first_block;

        if (offset <= 0) return;
        
        while (offset > 0) {
            int block = fh->current_block;
            if (fs->FAT[block] == EOF_BLOCK) return;
            
            if (offset > BLOCK_SIZE+1) fh->current_block = fs->FAT[block];
            offset -= (offset > BLOCK_SIZE+1) ? BLOCK_SIZE+1 : offset;
            fh->pos += (offset > BLOCK_SIZE+1) ? BLOCK_SIZE+1 : offset;
        }
    }

    //whence == 1 -> current_position
    else if (whence == 1) {
        seekFile(fs, fh, fh->pos + offset, 0);
    }

    //whence == 2 -> end_position
    else if (whence == 2) {
        fh->pos = 0;
        fh-> current_block = fh->last_block;
        int block = fh->first_block;
        
        while (block != fh->last_block) {
            fh->pos += BLOCK_SIZE;
            block = fs->FAT[block];
        }

        char* temp = fs->data + (fh->last_block * BLOCK_SIZE);
        while (*temp) {
            fh->pos++;
            temp++;
        }

        if (offset >= 0) return;

        seekFile(fs, fh, fh->pos + offset, 0);
    }
}