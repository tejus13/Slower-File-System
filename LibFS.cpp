#include "LibFS.h"
#include "LibDisk.h"
// global errno value here
#include <bits/stdc++.h>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
using namespace std;
// #define ll long long int
// #define db_size (1<<15)

// #define NO_OF_FILEDESCRIPTORS 32  //No of File descriptors can be initialized here
// #define FS fileDescriptor_map[fildes].second

int read_block(int block,char* buf);


int write_block(int block, char *buf, int size_to_wrt, int start_pos_to_wrt);

//data structure

struct inode
{
    int inode_num;
    int type;
    int filesize;
    int pointer[30]; //contains DB nos of files (-1 if no files assigned)
};
struct inode inode_arr[NO_OF_INODES];


struct dir_entry
{
    char file_name[16]; //filename size is 15 max
    int inode_num;  // of this 
};
struct dir_entry dir_struct_entry[NO_OF_INODES];


struct super_block  // map of directory  structure
{
    
            /* directory information */
    int firstDbOfDir = ceil( ((float)sizeof(super_block))/BLOCK_SIZE );    //33; //set in main or create_disk = ceil( (float)sizeof(super_block)/block_size )
    int noOfDbUsedinDir = (sizeof(struct dir_entry)*NO_OF_INODES)/BLOCK_SIZE;
            /* data information */
    int startingInodesDb = firstDbOfDir+noOfDbUsedinDir; //130; //coz 0to32=sup_block 33to129=DS 130to(130+255)==130to385 inode data blocks
    int total_Usable_DB_Used_for_inodes= ceil( ((float)(NO_OF_INODES * sizeof(struct inode))) / BLOCK_SIZE );  //256; //coz tot no of inodes are 16k=16*1024 which is contained inside 256 DB
    int starting_DB_Index= startingInodesDb + total_Usable_DB_Used_for_inodes;  //386; //coz 130to385 is inode blocks
    int total_Usable_DB= DISK_BLOCKS - starting_DB_Index;  //130686; //coz 128k=128*1024 is total no of db in virtual disk out of this 386 assigned to meta data so 128k-386 = 130686 db for data
    char inode_Bitmap[NO_OF_INODES];  //to check which inode no is free to assign to file
  //  int next_freeinode=0;//index into inode_Bitmap[16384];  //index of inode_arr
    char datablock_Bitmap[DISK_BLOCKS];  //to check which DB is free to allocate to file
  //  unsigned int next_freedatablock=starting_DB_Index;  //the DB no which is free
    //add int free_avail_db
};
struct super_block sup_block;


char disk_name[100];
char filename[16];

map <string,int> directory_map;

vector<int> free_Db_list;

vector<int> free_Inode_list;

map <string,int> :: iterator it;

vector<int> free_FD_list;   //Keeps track of Free file descriptors.
int openfile_count=0 ; //keeps track of number of files opened.
map <int,pair<int,int>> fileDescriptor_map; //Stores files Descriptor as key and corresponding Inode number(First) and file pointer(second) as value

//main functionality starts
int osErrno;

FILE *fp;
//mounting function
int mounting()
{
    fp = fopen(disk_name,"rb+");
    //retrieve super block from virtual disk and store into global struct super_block sup_block 
    char sup_block_buff[sizeof(sup_block)];
    memset(sup_block_buff,0,sizeof(sup_block));
    fread(sup_block_buff,1,sizeof(sup_block),fp);
    memcpy(&sup_block,sup_block_buff,sizeof(sup_block));


    //retrieve DS block from virtual disk and store into global struct dir_entry dir_struct_entry[NO_OF_INODES]
    fseek( fp, (sup_block.firstDbOfDir)*BLOCK_SIZE, SEEK_SET );
    char dir_buff[sizeof(dir_struct_entry)];
    memset(dir_buff,0,sizeof(dir_struct_entry));
    fread(dir_buff,1,sizeof(dir_struct_entry),fp);
    memcpy(dir_struct_entry,dir_buff,sizeof(dir_struct_entry));



    //retrieve Inode block from virtual disk and store into global struct inode inode_arr[NO_OF_INODES];
    fseek( fp, (sup_block.startingInodesDb)*BLOCK_SIZE, SEEK_SET );
    char inode_buff[sizeof(inode_arr)];
    memset(inode_buff,0,sizeof(inode_arr));
    fread(inode_buff,1,sizeof(inode_arr),fp);
    memcpy(inode_arr,inode_buff,sizeof(inode_arr));


    //storing all filenames into map
    for (int i = NO_OF_INODES - 1; i >= 0; --i)
        if(sup_block.inode_Bitmap[i]==1)
            directory_map[string(dir_struct_entry[i].file_name)]=dir_struct_entry[i].inode_num;
        else
            free_Inode_list.push_back(i);

    // populate free_Inode_list and free_Db_list
    for(int i = DISK_BLOCKS - 1; i>=sup_block.starting_DB_Index; --i)
        if(sup_block.datablock_Bitmap[i] == 0)
            free_Db_list.push_back(i);

    // Populate Free Filedescriptor vector
    for(int i=NO_OF_FILEDESCRIPTORS-1;i>=0;i--){
        free_FD_list.push_back(i);
    }

    printf("Disk is mounted now\n");


    /*printf(" Starting DB NO Directory Str%d\n", sup_block.firstDbOfDir);
    printf(" Starting DB NO INODES%d\n", sup_block.startingInodesDb);
    printf("NO Data blocks for directory structure %d\n",sup_block.noOfDbUsedinDir);
    printf(" Data Blocks Starting index%d\n",sup_block.starting_DB_Index );
    printf(" Total No of data blocks%d\n",sup_block.total_Usable_DB );*/
    return 1;
}


void disk_Set()
{
    char buffer[BLOCK_SIZE];

    fp = fopen(disk_name,"wb");
    memset(buffer, 0, BLOCK_SIZE); //setting the buffer's value=NULL at all index

    for (int i = 0; i < DISK_BLOCKS; ++i)
        fwrite(buffer,1,BLOCK_SIZE,fp);
        //write(fp, buffer, BLOCK_SIZE);
    
    
    struct super_block sup_block;  //initializing sup_block

    //setting DB vecor array to 1(=used i.e cant assign to file) for all metadata DB and 0(=free) to all data DBs
    for (int i = 0; i < sup_block.starting_DB_Index; ++i)
        sup_block.datablock_Bitmap[i] = 1; //1 means DB is not free
    for (int i = sup_block.starting_DB_Index; i < DISK_BLOCKS; ++i)
        sup_block.datablock_Bitmap[i] = 0; //1 means DB is not free
    for(int i = 0; i < NO_OF_INODES; ++i)
        sup_block.inode_Bitmap[i]=0; // char 0;

    for (int i = 0; i < NO_OF_INODES; ++i)
    {   // we have 30 data blocks pointer
        for(int j = 0; j < 30; j++ )
        {
            inode_arr[i].pointer[j]=-1;
        }
        //inode_arr[i].pointer[13]=-1;
    }


    //storing sup_block into begining of virtual disk
    fseek( fp, 0, SEEK_SET );
    char sup_block_buff[sizeof(struct super_block)];
    memset(sup_block_buff, 0, sizeof(struct super_block));
    memcpy(sup_block_buff,&sup_block,sizeof(sup_block));
    fwrite(sup_block_buff,1,sizeof(sup_block),fp);



    //storing DS after sup_block into virtual disk
    fseek( fp, (sup_block.firstDbOfDir)*BLOCK_SIZE, SEEK_SET );
    char dir_buff[sizeof(dir_struct_entry)];
    memset(dir_buff, 0, sizeof(dir_struct_entry));
    memcpy(dir_buff,dir_struct_entry,sizeof(dir_struct_entry));
    fwrite(dir_buff,1,sizeof(dir_struct_entry),fp);



    //storing inode DBs after sup_block & DB into virtual disk
    fseek( fp, (sup_block.startingInodesDb)*BLOCK_SIZE, SEEK_SET );
    char inode_buff[sizeof(inode_arr)];
    memset(inode_buff, 0, sizeof(inode_arr));
    memcpy(inode_buff,inode_arr,sizeof(inode_arr));
    fwrite(inode_buff,1,sizeof(inode_arr),fp);



    fclose(fp);
    printf("Virtual Disk %s created sucessfully\n",disk_name );
    
}


int 
FS_Boot(char *path)
{   
    //disk_name = path;
    strcpy(disk_name, path);
    cout<<"disk name:" << disk_name<<endl;

    printf("FS_Boot %s\n", disk_name);

    if( access( disk_name, F_OK ) != -1 )
    {
        // file exists
        printf("The Virtual Disk already exists\n");
       // mounting();
    } 
    else 
    {
        //  create_disk();      
        // mounting();
    
        // oops, check for errors
        if (Disk_Init() == -1) {
    	printf("Disk_Init() failed\n");
    	osErrno = E_GENERAL;
    	return -1;
        }

	   //modified

	   Disk_Save(disk_name);
       disk_Set();
        // do all of the other stuff needed...
    }

    mounting();
    return 0;
}

int FS_Sync()
{
    printf("FS_Sync\n");
    return 0;
}


int
File_Create(char *filename)
{
    printf("FS_Create\n");

    int nextin, nextdb;
    //check if filename already exists in directory (using map directory_map)
    if(directory_map.find(filename)!= directory_map.end())
    {
        printf("Create File Error : File already exist.\n");
        return -1;
    }
    //check for free inode(if present i.e sup_block.next_freeinode != -1 )=in_free & set it 0 (0 means now this inode no is nt free)
    if(free_Inode_list.size()==0) //sup_block.next_freeinode == -1
    {
            printf("Create File Error : No more files can be created.Max number of files reached.\n");
            return -1;
    }   

    //set pointer of sup_block.next_freeinode to point nextfree inode if no free inode available then set sup_block.next_freeinode=-1
    //check for free db(if present i.e != -1)
    if(free_Db_list.size() == 0) //sup_block.next_freedatablock == -1
    {
        printf("Create File Error : Not enough space.\n");
        return -1;
    }
    //assign a 4kb db & set 
    nextin = free_Inode_list.back();
    free_Inode_list.pop_back();
    nextdb = free_Db_list.back();
    free_Db_list.pop_back();
    inode_arr[nextin].pointer[0]=nextdb;
    
    //set inode_arr[in_free].filesize=0;
    inode_arr[nextin].filesize = 0; // or 4KB whatever. 
    //set value of map directory_map[string(filename)] = in_free;
    directory_map[string(filename)] = nextin;
    //set directory structure also i.e strcpy(dir_struct_entry[in_free].file_name, filename) & dir_struct_entry[in_free].inode_num = in_free
    strcpy(dir_struct_entry[nextin].file_name,filename);
    dir_struct_entry[nextin].inode_num = nextin;
    //set current nextfreeinode and nextfreedb indices and choose nextfreeinode and nextfreedb indices

    //  no need to do this we'll use only freevectors now onwards 
    //      sup_block.inode_Bitmap[nextin]=1; // now this inode is reserved.
    //      sup_block.datablock_Bitmap[nextdb]= 1; // now this db is reserved.

    /*  for( i=0; i<NO_OF_INODES;i++)
    {
    if(sup_block.inode_Bitmap[i]==0)
        break;
    }
    if(i != NO_OF_INODES)
        next_freeinode = i;
    else
        next_freeinode = -1;

    for(i=sup_block.starting_DB_Index ; i < DISK_BLOCKS ; i++)
    {
    if(sup_block.datablock_Bitmap[i] == 0)
    break;
    }
    if(i != DISK_BLOCKS)
    next_freedatablock = i;
    else
    next_freedatablock = -1;
    */
    printf(" %s file is created successfully.\n",filename );
    return 0;
}

int
File_Open(char *name)
{
    printf("FS_Open\n");

    //form a gloabl var openfile_count=0 & map with file descriptor as key and a pair of Inode and current cursor position as value 
    //(coz  fd is key and unique fd can be assigned to one inodeNo)
    //each time on opening a file do openfile_count++ and get a free file descriptor from free_FD_list and make it as key and assign it to Inode
    // and on closing do openfile_count-- 
    //and remove the entry from map and push file descriptor in free_FD_list limit the count_of_openFile to 32 so that on exceeding openfile_count = 31 throw an error.
    // Can allocate the file descriptor until free_FD_list becomes empty.
    //allocate the file descriptor and set cursor position to 0 return the allocated file descriptor to the called function.  

    int inode_no;
    if ( directory_map.find(name) == directory_map.end() ) 
    {
        printf("Open File Error : File Not Found\n");
        return -1;  
    }
    else 
    {
        //int file_des = getFreeFileDescriptor();
        if(free_FD_list.size() == 0)
        {
            printf("Open File Error : No file descriptors available\n");
            return -1;
        }
        else
        {
            int inode = directory_map[name];
            int fd = free_FD_list.back();
            free_FD_list.pop_back();
            fileDescriptor_map[fd].first=inode;
            fileDescriptor_map[fd].second = 0;
            openfile_count++;
            //char inode_buf[sizeof(ino)]
            /*for(int i=0;i<inode_arr[inode].filesize/BLOCK_SIZE;i++){

            }*/
            printf("Given %d as File descriptor\n",fd);

            //For Debugging Purpose Will remove later
            /*printf("%d\n",inode_arr[inode].filesize );
            printf("%d\n",inode_arr[inode].inode_num );
            for (int i = 0; i < 13; ++i)
            {
                /* code 
                printf("%d\n",inode_arr[inode].pointer[i] );
            }*/

            return fd;

        }
    }
    return 0;
}

int
File_Read(int fildes, void  *buf, int nbyte)
{
    printf("FS_Read\n");


    char dest_filename[16];
    int flag=0;
    if(fileDescriptor_map.find(fildes) == fileDescriptor_map.end()){
        printf("File Read Error: File not opened \n");
        return -1;
    }
    else
    {
        int inode =fileDescriptor_map[fildes].first;
        int noOfBlocks=ceil(((float)inode_arr[inode].filesize)/BLOCK_SIZE);
        int tot_block=noOfBlocks;
        strcpy(dest_filename, dir_struct_entry[inode].file_name);

        FILE* fp1 = fopen(dest_filename,"wb+");
        char read_buf[BLOCK_SIZE];
        //noOfBlocks--;             //Why?  To leave last block unread using traditional while() because data in last block may be partially filled. 

        for( int i = 0; i < 30; i++ )
        {       //Reading data present at direct pointers
            if(noOfBlocks == 0){
                break;
            }
            int block=inode_arr[inode].pointer[i];
            
            read_block(block,read_buf);
            

            // if(noOfBlocks > 1) 
            //  fwrite(read_buf,1,BLOCK_SIZE,fp1);


            if((tot_block-noOfBlocks >= FS/BLOCK_SIZE) && (noOfBlocks > 1))
            {
                if(flag==0)
                {
                    fwrite(read_buf+(FS%BLOCK_SIZE),1,(BLOCK_SIZE-FS%BLOCK_SIZE),fp1);  
                    flag=1;
                }
                else
                    fwrite(read_buf,1,BLOCK_SIZE,fp1);

            }


            noOfBlocks--;
        }


        if(tot_block-FS/BLOCK_SIZE > 1)
            fwrite(read_buf,1,(inode_arr[inode].filesize)%BLOCK_SIZE,fp1);
        else if(tot_block-FS/BLOCK_SIZE == 1)
            fwrite(read_buf+(FS%BLOCK_SIZE),1,(inode_arr[inode].filesize)%BLOCK_SIZE - FS%BLOCK_SIZE,fp1);


        fclose(fp1);

    }
    return 0;
}

int
File_Write(int fd, char *buf, int size)
{

    
    //take user filename input in global var filename[20] (already declared) and size of file in a local var write_file_size (why local coz it'll get updated in inode at the end of function)
    //check if avail DB size > write_file_size else throw error & return

    int cur_pos=fileDescriptor_map[fd].second, db_to_write;

    if( cur_pos%BLOCK_SIZE == 0 && size > BLOCK_SIZE ) //if cur_pos is at the end(oe begin) of DB i.e new byte will come in at 0th position of DB but size is > BLOCK_SIZE 
    {
        printf("size of data to write is more than 1 DB size\n");
        return -1;
    }
    else if( (BLOCK_SIZE - cur_pos%BLOCK_SIZE) < size ) //if DB partially filled and remaing empty size is less than size to write in the block
    {
        printf("size of data to write is more than remainig empty size of DB\n");
    }

    int total_Usable_DB_used_till_cur_pos = cur_pos / BLOCK_SIZE ;  //total_Usable_DB_used_till_cur_pos stores the no of DB that is fully occupied by existing file

    
        if(inode_arr[fileDescriptor_map[fd].first].filesize == 0) //if filesize = 0 means 1 empty DB is already assigned at pointer[0] by create_file()
        {
            write_block(inode_arr[fileDescriptor_map[fd].first].pointer[0], buf, size, 0 );
            inode_arr[fileDescriptor_map[fd].first].filesize += size; //updating filesize
            fileDescriptor_map[fd].second += size;  //updating the cur pos in the file
        }
        else //if filesize is not 0
        {
            if(cur_pos<inode_arr[fileDescriptor_map[fd].first].filesize) //if last blok is partially filled & cur_pos at begin then fill this blok (no need to create new block)
            {
                if(total_Usable_DB_used_till_cur_pos < 10 )
                {
                    db_to_write  = inode_arr[fileDescriptor_map[fd].first].pointer[total_Usable_DB_used_till_cur_pos];
                    // fseek(fp, (db_to_write * BLOCK_SIZE) + ( cur_pos%BLOCK_SIZE ), SEEK_SET);  //move the cur after last byte
                    // fwrite(buf,1,size,fp); //write the buf into file

                    write_block(db_to_write, buf, size, 0 );

                    if( inode_arr[fileDescriptor_map[fd].first].filesize - cur_pos < size )  //updating the filesiz
                        inode_arr[fileDescriptor_map[fd].first].filesize += size - (inode_arr[fileDescriptor_map[fd].first].filesize - cur_pos);
                    fileDescriptor_map[fd].second += size;  //updating the cur pos in the file
                }
                else if(total_Usable_DB_used_till_cur_pos < 1034 ) //i.e if last DB is at single indirect
                {
                    int block = inode_arr[fileDescriptor_map[fd].first].pointer[10];
                    int blockPointers[1024];        //Contains the array of data block pointers.
                    char read_buf[(1<<15)];
                    read_block(block,read_buf);  //reading the block into read_buf
                    memcpy(blockPointers,read_buf,sizeof(read_buf));
                    


                    db_to_write  = blockPointers[total_Usable_DB_used_till_cur_pos-10];
                    // fseek(fp, (db_to_write * BLOCK_SIZE) + ( cur_pos%BLOCK_SIZE ), SEEK_SET);  //move the cur after last byte
                    // fwrite(buf,1,size,fp); //write the buf into file

                    write_block(db_to_write, buf, size, 0);

                    if( inode_arr[fileDescriptor_map[fd].first].filesize - cur_pos < size )  //updating the filesize
                        inode_arr[fileDescriptor_map[fd].first].filesize += size - (inode_arr[fileDescriptor_map[fd].first].filesize - cur_pos);
                    fileDescriptor_map[fd].second += size;  //updating the cur pos in the file
                }
                else
                {

                    int block = inode_arr[fileDescriptor_map[fd].first].pointer[10];
                    int blockPointers[1024];        //Contains the array of data block pointers.
                    char read_buf[(1<<15)];
                    read_block(block,read_buf);  //reading the block into read_buf
                    memcpy(blockPointers,read_buf,sizeof(read_buf));
                    
                    int block2 = blockPointers[(total_Usable_DB_used_till_cur_pos-1034)/1024];
                    int blockPointers2[1024];       //Contains the array of data block pointers.
                    read_block(block2,read_buf);  //reading the block2 into read_buf
                    memcpy(blockPointers2,read_buf,sizeof(read_buf));



                    db_to_write  = blockPointers2[(total_Usable_DB_used_till_cur_pos-1034)%1024];
                    // fseek(fp, (db_to_write * BLOCK_SIZE) + ( cur_pos%BLOCK_SIZE ), SEEK_SET);  //move the cur after last byte
                    // fwrite(buf,1,size,fp); //write the buf into file
                    write_block(db_to_write, buf, size, 0 );

                    if( inode_arr[fileDescriptor_map[fd].first].filesize - cur_pos < size )  //updating the filesize
                        inode_arr[fileDescriptor_map[fd].first].filesize += size - (inode_arr[fileDescriptor_map[fd].first].filesize - cur_pos);
                    fileDescriptor_map[fd].second += size;  //updating the cur pos in the file

                }
            }
            else //if cur pos == filesze and its end of block i.e muliple of (1<<15)
            {
                if(total_Usable_DB_used_till_cur_pos < 10 )
                {
                    db_to_write  =  free_Db_list.back();
                    free_Db_list.pop_back();
                    inode_arr[fileDescriptor_map[fd].first].pointer[total_Usable_DB_used_till_cur_pos];
                    write_block(db_to_write, buf, size, 0 );

                    inode_arr[fileDescriptor_map[fd].first].filesize += size;
                    fileDescriptor_map[fd].second += size;  //updating the cur pos in the file
                }
                else if(total_Usable_DB_used_till_cur_pos < 1034 ) //i.e if last DB is at last of single indirect DB or at last of pointer[9]
                {
                    if(total_Usable_DB_used_till_cur_pos == 10) //i.e all direct DB pointer 0-9 is full then need to allocate a DB to pointer[10] & store 1024 DB in this DB
                    {
                        int blockPointers[1024];
                        for (int i = 0; i < 1024; ++i)
                            blockPointers[i] = -1;
                        
                        int db_for_single_indirect = free_Db_list.back();  //to store blockPointers[1024] into db_for_single_indirect
                        free_Db_list.pop_back();
                        
                        inode_arr[fileDescriptor_map[fd].first].pointer[10]=db_for_single_indirect;
                        char temp_buf[(1<<15)];
                        memcpy(temp_buf,blockPointers,BLOCK_SIZE);

                        write_block(db_for_single_indirect, temp_buf, BLOCK_SIZE, 0 );

                    }

                    int block = inode_arr[fileDescriptor_map[fd].first].pointer[10];
                    int blockPointers[1024];        //Contains the array of data block pointers.
                    char read_buf[(1<<15)];
                    read_block(block,read_buf);  //reading the single indirect block into read_buf
                    memcpy(blockPointers,read_buf,sizeof(read_buf)); //moving the data into blockPointer

                    db_to_write  =  free_Db_list.back();
                    free_Db_list.pop_back();
                    
                    //store back the blockPointer to disk
                    blockPointers[total_Usable_DB_used_till_cur_pos-10] = db_to_write;
                    char temp_buf[(1<<15)];
                    memcpy(temp_buf,blockPointers,BLOCK_SIZE);
                    write_block(block, temp_buf, BLOCK_SIZE, 0 );

                    //write data into DB
                    write_block(db_to_write, buf, size, 0);

                    inode_arr[fileDescriptor_map[fd].first].filesize += size;
                    fileDescriptor_map[fd].second += size;  //updating the cur pos in the file
                }
                else
                {
                    if(total_Usable_DB_used_till_cur_pos == 1034) //if all direct and single direct is full then assign a DB to double indirect all init to -1
                    {
                        int blockPointers[1024];
                        for (int i = 0; i < 1024; ++i)
                            blockPointers[i] = -1;
                        
                        int db_for_double_indirect = free_Db_list.back();  //to store blockPointers[1024] into db_for_double_indirect
                        free_Db_list.pop_back();
                        
                        inode_arr[fileDescriptor_map[fd].first].pointer[11]=db_for_double_indirect;
                        char temp_buf[(1<<15)];
                        memcpy(temp_buf,blockPointers,BLOCK_SIZE);

                        write_block(db_for_double_indirect, temp_buf, BLOCK_SIZE, 0 );
                    }
                    if( (total_Usable_DB_used_till_cur_pos-1034)%1024==0 ) //i.e if total_Usable_DB_used_till_cur_pos is multiple of 1024 means need new DB to be assigned
                    {
                        int block = inode_arr[fileDescriptor_map[fd].first].pointer[11];
                        int blockPointers[1024];        //Contains the array of data block pointers.
                        char read_buf[(1<<15)];
                        read_block(block,read_buf);  //reading the block into read_buf
                        memcpy(blockPointers,read_buf,sizeof(read_buf));



                        int blockPointers2[1024];
                        for (int i = 0; i < 1024; ++i)
                            blockPointers2[i] = -1;
                        
                        int db_for_double_indirect2 = free_Db_list.back();  //to store blockPointers[1024] into db_for_double_indirect
                        free_Db_list.pop_back();
                        
                        blockPointers[(total_Usable_DB_used_till_cur_pos-1034)/1024]=db_for_double_indirect2;
                        char temp_buf[(1<<15)];
                        memcpy(temp_buf,blockPointers2,BLOCK_SIZE);
                        write_block(db_for_double_indirect2, temp_buf, BLOCK_SIZE, 0 );

                        memcpy(temp_buf,blockPointers,BLOCK_SIZE);
                        write_block(block, temp_buf, BLOCK_SIZE, 0 );

                    }


                    int block = inode_arr[fileDescriptor_map[fd].first].pointer[11];
                    int blockPointers[1024];        //Contains the array of data block pointers.
                    char read_buf[(1<<15)];
                    read_block(block,read_buf);  //reading the block into read_buf
                    memcpy(blockPointers,read_buf,BLOCK_SIZE);

                    //#####################################################


                    int block2 = blockPointers[(total_Usable_DB_used_till_cur_pos-1034)/1024];
                    int blockPointers2[1024];       //Contains the array of data block pointers.
                    char read_buf2[(1<<15)];
                    read_block(block2,read_buf2);  //reading the block2 into read_buf2
                    memcpy(blockPointers2,read_buf2,BLOCK_SIZE);


                    int db_to_write = free_Db_list.back();  //to store blockPointers[1024] into db_for_double_indirect
                    free_Db_list.pop_back();
                    blockPointers2[(total_Usable_DB_used_till_cur_pos-1034)%1024]=db_to_write;
                    write_block(db_to_write, buf, size, 0 );  //writing data into db_to_write DB

                    //now restore blockPointers2 back to the block2
                    char temp_buf[(1<<15)];
                    memcpy(temp_buf,blockPointers2,BLOCK_SIZE);
                    write_block(block2, temp_buf, BLOCK_SIZE, 0 );


                    //updating the filesize
                    inode_arr[fileDescriptor_map[fd].first].filesize += size;
                    fileDescriptor_map[fd].second += size;  //updating the cur pos in the file

                }

            }
        }
    
}

int
File_Seek(int fd, int offset)
{
    printf("FS_Seek\n");
    return 0;
}

int
File_Close(char* name)
{/*
    printf("FS_Close\n");
    if ( directory_map.find(name) == directory_map.end() ) 
    {
        printf("File Not Found\n");
        return -1;  
    }
    else
    {   
      int inode= directory_map.find(name);

    }
*/







    return 0;
}

int
File_Unlink(char *file)
{
    printf("FS_Unlink\n");
    return 0;
}




// directory ops
int
Dir_Create(char *path)
{
    printf("Dir_Create %s\n", path);
    return 0;
}

int
Dir_Size(char *path)
{
    printf("Dir_Size\n");
    return 0;
}

int
Dir_Read(char *path, void *buffer, int size)
{
    printf("Dir_Read\n");
    return 0;
}

int
Dir_Unlink(char *path)
{
    printf("Dir_Unlink\n");
    return 0;
}



//Helper function 
int read_block(int block,char* buf)
{
    if ((block < 0) || (block >= DISK_BLOCKS)) {
    printf("Block read Error : block index out of bounds\n");
    return -1;
  }

  if (fseek(fp, block * BLOCK_SIZE, SEEK_SET) < 0) {
    printf("Block read Error : failed to lseek");
    return -1;
  }
  if (fread(buf, 1, BLOCK_SIZE,fp) < 0) {
    printf("Block read Error : failed to read");
    return -1;
  }
  return 0;
}

//Helper function 
int write_block(int block, char *buf, int size_to_wrt, int start_pos_to_wrt) //in gen write_block(5, buf, (1024*32), 0) i.e write all data(of (1024*32) size) into DB no=5 starting from 0th pos
{                                                                            // 0 < size_to_wrt <= (1024*32)  & 0 <= start_pos_to_wrt <= 4095
    cout<<"block_number:"<<block<<endl;
  if ((block < 0) || (block >= DISK_BLOCKS)) 
  {
    printf("Block write Error : block index out of bounds\n");
    return -1;
  }

  if (fseek(fp, (block * BLOCK_SIZE)+start_pos_to_wrt, SEEK_SET) < 0) 
  {
    printf("Block write Error : failed to lseek");
    return -1;
  }

  if (fwrite(buf, 1, size_to_wrt,fp) < 0) 
  {
    perror("block_write: failed to write");
    return -1;
  }

  return 0;
}


int store_file_into_Disk(char * filename)
{
    int fp2_filesize, fd2, size_buf2, total_Usable_DB_in_src_file, remain_size_OfLast_db;
    char buf2[(1<<15)-1];
    FILE *fp2;
    fp2=fopen(filename,"rb");

    if(fp2 == NULL)
    {
        printf("Source File doen not exists\n");
        return -1;
    }

    fseek(fp2, 0L, SEEK_END); // file pointer at end of file
    fp2_filesize = ftell(fp2);  //getting the position
    //total_Usable_DB_in_src_file = fp2_filesize / BLOCK_SIZE;
    fseek( fp2, 0, SEEK_SET );


    if( File_Create(filename) < 0 )
    {
        printf("Error in File_Create()\n");
        return -1;
    }
    if( (fd2=File_Open(filename)) < 0)
    {
        printf("Error in open_file()\n");
        return -1;
    }

    // if( (free_Db_list.size() * (1<<15)) < fp2_filesize )
    // {
    //     printf("Not Enough space in Disk\n");
    //     return -1;
    // }

    remain_size_OfLast_db=BLOCK_SIZE;
    //remain_size_OfLast_db = BLOCK_SIZE - ( (fileDescriptor_map[fd2].second) % BLOCK_SIZE ); //checking how many in last DB have space remaing to store data
    
    if( remain_size_OfLast_db >= fp2_filesize ) //if remaing empty size is more than src file size then write directly in last DB
    {
        fread(buf2, fp2_filesize, 1, fp2);
        buf2[fp2_filesize] = '\0';
        File_Write(fd2, buf2, fp2_filesize);
    }
    else
    {
        //1st write remain_size_OfLast_db
        fread(buf2, remain_size_OfLast_db, 1, fp2);
        buf2[remain_size_OfLast_db] = '\0';
        File_Write(fd2, buf2, remain_size_OfLast_db);
        //printf("1st : remain_size_OfLast_db = %d written\n",remain_size_OfLast_db );
        //write block by block till last block
        int remaining_block_count = (fp2_filesize - remain_size_OfLast_db) / BLOCK_SIZE;
        while(remaining_block_count--)
        {
            fread(buf2, BLOCK_SIZE, 1, fp2);
            buf2[BLOCK_SIZE] = '\0';
            File_Write(fd2, buf2, BLOCK_SIZE);
           // printf("Written 1 block now remaining_block_count = %d\n",remaining_block_count-1 );
        }
        //write last partial block
        int remaining_size = (fp2_filesize - remain_size_OfLast_db) % BLOCK_SIZE;
        
        fread(buf2, remaining_size, 1, fp2);
        buf2[remaining_size] = '\0';
        File_Write(fd2, buf2, remaining_size);
      //  printf("Remaing size = %d written = %s\n",remaining_size,buf2 );

    }

   // close_file(fd2);
    if ( fileDescriptor_map.find(fd2) == fileDescriptor_map.end() ) 
    {
        printf("File Descripter %d not found\n",fd2);
    }
    else 
    {
        fileDescriptor_map.erase(fd2);
        openfile_count--;
        free_FD_list.push_back(fd2);
        printf("FD %d closed successfully\n",fd2);
    }

    return 0;

}


int unmounting()
{
    //to be done::
    //close all open fd
    //store bit vector for inode and datablock vector into sup_block then 

    //storing sup_block into begining of virtual disk
    fseek( fp, 0, SEEK_SET );
    char sup_block_buff[sizeof(struct super_block)];
    memset(sup_block_buff, 0, sizeof(struct super_block));
    memcpy(sup_block_buff,&sup_block,sizeof(sup_block));
    fwrite(sup_block_buff,1,sizeof(sup_block),fp);



    //storing DS after sup_block into virtual disk
    fseek( fp, (sup_block.firstDbOfDir)*BLOCK_SIZE, SEEK_SET );
    char dir_buff[sizeof(dir_struct_entry)];
    memset(dir_buff, 0, sizeof(dir_struct_entry));
    memcpy(dir_buff,dir_struct_entry,sizeof(dir_struct_entry));
    fwrite(dir_buff,1,sizeof(dir_struct_entry),fp);



    //storing inode DBs after sup_block & DB into virtual disk
    fseek( fp, (sup_block.startingInodesDb)*BLOCK_SIZE, SEEK_SET );
    char inode_buff[sizeof(inode_arr)];
    memset(inode_buff, 0, sizeof(inode_arr));
    memcpy(inode_buff,inode_arr,sizeof(inode_arr));
    fwrite(inode_buff,1,sizeof(inode_arr),fp);

    

    printf("Disk Unmounted\n");
    fclose(fp);
    return 1;
}