#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "file-system.h"

//UTILITIES
int allocate_block(FATFileSystem* fs) {
    int* pblock = (int*)fs->FAT->free_list; 
    int block = *pblock;
    fs->FAT->free_blocks--;
    fs->FAT->free_list++;

    return block;
}

void free_block(FATFileSystem* fs, int block) {
    fs->FAT->free_blocks++;
    fs->FAT->free_list--;
    int* pblock = (int*)fs->FAT->free_list; 
    *pblock = block;
}

void printFAT(FATFileSystem* fs) {
    printf("[");

    for (int i=0; i<63; i++) {
        printf("%d, ", fs->FAT->FAT[i]);
    }
    printf("%d]\n", fs->FAT->FAT[63]);
}

void defragment(FileSystem* fs, int current_block, int previous_block, int pos) {

    while (1) {
        for (int i = (pos/32); i < (BLOCK_SIZE/32)-1; i++) {
            DirEntry* current_entry = (DirEntry*)(fs->FATfs->data + (current_block * BLOCK_SIZE) + i*32);
            DirEntry* next_entry = (DirEntry*)(fs->FATfs->data + (current_block * BLOCK_SIZE) + (i+1)*32);
            memcpy(current_entry, next_entry, 32);
        }
        memset(fs->FATfs->data + (current_block * BLOCK_SIZE) + (BLOCK_SIZE - 32), '\0', 32);

        if  ((*(fs->FATfs->data + (current_block * BLOCK_SIZE)) == '\0') && (*(fs->FATfs->data + (current_block * BLOCK_SIZE) + 32) == '\0')) {
            fs->FATfs->FAT->FAT[current_block] = FREE;
            fs->FATfs->FAT->FAT[previous_block] = EOF_BLOCK;
            free_block(fs->FATfs, current_block);
            int* parent_last_block = (int*)(fs->FATfs->data + (fs->current_dir * BLOCK_SIZE) + LAST_BLOCK_OFFSET);
            *parent_last_block = previous_block;
            return;
        }

        previous_block = current_block;
        current_block = fs->FATfs->FAT->FAT[previous_block];

        if(current_block == EOF_BLOCK) {
            return;
        }
        
        memcpy(fs->FATfs->data + (previous_block * BLOCK_SIZE) + (BLOCK_SIZE - 32), fs->FATfs->data + (current_block * BLOCK_SIZE), 32);
        memset(fs->FATfs->data + (current_block * BLOCK_SIZE), '\0', 32);
        pos = 0;
    }

    
}

//LOAD FILE SYSTEM
FileSystem* loadFileSystem(char* name, int size_requested) {
    int blocks_required = (size_requested + BLOCK_SIZE -1) /BLOCK_SIZE;
    int total_size = sizeof(FATFileSystem) + sizeof(FATStruct) + blocks_required*8 + blocks_required*BLOCK_SIZE; 
    
    FileSystem* fs = (FileSystem*) malloc(sizeof(FileSystem));
    fs->current_dir = 0;

    int fd = open (name, O_RDWR | O_CREAT, 0666);
    
    if (fd == -1) {
        perror("Error opening filesystem");
        exit(EXIT_FAILURE);
    }

    int len = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    //Filesystem is not empty
    if (len > 0) {
        fs->FATfs = (FATFileSystem*) mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);   
        if (fs->FATfs == MAP_FAILED) {
            perror("Error mapping filesystem");
            exit(EXIT_FAILURE);
        }        

        fs->total_size = len;

        char* start_ptr = (char*)fs->FATfs + sizeof(FATFileSystem);
        fs->FATfs->FAT = (FATStruct*) start_ptr;
        int free_size = *((int*)(start_ptr + 16));
        fs->FATfs->FAT->free_list = (int*)(start_ptr + sizeof(FATStruct) + (blocks_required - free_size)*4);
        fs->FATfs->FAT->FAT = (int*)(start_ptr + sizeof(FATStruct) + blocks_required*4);
        fs->FATfs->data = start_ptr + sizeof(FATStruct) + blocks_required*8;
        
        //Not enough space left in filesystem        
        if (fs->FATfs->FAT->free_blocks < blocks_required) {
            printf ("Space left in this file system is not enough. Bytes left = %d\n", fs->FATfs->FAT->free_blocks * BLOCK_SIZE);    
        }
    }

    //Empty filesystem
    else {
        if (ftruncate(fd, total_size) == -1) {
            perror("Error truncating filesystem");
            exit(EXIT_FAILURE);
        }

        fs->FATfs = (FATFileSystem*) mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (fs->FATfs == MAP_FAILED) {
            perror("Error mapping filesystem");
            exit(EXIT_FAILURE);
        }

        fs->total_size = total_size;

        char* start_ptr = (char*)fs->FATfs + sizeof(FATFileSystem);
        fs->FATfs->FAT = (FATStruct*) start_ptr;
        fs->FATfs->FAT->free_list = (int*)(start_ptr + sizeof(FATStruct));
        fs->FATfs->FAT->FAT = (int*)(start_ptr + sizeof(FATStruct) + blocks_required*4);
        fs->FATfs->FAT->free_blocks = blocks_required;
        fs->FATfs->data = start_ptr + sizeof(FATStruct) + blocks_required*8;

        for (int i=0; i<blocks_required; i++) {
            fs->FATfs->FAT->free_list[i] = i;
        }

        memset(fs->FATfs->FAT->FAT, FREE, blocks_required*4);

        //Setting root directory (by default in block 0 of FAT)
        allocate_block(fs->FATfs);
        fs->FATfs->FAT->FAT[0] = EOF_BLOCK;

        DirEntry* root = (DirEntry*)fs->FATfs->data;
        strncpy(root->name, "root", 16);
        root->start_block = 0;
        root->last_block = 0;
        root->type = TYPE_DIRECTORY;
        root->size = 32;
    }

    close(fd);
    return fs;
}

void unloadFileSystem(FileSystem* fs) { 
    if (munmap(fs->FATfs, fs->total_size) == -1) {
        perror("Error unmapping filesystem");
        exit(EXIT_FAILURE);
    }
    free(fs);
}
 
void createFile(FileSystem* fs, char *filename) {
    int* parent_size = (int*)(fs->FATfs->data + (fs->current_dir * BLOCK_SIZE) + SIZE_OFFSET);
    int block = fs->current_dir;
    int* parent_last_block = (int*)(fs->FATfs->data + (fs->current_dir * BLOCK_SIZE) + LAST_BLOCK_OFFSET);

    while (block != EOF_BLOCK) {
        for (int i = 0; i < BLOCK_SIZE; i += 32) {
            int type = *((int*)fs->FATfs->data + (block * BLOCK_SIZE) + i + TYPE_OFFSET);

            if (type == TYPE_DIRECTORY) continue;

            char name[16];
            strncpy(name, fs->FATfs->data + (block * BLOCK_SIZE) + i, 16);

            if(strcmp(filename, name) == 0) {
                printf("File '%s' already exist in this directory!\n", filename);
                return;
            } 
        }
        block = fs->FATfs->FAT->FAT[block];
    }
    
    block = 0;
    int offset = (*parent_size) % BLOCK_SIZE;

    if (offset == 0) {
        block = allocate_block(fs->FATfs);
        fs->FATfs->FAT->FAT[*parent_last_block] = block;
        fs->FATfs->FAT->FAT[block] = EOF_BLOCK;
        *parent_last_block = block;
    }
    else {
        block = *parent_last_block;
    }

    DirEntry* new_file = (DirEntry*)(fs->FATfs->data + (block * BLOCK_SIZE) + offset);
    strncpy(new_file->name, filename, 16);
    new_file->name[15] = '\0';
    new_file->type = TYPE_FILE;
    new_file->size = 0;
    *parent_size += 32;
}

void eraseFile(FileSystem* fs, char* filename) {
    int block = fs->current_dir;
    int previous_block = block;

    while (block != EOF_BLOCK) {
        for (int i = 0; i < BLOCK_SIZE; i += 32) {
            int type = *((int*)(fs->FATfs->data + (block * BLOCK_SIZE) + i + TYPE_OFFSET));

            if (type == TYPE_DIRECTORY) continue;

            char name[16];
            strncpy(name, fs->FATfs->data + (block * BLOCK_SIZE) + i, 16);

            if(strcmp(filename, name) == 0) {
                defragment(fs, block, previous_block, i);
                int* parent_size = (int*)(fs->FATfs->data + (fs->current_dir * BLOCK_SIZE) + SIZE_OFFSET);
                *parent_size -= 32;

                int* file_size = (int*)(fs->FATfs->data + (block * BLOCK_SIZE) + i + SIZE_OFFSET);
                
                if (*file_size != 0) {
                    int start = *((int*)(fs->FATfs->data + (block * BLOCK_SIZE) + i + START_BLOCK_OFFSET));

                    while (start != EOF_BLOCK) {
                        memset(fs->FATfs->data + (start * BLOCK_SIZE), '\0', BLOCK_SIZE);
                        free_block(fs->FATfs, start);
                        start = fs->FATfs->FAT->FAT[start];
                    }
                }

                return;
            } 
        }
        previous_block = block;
        block = fs->FATfs->FAT->FAT[previous_block];
    }

    printf("No file named %s in this directory\n", filename);
}

/*void writeFile(FATFileSystem* fs, FileHandle *fh, const void *buf, int size) {
    if (fh == NULL) {
        printf("Invalid file\n");
        return;
    }

    if (strcmp(fs->current_dir->name, fh->parent_name) != 0) {
        printf("The file is not in this directory. Change to '%s' directory and try again\n", fh->parent_name);
        return;
    }

    //Cleaning the previous contents of the file
    int i = fh->first_block;
    while (i != EOF_BLOCK) {
        memset(fs->data + (i * BLOCK_SIZE), '\0', BLOCK_SIZE);
        int temp = fs->FAT[i];
        fs->FAT[i] = (i == fh->first_block) ? EOF_BLOCK : FREE;
        i = temp;
    }

    //Writing from the beginning 
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

    if (fh == NULL) {
        printf("Invalid file\n");
        return;
    }

    if (strcmp(fs->current_dir->name, fh->parent_name) != 0) {
        printf("The file is not in this directory. Change to '%s' directory and try again\n", fh->parent_name);
        return;
    }

    int initial_position = fh->pos;
    int offset = fh->pos % BLOCK_SIZE;

    //Cleaning the previous contents of the file after current position
    int i = fh->current_block;
    while (i != EOF_BLOCK) {
        (i == fh->current_block)? memset(fs->data + (i * BLOCK_SIZE) + offset, '\0', BLOCK_SIZE - offset) : memset(fs->data + (i * BLOCK_SIZE), '\0', BLOCK_SIZE);
        int temp = fs->FAT[i];
        fs->FAT[i] = (i == fh->current_block) ? EOF_BLOCK : FREE;
        i = temp;
    }

    //Writing from current position
    if (size < BLOCK_SIZE - offset) {
        memcpy (fs->data + (fh->current_block * BLOCK_SIZE) + offset-1, buf, size);
        fh->pos += size;
        return;
    }

    memcpy (fs->data + (fh->current_block * BLOCK_SIZE) + offset-1, buf, BLOCK_SIZE - offset+1);
    int written_bytes = (BLOCK_SIZE - offset + 1);
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
        memcpy (fs->data + (fh->last_block * BLOCK_SIZE), buf + written_bytes, left_bytes);
        written_bytes += left_bytes;
        size -= left_bytes;
        fh->pos += left_bytes;
    }
}

void readFile(FATFileSystem* fs, FileHandle *fh, void *buf, int size) {

    if (fh == NULL) {
        printf("Invalid file\n");
        return;
    }

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
        int block = fh->current_block;
        fh->current_block = fs->FAT[block];

        int left_bytes = (size > BLOCK_SIZE) ? BLOCK_SIZE : size;
        memcpy (buf + read_bytes, fs->data + (fh->current_block * BLOCK_SIZE), left_bytes);
        read_bytes += left_bytes;
        size -= left_bytes;
        fh->pos += left_bytes;

        if (fh->current_block == fh->last_block) return;
    }
}

void seekFile(FATFileSystem* fs, FileHandle *fh, int offset, int whence) {

    if (fh == NULL) {
        printf("Invalid file\n");
        return;
    }

    if (strcmp(fs->current_dir->name, fh->parent_name) != 0) {
        printf("The file is not in this directory. Change to '%s' directory and try again\n", fh->parent_name);
        return;
    }

    if (*(fs->data + fh->first_block * BLOCK_SIZE) == '\0') {
        return;        
    }

    //whence == 0 -> initial_position
    if (whence == 0) {   
        fh->pos = 0;
        fh->current_block = fh->first_block;

        if (offset <= 0) return;
        
        while (offset > 0) {
            int block = fh->current_block;
            if (block == EOF_BLOCK) return;
            
            if (offset > BLOCK_SIZE) fh->current_block = fs->FAT[block];
            int val = (offset > BLOCK_SIZE) ? BLOCK_SIZE+1 : offset;
            offset -= val;
            fh->pos += val;
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

void createDir(FATFileSystem* fs, char *dirname) {
    if (fs->current_dir->num_directories == MAX_NUM_FILES) {
        printf("Current directory is full\n");
        return;
    };

    for (int i=0; i<fs->current_dir->num_directories; i++) {
        if (strcmp(fs->current_dir->directories[i]->name, dirname)==0) {
            printf("Directory '%s' already exists in current directory\n", dirname);
            return;
        }
    }

    //Creating new directory
    Entry* new_entry = (Entry*) malloc(sizeof(Entry));
    new_entry->name = dirname;
    new_entry->num_files = 0;
    new_entry->num_directories = 0;
    new_entry->parent = fs->current_dir;
    new_entry->directories = (Entry**) malloc(MAX_NUM_FILES * sizeof(Entry*));
    new_entry->files_handlers = (FileHandle**) malloc(MAX_NUM_FILES * sizeof(FileHandle*));


    fs->current_dir->directories[fs->current_dir->num_directories] = new_entry;
    fs->current_dir->num_directories++;
}

void eraseDir(FATFileSystem* fs, char *dirname) {
    Entry* current_dir = fs->current_dir;

    int ok = 0;
    for (int i=0; i<current_dir->num_directories; i++) {
        if (strcmp(current_dir->directories[i]->name, dirname)==0) {
            Entry* dir = current_dir->directories[i];
            fs->current_dir = dir;
        
            //Deleting files inside the directory
            while (fs->current_dir->num_files > 0) {
                eraseFile(fs, fs->current_dir->files_handlers[0]);
            }
            
            //Deleting directories inside the directory
            while (fs->current_dir->num_directories > 0) {
                eraseDir(fs, fs->current_dir->directories[0]->name);
            }

            //Freeing the directory
            free(dir->directories);
            free(dir->files_handlers);
            free(dir);

            fs->current_dir = current_dir;

            for (int k = i; k < fs->current_dir->num_directories - 1; k++) {
                fs->current_dir->directories[k] = fs->current_dir->directories[k + 1];
            }
            fs->current_dir->directories[fs->current_dir->num_directories-1] = NULL;
            fs->current_dir->num_directories--;
            
            ok = 1;
            break;
        }
    }

    if (!ok) {
        printf("No directory called '%s' in current directory!\n", dirname);
        return;
    }
}

void changeDir(FATFileSystem* fs, char *dirname) {
    Entry* current_dir = fs->current_dir;

    // dirname == "." -> remain in current directory
    if (strcmp(dirname, ".") == 0) {
        return;
    }

    // dirname == ".." -> move up to parent directory
    if (strcmp(dirname, "..") == 0) {
        fs->current_dir = current_dir->parent;
        return;
    }
    
    int ok = 0;
    for (int i=0; i<current_dir->num_directories; i++) {
        if (strcmp(current_dir->directories[i]->name, dirname)==0) {
            fs->current_dir = current_dir->directories[i];
            ok = 1;
            break;
        }
    }

    if (!ok) {
        printf("No directory called '%s' in current directory!\n", dirname);
        return;
    }
}

void listDir(FATFileSystem* fs) {
    Entry* current_dir = fs->current_dir;

    //Listing directories inside current directory
    printf("Directories inside '%s' directory:\n", current_dir->name);

    for (int i = 0; i < current_dir->num_directories; i++) {
        printf("%s\n", current_dir->directories[i]->name);
    }

    //Listing files inside current directory
    printf("Files inside '%s' directory:\n", current_dir->name);

    for (int j = 0; j < current_dir->num_files; j++) {
        printf("%s\n", current_dir->files_handlers[j]->name);
    }
}*/