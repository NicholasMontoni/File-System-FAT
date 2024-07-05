#define BLOCK_SIZE 64
#define FREE -1
#define EOF_BLOCK -2
#define TYPE_DIRECTORY 1
#define TYPE_FILE 2
#define START_BLOCK_OFFSET 16
#define LAST_BLOCK_OFFSET 20
#define TYPE_OFFSET 24
#define SIZE_OFFSET 28

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
    char name[16];
    int start_block;
    int last_block;
    int type;
    int size;
} DirEntry;

typedef struct {
    FATFileSystem* FATfs;
    int current_dir;
    int total_size;
} FileSystem;


FileSystem* loadFileSystem(char* name, int size_requested);
void unloadFileSystem(FileSystem* fs);
void createFile(FileSystem* fs, char *filename);
void eraseFile(FileSystem* fs, char* filename);
//OPEN FILE missing
void writeFile(FATFileSystem* fs, FileHandle *fh, const void *buf, int size);
void appendFile(FATFileSystem* fs, FileHandle* fh, const void *buf, int size);
void readFile(FATFileSystem* fs, FileHandle *fh, void *buf, int size);
void seekFile(FATFileSystem* fs, FileHandle *fh, int offset, int whence);
void createDir(FATFileSystem* fs, char *dirname);
void eraseDir(FATFileSystem* fs, char *dirname);
void changeDir(FATFileSystem* fs, char *dirname);
void listDir(FATFileSystem* fs);

void printFAT(FATFileSystem* fs);
