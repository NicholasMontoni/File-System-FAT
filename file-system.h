#define MAX_NUM_FILES 100
#define NUM_BLOCKS 1024
#define BLOCK_SIZE 40
#define FREE -1
#define EOF_BLOCK -2

typedef struct {
    int* free_list;
    int* FAT;
    int free_blocks;
} FATStruct;

typedef struct {
    FATStruct* FAT;
    char* data;
} FATFileSystem;

typedef struct {
    char* name;
    int parent_block;
    int first_block;
    int current_block;
    int last_block;
    int pos;
} FileHandle;

typedef struct {
    FATFileSystem* FATfs;
    int current_dir;
} FileSystem;


FileSystem* loadFileSystem(char* name, int size_requested);
void endFileSystem(FATFileSystem* fs);
FileHandle* createFile(FATFileSystem* fs, char *filename);
void eraseFile(FATFileSystem* fs, FileHandle* fh);
//OPEN FILE missing
void writeFile(FATFileSystem* fs, FileHandle *fh, const void *buf, int size);
void appendFile(FATFileSystem* fs, FileHandle* fh, const void *buf, int size);
void readFile(FATFileSystem* fs, FileHandle *fh, void *buf, int size);
void seekFile(FATFileSystem* fs, FileHandle *fh, int offset, int whence);
void createDir(FATFileSystem* fs, char *dirname);
void eraseDir(FATFileSystem* fs, char *dirname);
void changeDir(FATFileSystem* fs, char *dirname);
void listDir(FATFileSystem* fs);
