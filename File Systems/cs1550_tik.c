/*
	FUSE: Filesystem in Userspace
	Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

	This program can be distributed under the terms of the GNU GPL.
	See the file COPYING.

	gcc -Wall `pkg-config fuse --cflags --libs` cs1550.c -o cs1550
*/

#define	FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

//size of a disk block
#define	BLOCK_SIZE 512
#define BITMAP_LENGTH ((5242880/512)/8)
//we'll use 8.3 filenames
#define	MAX_FILENAME 8
#define	MAX_EXTENSION 3
#define BITMAP_LAST (5*BLOCK_SIZE - 1)

//How many files can there be in one directory?
#define	MAX_FILES_IN_DIR (BLOCK_SIZE - (MAX_FILENAME + 1) - sizeof(int)) / \
	((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long))

//How much data can one block hold?
#define	MAX_DATA_IN_BLOCK (BLOCK_SIZE - sizeof(unsigned long))

struct cs1550_directory_entry
{
	int nFiles;	//How many files are in this directory.
				//Needs to be less than MAX_FILES_IN_DIR

	struct cs1550_file_directory
	{
		char fname[MAX_FILENAME + 1];	//filename (plus space for nul)
		char fext[MAX_EXTENSION + 1];	//extension (plus space for nul)
		size_t fsize;					//file size
		long nStartBlock;				//where the first block is on disk
	} __attribute__((packed)) files[MAX_FILES_IN_DIR];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.  
	char padding[BLOCK_SIZE - MAX_FILES_IN_DIR * sizeof(struct cs1550_file_directory) - sizeof(int)];
} ;

typedef struct cs1550_directory_entry cs1550_directory_entry;

#define MAX_DIRS_IN_ROOT (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + sizeof(long))

struct cs1550_bitmap
{
    unsigned char bitmap[5*BLOCK_SIZE];
};

struct cs1550_root_directory
{
	int nDirectories;	//How many subdirectories are in the root
						//Needs to be less than MAX_DIRS_IN_ROOT
	struct cs1550_directory
	{
		char dname[MAX_FILENAME + 1];	//directory name (plus space for nul)
		long nStartBlock;				//where the directory block is on disk
	} __attribute__((packed)) directories[MAX_DIRS_IN_ROOT];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.  
	char padding[BLOCK_SIZE - MAX_DIRS_IN_ROOT * sizeof(struct cs1550_directory) - sizeof(int)];
} ;

typedef struct cs1550_root_directory cs1550_root_directory;

struct cs1550_disk_block
{
	//The first 4 bytes will be the value 0xF113DA7A 
	unsigned long magic_number;
	//And all the rest of the space in the block can be used for actual data
	//storage.
	char data[MAX_DATA_IN_BLOCK];
};

typedef struct cs1550_disk_block cs1550_disk_block;

//How many pointers in an inode?
#define NUM_POINTERS_IN_INODE (BLOCK_SIZE - sizeof(unsigned int) - sizeof(unsigned long))

struct cs1550_inode
{
	//The first 4 bytes will be the value 0xFFFFFFFF
	unsigned long magic_number;
	//The number of children this node has (either other inodes or data blocks)
	unsigned int children;
	//An array of disk pointers to child nodes (either other inodes or data)
	unsigned long pointers[NUM_POINTERS_IN_INODE];
};

typedef struct cs1550_inode cs1550_inode;

static void parsing(const char* path, char* dname, char* fname, char* fext)
{
	memset(dname, 0, MAX_FILENAME + 1);
	memset(fname, 0, MAX_FILENAME + 1);
	memset(fext, 0, MAX_FILENAME + 1);
	sscanf(path, "/%[^/]/%[^.].%s", dname, fname, fext);
}
static long find_free_block(FILE* f, unsigned char* bitmap)
{
    int res = 0;
	int x = 1;
    for(; x < BITMAP_LAST; x++)
        if(bitmap[x] < 255) //meaning there is an empty block.
        {
            unsigned char mask = bitmap[x] + 1;
            unsigned char count = 128;
            for(res = 7; (count ^ mask) != 0; count >>= 1, res--);
            res += (x*8);
            bitmap[x] = bitmap[x] | mask;
            printf("location found.\n");
            break;
        }
    if(fwrite(bitmap, 1, 5*BLOCK_SIZE, f) == BITMAP_LAST + 1)
        printf("bitmap updated.\n");
    return res;
}
static int addFile(cs1550_directory_entry *ent, int x)
{
	FILE *fd = fopen(".directories", "rb+");
	fseek(fd, sizeof(cs1550_directory_entry)*x, SEEK_SET);
	fwrite(ent, sizeof(cs1550_directory_entry), 1, fd);
	fclose(fd);
	return 0;
}
static long disk_offset_dir(FILE* f, char* dname)
{
	cs1550_root_directory root;
	if(fread(&root, 1, BLOCK_SIZE, f) == BLOCK_SIZE)
	{
		int numDirs = root.nDirectories;
		int x = 0;
		printf("%d directories on disk. \n", numDirs);
		for(x=0; x<numDirs; x++)
		{
			if(strcmp(dname, root.directories[x].dname)==0)
				return root.directories[x].nStartBlock;
		}
		return 0;
	}
}
static long disk_offset_ext(cs1550_directory_entry* dir, char* fname, char* fext, int* ind)
{
	int numFiles = dir -> nFiles;
	for(; *ind<numFiles; (*ind)++)
	{
		if(strcmp((dir->files[*ind]).fname, fname) == 0 && strcmp((dir->files[*ind]).fext, fext) == 0)
            		return (dir->files[*ind]).nStartBlock;
	}
	return 0;
}

//Gets the index of the file we want. Returns -1 if not found.
static int get_file_index(cs1550_directory_entry * subDir, char * fname, char * fext)
{
        int index = 0;
        while(index < subDir->nFiles)
        {
                //Check filename
                if(strcmp((subDir->files[index]).fname, fname) == 0)
                {
                        //Check extension
                        if(strcmp((subDir->files[index]).fext, fext) == 0)
                        {
                                return index;
                        }
                }

                index++;
        }

        return -1;
}
static long get_file_offset(cs1550_directory_entry * subDir, char * fname, char * fext)
{
        int index = 0;
        while(index < subDir->nFiles)
        {
                //Check filename
                if(strcmp((subDir->files[index]).fname, fname) == 0)
                {
                        //Check extension
                        if(strcmp((subDir->files[index]).fext, fext) == 0)
                        {
                                return (subDir->files[index]).nStartBlock;
                        }
                }

                index++;
        }

        return -1;
}

static int disk_add_dir(FILE* f, char* dname)
{
	int res = -1;
    if(fseek(f, -5*BLOCK_SIZE, SEEK_END) == 0)
    {
        struct cs1550_bitmap _bitmap;
        if(fread(&_bitmap, 1, 5*BLOCK_SIZE, f) == (BITMAP_LAST+1))
        {
            long _nStartBlock = find_free_block(f, _bitmap.bitmap);
            if(_nStartBlock)  /** 0 is returned if can't find one. **/
            {
                printf("bitmap returned. The starting block is %ld\n", _nStartBlock);
                if(fseek(f, 0, SEEK_SET) == 0)
                {
                    cs1550_root_directory root;
                    if(fread(&root, 1, BLOCK_SIZE, f) == BLOCK_SIZE)
                    {
                        stpcpy(root.directories[root.nDirectories].dname, dname);
                        root.directories[root.nDirectories].nStartBlock = _nStartBlock;
                        root.nDirectories += 1;
                        printf("now %d directories on disk.\n", root.nDirectories);
                        if(fseek(f, 0, SEEK_SET) == 0)
                            if(fwrite(&root, 1, BLOCK_SIZE, f) == BLOCK_SIZE)
                            {
                                printf("root updated.\n");
                                cs1550_directory_entry new_dir;
                                if(fseek(f, _nStartBlock*BLOCK_SIZE, SEEK_SET) == 0)
                                    if(fwrite(&new_dir, 1, BLOCK_SIZE, f) == BLOCK_SIZE)
                                    {
                                        printf("new dir written.\n");
                                        res = 0;
                                    }
                            }
                    }
                }
            }
        }
    }
    return res;
} 


/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not. 
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int cs1550_getattr(const char *path, struct stat *stbuf)
{
	int res = -ENOENT;

	memset(stbuf, 0, sizeof(struct stat));
   
	//is path the root dir?
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else {
		char dname[MAX_FILENAME + 1];
		char fname[MAX_FILENAME + 1];
		char fext[MAX_FILENAME + 1];
		parsing(path, dname, fname, fext);
		//Check if name is subdirectory
		if(dname[0] && (dname[MAX_FILENAME] == '\0') && (fname[MAX_FILENAME] == '\0'))
		{
			FILE* f = fopen(".disk", "rb");
			if(f)
			{
				long dirPosition = disk_offset_dir(f, dname);
				if(dirPosition)
				{
					if(fname[0] == '\0') //only directory name specified
					{
						stbuf->st_mode = S_IFDIR | 0755;
						stbuf->st_nlink = 2;
						res = 0; //no error
					}
					else
					{
						if(fseek(f, BLOCK_SIZE*dirPosition, SEEK_SET) == 0)
						{
							cs1550_directory_entry dir;
							if(fread(&dir, 1, BLOCK_SIZE, f) == BLOCK_SIZE)
							{
								int ind = 0;
								long extPosition = disk_offset_ext(&dir, fname, fext, &ind);
								if(extPosition)
								{
									stbuf->st_mode = S_IFREG | 0666; 
									stbuf->st_nlink = 1; //file links
									stbuf->st_size = 0; //file size - make sure you replace with real size!
									res = 0; // no error
								}
							}
						}
						
					}
				}
				else printf("directory not found");
				fclose(f);
			}
		}
	}
	return res;
}

/* 
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 */
static int cs1550_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void) offset;
	(void) fi;
	int res = -ENOENT;
	
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	
	//This line assumes we have no subdirectories, need to change
	if (strcmp(path, "/") != 0)
	{
		char dname[MAX_FILENAME + 1];
		char fname[MAX_FILENAME + 1];
		char fext[MAX_FILENAME + 1];
		parsing(path, dname, fname, fext);
		
		if(dname[0] && (dname[MAX_FILENAME] == '\0') && (fname[MAX_FILENAME] == '\0'))
		{
			FILE* f = fopen(".disk", "rb");
			if(f)
			{
				long dirPosition = disk_offset_dir(f, dname);
				if(dirPosition)
				{
					if(fseek(f, BLOCK_SIZE*dirPosition, SEEK_SET) == 0)
					{
						cs1550_directory_entry dir;
						if(fread(&dir, 1, BLOCK_SIZE, f) == BLOCK_SIZE)
						{
							int numFiles = dir.nFiles;
							int x = 0;
							for(x=0; x<numFiles; x++)
							{
								char fileExt[MAX_FILENAME + MAX_EXTENSION +2];
								strcpy(fileExt, dir.files[x].fname);
								if(dir.files[x].fext[0])
								{
									strcat(fileExt, ".");
									strcat(fileExt, dir.files[x].fext);
								}
								filler(buf, fileExt, NULL, 0);
							}
							res = 0;
						}
					}
				}
				fclose(f);
			}
		}
	}
	else{
		cs1550_root_directory root;
		FILE* f = fopen(".disk", "rb");
		if(f && (fread(&root, 1, BLOCK_SIZE, f) == BLOCK_SIZE))
		{
			int numDirs = root.nDirectories;
			int x = 0;
			for(x=0; x<numDirs; x++)
				filler(buf, root.directories[x].dname, NULL, 0);
			res = 0;
		}
		fclose(f);
	}
	return res;
}

/* 
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int cs1550_mkdir(const char *path, mode_t mode)
{
	(void) path;
	(void) mode;
	int res = 0;
	char dname[MAX_FILENAME + 1];
	char fname[MAX_FILENAME + 1];
	char fext[MAX_FILENAME + 1];
	parsing(path, dname, fname, fext);
	
	if(dname[MAX_FILENAME])
	{
		res = -ENAMETOOLONG;
	}
	else if(fname[0])
	{
		res = -EPERM;
	}
	else if (dname[0] && (dname[MAX_FILENAME] == '\0') && (fname[0] == '\0'))
	{
		FILE* f = fopen(".disk", "rb+");
		if(f)
		{
			if(disk_offset_dir(f, dname))
			{
				res = -EEXIST;
			}
			else
			{
				res = disk_add_dir(f, dname);
			}
		}
	}
	return 0;
}

/* 
 * Removes a directory.
 */
static int cs1550_rmdir(const char *path)
{
	(void) path;
    return 0;
}

/* 
 * Does the actual creation of a file. Mode and dev can be ignored.
 *
 */
static int cs1550_mknod(const char *path, mode_t mode, dev_t dev)
{
    (void) mode;
    (void) dev;
    (void) path;

    //If this isn't the root
    if(strcmp(path, "/") != 0)
    {
        //Parse the path
        char dname[MAX_FILENAME + 1];   
        char fname[MAX_FILENAME + 1];   
        char fext[MAX_EXTENSION + 1]; 
        memset(dname, 0, MAX_FILENAME + 1);
        memset(fname, 0, MAX_FILENAME + 1);
        memset(fext, 0, MAX_EXTENSION + 1);
        sscanf(path, "/%[^/]/%[^.].%s", dname, fname, fext);
        
        //if the directory, filename, and file extension data are valid we open .disk and look at contents
        if(dname[0] && dname[MAX_FILENAME] == '\0' && fname[MAX_FILENAME] == '\0' && fext[MAX_EXTENSION] == '\0')
        {
            FILE* fd = fopen(".disk", "rb++");
            if(fd)
            {
                long subdirposition = disk_offset_dir(fd, dname);

                //See if the file already exists
                if(subdirposition)
                {
                    //Go to the desired directory
                    if(fseek(fd, BLOCK_SIZE * subdirposition, SEEK_SET) == 0){
                        cs1550_directory_entry Directory;
                        if(fread(&Directory, 1, BLOCK_SIZE, fd) == BLOCK_SIZE){
                            int index = 0;
                            //Check if file already exists
                            if(disk_offset_ext(&Directory, fname, fext, &index) != 0){
                             
								fclose(fd);
                                return -EEXIST;
                            }


                            //Go to our bitmap
                            fseek(fd, -(5*BLOCK_SIZE), SEEK_END);

               

                            //Create our bitmap from .disk
                            struct cs1550_bitmap myBitmap;
                            if(fread(&myBitmap, 1, BITMAP_LENGTH, fd) == (BITMAP_LENGTH))
                            {
                                printf("Successfully read bitmap; finding place for inode...\n");
                                //try to find a free block in the bitmap
                                long freeblock = find_free_block(fd, myBitmap.bitmap);
                                if(freeblock)
                                {
                                    //Add the file data to the proper location
                                    stpcpy(Directory.files[Directory.nFiles].fname, fname);
                                    stpcpy(Directory.files[Directory.nFiles].fext, fext);
                                    Directory.files[Directory.nFiles].fsize = 0;
                                    Directory.files[Directory.nFiles].nStartBlock = freeblock;

                                    Directory.nFiles += 1;

                                    fseek(fd, subdirposition * BLOCK_SIZE, SEEK_SET);
                                    fwrite(&Directory, 1, BLOCK_SIZE, fd);

                                    printf("%s has %d files in mknod\n", dname, Directory.nFiles);

                                    //Try to add the inode to where the directory points to
                                    if(fseek(fd, freeblock * BLOCK_SIZE, SEEK_SET) == 0)
                                    {
                                        cs1550_inode myinode;
                                        myinode.magic_number = 0xF113DA7A;
                                        myinode.children = 0;
                                        if(fwrite(&myinode, 1, BLOCK_SIZE, fd) == BLOCK_SIZE){
                                            printf("%s %s was created at %li", fname, fext, freeblock);
                                        }
                                    }
									fclose(fd);
                                }
                            }
                        }
                    }
                }
            }

        }
        else
        {
			
            return -ENAMETOOLONG;
        }
    }
    else{
		
        return -EPERM;
    }
	
    return 0;
}

//Free block in our bitmap
static long freeBlock(FILE *fd, unsigned char* bitmap, long locationToFree){
    long shiftAmount = locationToFree % 8;
    long index = locationToFree / 8;

    //We have to use bitshifts to clear the correct bit
    bitmap[index] &= ~(1 << shiftAmount);

    //Write to bitmap
    fseek(fd, -(BITMAP_LENGTH), SEEK_END);
    if(fwrite(bitmap, 1, BITMAP_LENGTH, fd) == BITMAP_LENGTH)
    return 0;
}

/*
 * Deletes a file
 */
static int cs1550_unlink(const char *path)
{
    (void) path;

    //root
    if(strcmp(path, "/") != 0)
    {
        char dname[MAX_FILENAME + 1];   
        char fname[MAX_FILENAME + 1];   
        char fext[MAX_EXTENSION + 1]; 
        memset(dname, 0, MAX_FILENAME + 1);
        memset(fname, 0, MAX_FILENAME + 1);
        memset(fext, 0, MAX_EXTENSION + 1);
        sscanf(path, "/%[^/]/%[^.].%s", dname, fname, fext);

        FILE *fd = fopen(".disk", "rb+");
        if(fd){
            long directoryPosition = disk_offset_dir(fd, dname);

            if(directoryPosition){
                cs1550_directory_entry myDirectory;
                fseek(fd, directoryPosition * BLOCK_SIZE, SEEK_SET);
                fread(&myDirectory, 1, BLOCK_SIZE, fd);

                int index = 0;

                //Find file position
                long filePosition = disk_offset_ext(&myDirectory, fname, fext, &index);
                if(filePosition)
                {
                    //We'll need our bitmap
                    struct cs1550_bitmap myBitmap;
                    fseek(fd, -(BITMAP_LENGTH), SEEK_END);
                    fread(&myBitmap, 1, BITMAP_LENGTH, fd);

                    //Read in our inode
                    cs1550_inode myinode;
                    fseek(fd, filePosition * BLOCK_SIZE, SEEK_SET);
                    fread(&myinode, 1, BLOCK_SIZE, fd);

                    //Need to free all of the data blocks used within inode
                    int j;
                    for(j = 0; j < myinode.children; j++){
                        long blockLocation = myinode.pointers[j];
                        freeBlock(fd, myBitmap.bitmap, blockLocation);
                    }

                    //Need to update our directory
                    //If in center or left end of array need to shift other files over
                    if(index != myDirectory.nFiles - 1){
                        int j;
                        for(j = index; j < myDirectory.nFiles - 1; j++){
                            myDirectory.files[j] = myDirectory.files[j + 1];
                        }
                    }

                    myDirectory.nFiles -= 1;
                    fseek(fd, directoryPosition * BLOCK_SIZE, SEEK_SET);
                    fwrite(&myDirectory, 1, BLOCK_SIZE, fd);

                    //free the inode
                    freeBlock(fd, myBitmap.bitmap, filePosition);
                }
                else{
                    fclose(fd);
                    return -EISDIR;
                }
            }
        }

        fclose(fd);
    }
    else{
        return -EISDIR;
    }

    return 0;
}

/* 
 * Read size bytes from file into buf starting from offset
 *
 */
static int cs1550_read(const char *path, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	
    (void) path;

	size = 0;
	
    //If this isn't the root
    if(strcmp(path, "/") != 0)
    {
        //Parse the path
        char dname[MAX_FILENAME + 1];   
        char fname[MAX_FILENAME + 1];   
        char fext[MAX_EXTENSION + 1]; 
        memset(dname, 0, MAX_FILENAME + 1);
        memset(fname, 0, MAX_FILENAME + 1);
        memset(fext, 0, MAX_EXTENSION + 1);
        sscanf(path, "/%[^/]/%[^.].%s", dname, fname, fext);
        
        //if the directory, filename, and file extension data are valid we open .disk and look at contents
        if(dname[0] && dname[MAX_FILENAME] == '\0' && fname[MAX_FILENAME] == '\0' && fext[MAX_EXTENSION] == '\0')
        {
            FILE* fd = fopen(".disk", "rb++");
            if(fd)
            {
                long subdirposition = disk_offset_dir(fd, dname);

                //See if the file already exists
                if(subdirposition)
                {
                    //Go to the desired directory
                    if(fseek(fd, BLOCK_SIZE * subdirposition, SEEK_SET) == 0){
                        cs1550_directory_entry Directory;
                        if(fread(&Directory, 1, BLOCK_SIZE, fd) == BLOCK_SIZE){
                            int index = 0;
                            //Check if file already exists
                            if(disk_offset_ext(&Directory, fname, fext, &index) != 0){
                             
								fclose(fd);
                                return -EEXIST;
                            }


                            //Go to our bitmap
                            fseek(fd, -(5*BLOCK_SIZE), SEEK_END);

               

                            //Create our bitmap from .disk
                            struct cs1550_bitmap myBitmap;
                            if(fread(&myBitmap, 1, BITMAP_LENGTH, fd) == (BITMAP_LENGTH))
                            {
                                printf("Successfully read bitmap; finding place for inode...\n");
                                //try to find a free block in the bitmap
                                long freeblock = find_free_block(fd, myBitmap.bitmap);
                                if(freeblock)
                                {
                                    //Add the file data to the proper location
                                    stpcpy(Directory.files[Directory.nFiles].fname, fname);
                                    stpcpy(Directory.files[Directory.nFiles].fext, fext);
                                    Directory.files[Directory.nFiles].fsize = 0;
                                    Directory.files[Directory.nFiles].nStartBlock = freeblock;

                                    Directory.nFiles += 1;

                                    fseek(fd, subdirposition * BLOCK_SIZE, SEEK_SET);
                                    fwrite(&Directory, 1, BLOCK_SIZE, fd);

                                    printf("%s has %d files in mknod\n", dname, Directory.nFiles);

                                    //Try to add the inode to where the directory points to
                                    if(fseek(fd, freeblock * BLOCK_SIZE, SEEK_SET) == 0)
                                    {
                                        cs1550_inode myinode;
                                        myinode.magic_number = 0xF113DA7A;
                                        myinode.children = 0;
                                        if(fwrite(&myinode, 1, BLOCK_SIZE, fd) == BLOCK_SIZE){
                                            printf("%s %s was created at %li", fname, fext, freeblock);
                                        }
										size = BLOCK_SIZE;
                                    }
									fclose(fd);
                                }
                            }
                        }
                    }
                }
            }

        }
        else
        {
			
            return -ENAMETOOLONG;
        }
    }
    else{
		
        return -EPERM;
    }
	
	return size;
}

/* 
 * Write size bytes from buf into file starting from offset
 *
 */
static int cs1550_write(const char *path, const char *buf, size_t size, 
			  off_t offset, struct fuse_file_info *fi)
{
	//added printfs for test
		//(void) buf;
        //(void) offset;
        (void) fi;
        //(void) path;

        //check to make sure path exists
        //check that size is > 0
        //check that offset is <= to the file size
        //write data
        //set size (should be same as input) and return, or error
		
		//char arrays for storing directory name, filename, and extension
        char dir[MAX_FILENAME + 1];
		char fname[MAX_FILENAME + 1];
		char ext[MAX_EXTENSION + 1];
        //set all values in them to 0
        memset(dir, 0, MAX_FILENAME + 1);
        memset(fname, 0, MAX_FILENAME + 1);
        memset(ext, 0, MAX_EXTENSION + 1);

        sscanf(path, "/%[^/]/%[^.].%s", dir, fname, ext);

        FILE * f = fopen(".disk", "rb+");
		
        if(!f)
        {
                printf("File cannot be opened.\n");

        } else {
		
			//printf("In subdirectory, showing all files: \n");
            long dirIndex = disk_offset_dir(f, dir);
            if(dirIndex == -1)
			{
				printf("\nDirectory not found when trying to write.\npath: %s\n", path);
				
			} else {
				printf("Directory found when trying to write\n");
				if(fseek(f, BLOCK_SIZE*dirIndex, SEEK_SET) == 0)
				{
					cs1550_directory_entry subDir;

					if(fread(&subDir, 1, BLOCK_SIZE, f) == BLOCK_SIZE)
					{
						//Check if file exists
						long fOffset = get_file_offset(&subDir, fname, ext);
						int index = get_file_index(&subDir, fname, ext);
						if(fOffset == -1)
						{
																			
							if(subDir.files[index].fsize < offset){
								printf("Offset > size!\n");
								fclose(f);
								return -EFBIG;
							} else if (size <= 0){
								printf("Size is not a positive number\n");
							} else {
								subDir.files[index].fsize = size;
								
								struct cs1550_bitmap bMap;
								fseek(f, -(BITMAP_LENGTH), SEEK_END);
									
								if(fread(&bMap, 1, BITMAP_LENGTH, f) == BITMAP_LENGTH){
									int numBlocks = size/BLOCK_SIZE + 1;
									long freeBlockIndex;
									cs1550_inode new_inode;
									new_inode.magic_number = 0xFFFFFFFF;
									new_inode.children = numBlocks;	
									
									int bufOffset = 0;
									int i, j;
									for(i = 0; i < numBlocks; i++){
										//Get block address using bMap
										freeBlockIndex = find_free_block(f, bMap.bitmap);
										if(freeBlockIndex == 0)
										{
											printf("No free blocks found!\n");
										} else {
											printf("Free block found\n");
											struct cs1550_disk_block temp;
											temp.magic_number = 0xF113DA7A;
											//copy data over to temp.data[];
											for(j = offset; j < BLOCK_SIZE; j++){
												temp.data[j] = buf[bufOffset];
												offset = 0;
												bufOffset++;
											}
											
											fseek(f, freeBlockIndex * BLOCK_SIZE, SEEK_SET);
											if (fwrite(&temp, 1, BLOCK_SIZE, f) == BLOCK_SIZE){
												printf("New data block filled");
											}
											new_inode.pointers[i] = freeBlockIndex;
										}
									}
									
									fseek(f, BLOCK_SIZE * subDir.files[index].nStartBlock, SEEK_SET);
									if(fwrite(&new_inode, 1, BLOCK_SIZE, f) == BLOCK_SIZE){
										printf("Data has successfully been written.\n");
									}
								}								
							}
						} else {
							printf("File does not exist!\n");
						}
					}
				}
			} 
			
			fclose(f);
        }

        return size;
}

/******************************************************************************
 *
 *  DO NOT MODIFY ANYTHING BELOW THIS LINE
 *
 *****************************************************************************/

/*
 * truncate is called when a new file is created (with a 0 size) or when an
 * existing file is made shorter. We're not handling deleting files or 
 * truncating existing ones, so all we need to do here is to initialize
 * the appropriate directory entry.
 *
 */
static int cs1550_truncate(const char *path, off_t size)
{
	(void) path;
	(void) size;

    return 0;
}


/* 
 * Called when we open a file
 *
 */
static int cs1550_open(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;
    /*
        //if we can't find the desired file, return an error
        return -ENOENT;
    */

    //It's not really necessary for this project to anything in open

    /* We're not going to worry about permissions for this project, but 
	   if we were and we don't have them to the file we should return an error

        return -EACCES;
    */

    return 0; //success!
}

/*
 * Called when close is called on a file descriptor, but because it might
 * have been dup'ed, this isn't a guarantee we won't ever need the file 
 * again. For us, return success simply to avoid the unimplemented error
 * in the debug log.
 */
static int cs1550_flush (const char *path , struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;

	return 0; //success!
}


//register our new functions as the implementations of the syscalls
static struct fuse_operations hello_oper = {
    .getattr	= cs1550_getattr,
    .readdir	= cs1550_readdir,
    .mkdir	= cs1550_mkdir,
	.rmdir = cs1550_rmdir,
    .read	= cs1550_read,
    .write	= cs1550_write,
	.mknod	= cs1550_mknod,
	.unlink = cs1550_unlink,
	.truncate = cs1550_truncate,
	.flush = cs1550_flush,
	.open	= cs1550_open,
};

//Don't change this.
int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &hello_oper, NULL);
}
