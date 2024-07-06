#define BLOCK_SIZE 64
#define FREE -1
#define EOF_BLOCK -2
#define TYPE_DIRECTORY 1
#define TYPE_FILE 2
#define NAME_MAX_LEN 16
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
    char name[NAME_MAX_LEN];
    int parent_block;
    int parent_offset;
    int first_block;
    int current_block;
    int last_block;
    int pos;
} FileHandle;

typedef struct {
    char name[NAME_MAX_LEN];
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
FileHandle* openFile(FileSystem* fs, char* filename);
void closeFile(FileSystem* fs, FileHandle* fh);
void writeFile(FileSystem* fs, FileHandle *fh, char *buf, int size);
void appendFile(FileSystem* fs, FileHandle* fh, char *buf, int size);
void readFile(FileSystem* fs, FileHandle *fh, char *buf, int size);
void seekFile(FileSystem* fs, FileHandle *fh, int offset, int whence);
void createDir(FileSystem* fs, char *dirname);
void eraseDir(FileSystem* fs, char *dirname);
void changeDir(FileSystem* fs, char *dirname);
void listDir(FileSystem* fs);

void printFAT(FATFileSystem* fs);
