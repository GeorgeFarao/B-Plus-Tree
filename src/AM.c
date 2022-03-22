#include "AM.h"
#include "bf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define MAX_OPEN_FILES 20
#define MAXSCANS 20

int AM_errno = AME_OK;

#define CALL_BF(call)       \
{                           \
  BF_ErrorCode code = call; \
  if (code != BF_OK) {         \
    BF_PrintError(code);    \
    exit(code);            \
  }                         \
}

typedef struct snode {     //node for stack
    int blocks_num;         //number of block we insert in the stack
    struct snode *next;     //pointer to the next node
} snode;

typedef struct Stack {
    snode *head;            //pointer to the start of stack
    snode *tail;            //pointer to the end of stack
    int size;               //number of blocks in the stack
} Stack;

Stack *create() {       //initialize the stack
    Stack *tmp = malloc(sizeof(Stack));
    tmp->head = NULL;
    tmp->tail = NULL;
    tmp->size = 0;
    return tmp;
}

struct snode *createnode(int blocks_num) {  //create a new stack node
    struct snode *tmp = malloc(sizeof(struct snode));
    tmp->blocks_num = blocks_num;
    tmp->next = NULL;
}

int isEmpty(Stack *s) {
    return s->size == 0;
}

void push(Stack *s, int blocks_num) {       //insert a node in the stack
    struct snode *tmp = createnode(blocks_num);
    if (isEmpty(s)) {   //if stack is empty both pointers point to the same block
        s->head = tmp;
        s->tail = tmp;
    } else {
        s->tail->next = tmp;
        s->tail = tmp;
    }
    s->size++;
}

int pop(Stack *s) {     //remove a node from the stack
    if (isEmpty(s))
        return -1;
    struct snode *tmp = s->head;
    for (int i = 1; i < s->size - 1; i++) {
        tmp = tmp->next;
    }
    struct snode *last = s->tail;
    s->tail = tmp;
    int bl_num = tmp->blocks_num;
    s->size--;
    if (isEmpty(s))
        s->head = NULL;
    free(tmp);
    return bl_num;
}

void DestroyStack(Stack *s) {
    for (int i = s->size; i != -1;)
        i = pop(s);
    free(s);

}

int Compare(void *value1, void *value2, char type) {    //compare two values with the same type
    //we return 1 if the first value is greater than the second
    //0 if they are equal
    //and -1 if the second is greater
    if (type == 'i') {
        int res = *(int *) value1 - *(int *) value2;
        if (res > 0)
            return 1;
        else if (res < 0)
            return -1;
        else
            return 0;
    } else if (type == 'f') {
        float res = *(float *) value1 - *(float *) value2;
        if (res > 0.0)
            return 1;
        else if (res < 0.0)
            return -1;
        else
            return 0;
    } else if (type == 'c') {
        int res = strcmp((char *) value1, (char *) value2);
        if (res > 0)
            return 1;
        else if (res < 0)
            return -1;
        else
            return 0;
    }
}

int Checkatrr(char attrType,
              int attrLength) {     //we check if we were given a valid attribute type and length
    if (attrType == 'c') {
        if (attrLength < 1 || attrLength > 255) {
            AM_errno = AME_INVALID_attrLENGTH;
            AM_PrintError("AM error occurred\n");
            return AME_INVALID_attrLENGTH;
        }
    } else if (attrType == 'i') {
        if (attrLength != sizeof(int)) {
            AM_errno = AME_INVALID_attrLENGTH;
            AM_PrintError("AM error occurred\n");
            return AME_INVALID_attrLENGTH;
        }
    } else if (attrType == 'f') {
        if (attrLength != sizeof(float)) {
            AM_errno = AME_INVALID_attrLENGTH;
            AM_PrintError("AM error occurred\n");
            return AME_INVALID_attrLENGTH;
        }
    }
}


struct filearray {      //struct for files
    char filename[100];
    int fd;         //fileDesc
    char attrType1;
    int attrLength1;
    char attrType2;
    int attrLength2;
    int max_entries_index;
    int max_entries_data;
    int head;               //root of tree
};

struct blockdata {
    char type; // I for index block, D for data block
    int counter; // Number of entries
    int pointer; // pointer to the next data block
};

typedef struct blockdata blockdata;

struct filearray *openfiles;


struct scandata {      //struct for scans
    int fileIndex;      //position in of scan's file in open file array
    void *value;        //value of scan
    int case_id; // case_id = 1-6 , depending on the case
    int current_block_num;      //current block we are in
    int current_position_in_block;
};

typedef struct scandata scandata;

scandata scanarray[MAXSCANS];

void write_to_block(char *data, void *value, char attrType, int attrLength) {
    if (attrType == 'i' || attrType == 'f')
        memcpy(data, value, attrLength);
    else if (attrType == 'c')
        strcpy(data, value);
}

void add_block_data(BF_Block *block, void *value1, void *value2, int fileIndex) {   //function to add values to a data block if there is space

    char *data = BF_Block_GetData(block);
    int counter;
    void *temp_val;
    int pairsize = openfiles[fileIndex].attrLength1 + openfiles[fileIndex].attrLength2;
    blockdata *bd = (blockdata *) data;
    counter = bd->counter;
    data = data + sizeof(blockdata);    //we skip the identifying struct
    temp_val = data;

    for (int i = 0; i < counter; ++i) { //we compare value1 with each value in the block
        if (Compare(value1, temp_val, openfiles[fileIndex].attrType1) <= 0) {   //if value1 is less than temp_val we have found it's position in the block
            memmove(temp_val + pairsize, temp_val, (counter - i) * pairsize);   //we move the values in the block so that value1 and value2 can fit and write them
            write_to_block(temp_val, value1, openfiles[fileIndex].attrType1, openfiles[fileIndex].attrLength1);
            write_to_block(temp_val + openfiles[fileIndex].attrLength1, value2, openfiles[fileIndex].attrType2,
                           openfiles[fileIndex].attrLength2);
            return;
        }
        temp_val = temp_val + pairsize; //temp_val point to the next value in the block
    }
    write_to_block(temp_val, value1, openfiles[fileIndex].attrType1, openfiles[fileIndex].attrLength1); //if never happened
    write_to_block(temp_val + openfiles[fileIndex].attrLength1, value2, openfiles[fileIndex].attrType2,
                   openfiles[fileIndex].attrLength2);

}

void add_index_data(BF_Block *block, void *value1, int block_num, int fileIndex) {  //function to add values to an index block if there is space

    char *data = BF_Block_GetData(block);
    int counter;
    void *temp_val;
    int pairsize = openfiles[fileIndex].attrLength1 + sizeof(int);
    blockdata *bd = (blockdata *) data;
    counter = bd->counter;
    data = data + sizeof(blockdata);
    temp_val = data + sizeof(int);

    for (int i = 0; i < counter; ++i) { //we compare value1 with each value in the block
        if (Compare(value1, temp_val, openfiles[fileIndex].attrType1) <= 0) {       //if value1 is less than temp_val we have found it's position in the block
            memmove(temp_val + pairsize, temp_val, (counter - i) * pairsize);   //we move the values in the block so that value1 and value2 can fit and write them
            write_to_block(temp_val, value1, openfiles[fileIndex].attrType1, openfiles[fileIndex].attrLength1);
            write_to_block(temp_val + openfiles[fileIndex].attrLength1, &block_num, 'i',
                           sizeof(int));
            return;

        }
        temp_val = temp_val + pairsize;
    }
    write_to_block(temp_val, value1, openfiles[fileIndex].attrType1, openfiles[fileIndex].attrLength1); //if never happened
    write_to_block(temp_val + openfiles[fileIndex].attrLength1, &block_num, 'i',
                   sizeof(int));
}

void AM_Init() {
    openfiles = malloc(sizeof(struct filearray) * MAX_OPEN_FILES);
    for (int i = 0; i < MAX_OPEN_FILES; ++i) {  //we initialize the arrays with -1
        openfiles[i].fd = -1;
    }

    for (int i = 0; i < MAXSCANS; ++i) {
        scanarray[i].fileIndex = -1;
    }
    CALL_BF(BF_Init(LRU));
    return;
}

int AM_CreateIndex(char *fileName,
                   char attrType1,
                   int attrLength1,
                   char attrType2,
                   int attrLength2) {
    if (Checkatrr(attrType1, attrLength1) == AME_INVALID_attrLENGTH
        || Checkatrr(attrType2, attrLength2) == AME_INVALID_attrLENGTH) {
        return AME_INVALID_attrLENGTH;
    }

    int fd, temp_val;
    BF_Block *b;
    char *data;

    temp_val = 1;

    CALL_BF(BF_CreateFile(fileName));
    CALL_BF(BF_OpenFile(fileName, &fd));
//first block
    BF_Block_Init(&b);
    CALL_BF(BF_AllocateBlock(fd, b));   //identifying block
    data = BF_Block_GetData(b);
    memcpy(data, &fd, sizeof(int));     //shows us that it is a valid file
    memcpy(data + sizeof(int), &temp_val, sizeof(int)); // pointer to the root block
    memcpy(data + 2 * sizeof(int), &attrType1, sizeof(attrType1));  //now we store the attributes and their lengths
    memcpy(data + 2 * sizeof(int) + sizeof(attrType1), &attrLength1, sizeof(attrLength1));
    memcpy(data + 2 * sizeof(int) + sizeof(attrType1) + sizeof(attrLength1), &attrType2, sizeof(attrType2));
    memcpy(data + 2 * sizeof(int) + sizeof(attrType1) + sizeof(attrLength1) + sizeof(attrType2), &attrLength2,
           sizeof(attrLength2));

    BF_Block_SetDirty(b);
    CALL_BF(BF_UnpinBlock(b));
    BF_Block_Destroy(&b);
    CALL_BF(BF_CloseFile(fd));
    return AME_OK;
}


int AM_DestroyIndex(char *fileName) {

    for (int i = 0; i < MAX_OPEN_FILES; ++i) {
        if (openfiles[i].fd != -1) {    //if there is an open file in the current array position we are in
            if (strcmp(openfiles[i].filename, fileName) == 0) { //file open
                AM_errno = AME_FILE_OPEN;
                AM_PrintError("AM error occurred\n");
                return AM_errno;
            }
        }
    }
    int ret = remove(fileName); //remove file
    if (ret == 0) {
        printf("File removed %s successfully\n", fileName);
    } else {
        AM_errno = AME_REMOVE_FAIL;
        AM_PrintError("AM error occurred\n");
        return AME_OK;
    }
    return AME_OK;
}


int AM_OpenIndex(char *fileName) {
    int fd;  //* filedesc *//
    int indexDesc;
    indexDesc = -1;
    BF_Block *b;
    char *data;
    int pair_size; // size of pointer and attr1

    CALL_BF(BF_OpenFile(fileName, &fd));
    for (int i = 0; i < MAX_OPEN_FILES; ++i) {
        if (openfiles[i].fd == -1) {    //valid position in the array
            //* initilize openfiles[i] *//
            openfiles[i].fd = fd;
            openfiles[i].filename;

            printf(" ");    //buffer error, do not delete
            strcpy(openfiles[i].filename, fileName);
            //* getting number of buckets from 1st block of file *//
            BF_Block_Init(&b);
            CALL_BF(BF_GetBlock(fd, 0, b));
            data = BF_Block_GetData(b);
            //we get the values from the identifying block and initialize opefiles values
            memcpy(&(openfiles[i].head), data + sizeof(int), sizeof(int));
            memcpy(&(openfiles[i].attrType1), data + 2 * sizeof(int), sizeof(char));
            memcpy(&(openfiles[i].attrLength1), data + 2 * sizeof(int) + sizeof(char), sizeof(int));
            memcpy(&(openfiles[i].attrType2), data + 3 * sizeof(int) + sizeof(char), sizeof(char));
            memcpy(&(openfiles[i].attrLength2), data + 3 * sizeof(int) + 2 * sizeof(char), sizeof(int));

            // calculation of max size of entries in index block
            pair_size = sizeof(int) + openfiles[i].attrLength1;
            openfiles[i].max_entries_index = (BF_BLOCK_SIZE - sizeof(blockdata)) / pair_size;
            //we subtract 1 so we can make sure there is space for the most right pointer
            if (openfiles[i].max_entries_index != 1)
                openfiles[i].max_entries_index--;
            // calculation of max size of entries in data block

            pair_size = openfiles[i].attrLength1 + openfiles[i].attrLength2;
            openfiles[i].max_entries_data = (BF_BLOCK_SIZE - sizeof(blockdata)) / pair_size;

            CALL_BF(BF_UnpinBlock(b));
            BF_Block_Destroy(&b);
            indexDesc = i; //* update indexDesc *//
            break;
        }
    }

    if (indexDesc == -1) {
        AM_errno = AME_MAXOPENFILES;
        AM_PrintError("AM error occurred\n");
    }

    return fd;
}


int AM_CloseIndex(int fileDesc) {
    int fileIndex = -1;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (openfiles[i].fd == fileDesc) {
            fileIndex = i;
            break;
        }
    }
    if (fileIndex == -1) {
        AM_errno = AME_FILE_NOT_OPEN;
        AM_PrintError("AM error occurred\n");
    }

    CALL_BF(BF_CloseFile(openfiles[fileIndex].fd));

    //* update open_files[fileDesc] *//
    openfiles[fileIndex].fd = -1;
    openfiles[fileIndex].attrType1 = '0';
    openfiles[fileIndex].attrLength1 = 0;
    openfiles[fileIndex].attrLength2 = 0;
    openfiles[fileIndex].attrType2 = '0';
    return AME_OK;
}

void *smash_block_data(BF_Block *block, void *value1, void *value2, int fileIndex, int *middle) {   //function used for spliting a data block
    char *data;
    void *copy_array = NULL;        //array that we copy in the values of the block and the value we want to insert
    data = BF_Block_GetData(block);
    blockdata *bd = (blockdata *) data;
    int counter = bd->counter;
    int pair_size = openfiles[fileIndex].attrLength1 + openfiles[fileIndex].attrLength2;
    copy_array = malloc((counter + 1) * pair_size);
    char *tmp;
    int position, flag;
    flag = 0;
    position = 0;
    data = data + sizeof(blockdata);

    memcpy(copy_array, data, pair_size * counter);  //copy values of the block in the array
    while (position < counter) {    //find the correct position of value1 in the array
        if (Compare(value1, copy_array + position * pair_size, openfiles[fileIndex].attrType1) <= 0) {
            break;
        }
        position++;
    }
    if (position < counter) {   //if value1 is not the last element in the block
        memmove(copy_array + (position + 1) * pair_size, copy_array + position * pair_size,
                (counter - position) * pair_size);
    }

    memcpy(copy_array + position * pair_size, value1, openfiles[fileIndex].attrLength1);   //store values

    memcpy(copy_array + position * pair_size + openfiles[fileIndex].attrLength1, value2,
           openfiles[fileIndex].attrLength2);
    *middle = (counter + 1) / 2;    //middle of array
    int midpos_count = 0; //poses thesis aristera apo thn mesh
    void *midvalue = copy_array + (*middle) * pair_size;
    void *data1 = midvalue - pair_size;
    while (memcmp(data1, midvalue, openfiles[fileIndex].attrLength1) == 0) {    //we move the middle left as many positions as the same values with the middle
        midpos_count++;
        data1 = data1 - pair_size;
    }
    *middle = *middle - midpos_count;
    return copy_array;
}

void *smash_index_data(BF_Block *block, void *value1, int block_num, int fileIndex, int *middle) {  //function used for spliting an index block, same logic as smash_data_block
    char *data;
    data = BF_Block_GetData(block);
    blockdata *bd = (blockdata *) data;
    int counter = bd->counter;
    void *copy_array;
    int pair_size = openfiles[fileIndex].attrLength1 + sizeof(int);
    copy_array = malloc((counter + 1) * pair_size + sizeof(int));
    int position;
    position = 0;
    data = data + sizeof(blockdata);
    for (int i = 0; i < counter; ++i) {     //copy values of block in the array
        memcpy(copy_array + i * pair_size, data + i * pair_size, pair_size);
    }
    memcpy(copy_array + counter * pair_size, data + counter * pair_size, sizeof(int));

    while (position < counter) {
        if (memcmp(value1, copy_array + sizeof(int) + position * pair_size, openfiles[fileIndex].attrLength1) <= 0) {
            break;
        }
        position++;
    }
    if (position < counter) {
        memmove(copy_array + sizeof(int) + (position + 1) * pair_size, copy_array + sizeof(int) + position * pair_size,
                (counter - position) * pair_size);
    }
    memcpy(copy_array + sizeof(int) + position * pair_size, value1, openfiles[fileIndex].attrLength1);
    memcpy(copy_array + sizeof(int) + position * pair_size + openfiles[fileIndex].attrLength1, &block_num, sizeof(int));

    if (counter % 2 == 0)   //middle depends on the number of elements
        *middle = (counter + 1) / 2 + 1;
    else
        *middle = (counter + 1) / 2;
    return copy_array;
}


int AM_InsertEntry(int fileDesc, void *value1, void *value2) {
    if (fileDesc >= MAX_OPEN_FILES) {
        AM_errno = AME_FILE_NOT_OPEN;
        AM_PrintError("AM error occurred\n");
    }

    int block_num;
    int fileIndex = -1;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {  //we find the file's position in the array
        if (openfiles[i].fd == fileDesc) {
            fileIndex = i;
            break;
        }
    }
    if (fileIndex == -1) {
        AM_errno = AME_FILE_NOT_OPEN;
        AM_PrintError("AM error occurred\n");
        return AME_FILE_NOT_OPEN;
    }

    CALL_BF(BF_GetBlockCounter(openfiles[fileIndex].fd, &block_num));

    if (block_num == 1) {   //only identifying block exists
        BF_Block *block1, *block2;
        char *data1, *data2;
        blockdata temp;
        int temp_val;

        BF_Block_Init(&block1);
        CALL_BF(BF_AllocateBlock(openfiles[fileIndex].fd, block1)); //allocate the first index block

        data1 = BF_Block_GetData(block1);

        temp.type = 'I';
        temp.counter = 1;
        temp.pointer = -1;
        temp_val = 2; // id of data block2
        memcpy(data1, &temp, sizeof(blockdata));
        memcpy(data1 + sizeof(blockdata), &temp.pointer, sizeof(int)); // set left pointer -1

        memcpy(data1 + sizeof(blockdata) + sizeof(int), value1, openfiles[fileIndex].attrLength1);

        memcpy(data1 + sizeof(blockdata) + sizeof(int) + openfiles[fileIndex].attrLength1, &temp_val,
               sizeof(int)); // right pointer indicates to data block2

        // initialize data block2
        BF_Block_Init(&block2);
        CALL_BF(BF_AllocateBlock(openfiles[fileIndex].fd, block2)); //allocate first index block

        data2 = BF_Block_GetData(block2);
        temp.type = 'D';
        temp.counter = 1;
        temp.pointer = -1;

        memcpy(data2, &temp, sizeof(blockdata));
        //store value1 and value2
        write_to_block(data2 + sizeof(blockdata), value1, openfiles[fileIndex].attrType1,
                       openfiles[fileIndex].attrLength1);
        data2 += sizeof(blockdata) + openfiles[fileIndex].attrLength1;
        write_to_block(data2, value2, openfiles[fileIndex].attrType2, openfiles[fileIndex].attrLength2);
        BF_Block_SetDirty(block1);
        CALL_BF(BF_UnpinBlock(block1));
        BF_Block_Destroy(&block1);

        BF_Block_SetDirty(block2);
        CALL_BF(BF_UnpinBlock(block2));
        BF_Block_Destroy(&block2);

    } else {
        Stack *stack;
        stack = create();   //we create a stack
        BF_Block *block;
        BF_Block_Init(&block);
        int inserted_flag = 0;
        BF_Block *identifying;
        char *id_data;
        int temp_pointer = 0;
        BF_Block_Init(&identifying);
        CALL_BF(BF_GetBlock(openfiles[fileIndex].fd, 0, identifying));  //we get the identifying block to get head's values
        id_data = BF_Block_GetData(identifying);

        memcpy(&(openfiles[fileIndex].head), id_data + sizeof(int), sizeof(int));
        CALL_BF(BF_UnpinBlock(identifying));
        BF_Block_Destroy(&identifying);
        CALL_BF(BF_GetBlock(openfiles[fileIndex].fd, openfiles[fileIndex].head, block));
        push(stack, openfiles[fileIndex].head); //store head in the stack
        char *data;
        data = BF_Block_GetData(block);
        blockdata *bd = (blockdata *) data;
        int counter = bd->counter;
        while (1) { //endless loop
            char *tmp;
            int flag = 0;
            tmp = data + sizeof(blockdata) + sizeof(int);
            int nblock; //here we store the next block's number

            for (int i = 0; i < counter; i++) {     //we find the right pointer for the value 1
                if (Compare(value1, tmp, openfiles[fileIndex].attrType1) <= 0) {

                    memcpy(&temp_pointer, tmp - sizeof(int), sizeof(int));  //copy the pointer's value to temp_pointer
                    flag = 1;   //flag true if found
                    memcpy(&nblock, tmp + openfiles[fileIndex].attrLength1, sizeof(int));   //copy the value of the pointer to the next block
                    break;
                }
                tmp += openfiles[fileIndex].attrLength1 + sizeof(int);
            }
            if (flag == 0) {
                //ftanoume sto telos kai to teleutaio value tou block einai megalutero
                // ara to bazoume ston aristero deikth tou
                memcpy(&temp_pointer, tmp - sizeof(int), sizeof(int));
                memcpy(&nblock, tmp - openfiles[fileIndex].attrLength1 - sizeof(int), sizeof(int));
            }

            if (temp_pointer == -1) {   //a block with this pointer doesn't exist
                memcpy(tmp - sizeof(int), &block_num, sizeof(int)); //we update the value of the identifying block we blocknum since block_num has the number of the block we will create
                blockdata temp;
                BF_Block_SetDirty(block);
                CALL_BF(BF_UnpinBlock(block));

                CALL_BF(BF_AllocateBlock(openfiles[fileIndex].fd, block));  //create new data block
                CALL_BF(BF_GetBlock(openfiles[fileIndex].fd, block_num, block));
                data = BF_Block_GetData(block);
                temp.type = 'D';
                temp.counter = 1;
                temp.pointer = nblock;

                memcpy(data, &temp, sizeof(blockdata));
                write_to_block(data + sizeof(blockdata), value1, openfiles[fileIndex].attrType1,
                               openfiles[fileIndex].attrLength1);
                data += sizeof(blockdata) + openfiles[fileIndex].attrLength1;
                write_to_block(data, value2, openfiles[fileIndex].attrType2, openfiles[fileIndex].attrLength2);
                BF_Block_SetDirty(block);
                CALL_BF(BF_UnpinBlock(block));
                BF_Block_Destroy(&block);
                inserted_flag = 1;
                break;
            }

            CALL_BF(BF_UnpinBlock(block));
            CALL_BF(BF_GetBlock(openfiles[fileIndex].fd, temp_pointer, block)); //get next block
            data = BF_Block_GetData(block);
            blockdata *tempbd = (blockdata *) data;

            if (tempbd->type == 'I') {  //if next block is an index block we insert it in the stack and do the same process for it
                counter = tempbd->counter;  //update counter
                push(stack, temp_pointer);
                continue;
            } else {    //next block is data block
                if (tempbd->counter < openfiles[fileIndex].max_entries_data) {  //there is space in the data block to insert the values
                    add_block_data(block, value1, value2, fileIndex);
                    tempbd->counter++;
                    memcpy(data, tempbd, sizeof(blockdata));    //update counter in the block
                    BF_Block_SetDirty(block);
                    CALL_BF(BF_UnpinBlock(block));
                    BF_Block_Destroy(&block);
                    break;
                } else {    //data block is full
                    BF_Block *temp;
                    char *tdata;
                    void *copy_array = NULL;
                    int pair_size = openfiles[fileIndex].attrLength1 + openfiles[fileIndex].attrLength2;
                    BF_Block_Init(&temp);
                    CALL_BF(BF_AllocateBlock(openfiles[fileIndex].fd, temp));
                    CALL_BF(BF_GetBlock(openfiles[fileIndex].fd, block_num, temp));     //this is the right block after the split
                    //prin eixame x-1 block kai to blocknum eixe timi x
                    counter = tempbd->counter;
                    tdata = BF_Block_GetData(temp);
                    blockdata temp_blockdata;
                    temp_blockdata.type = 'D';
                    temp_blockdata.pointer = tempbd->pointer;   //new block points to the block the left block pointed to
                    tempbd->pointer = block_num;  //kanw update ton pointer tou proigoumenou

                    int middle;
                    copy_array = smash_block_data(block, value1, value2, fileIndex, &middle);
                    temp_blockdata.counter = tempbd->counter - middle + 1;  //counter of right block
                    tempbd->counter = middle;   //counter of left block

                    memcpy(data, tempbd, sizeof(blockdata)); //aristero
                    memset(data + sizeof(blockdata), '0', openfiles[fileIndex].max_entries_data * pair_size);   //make every value 0
                    memcpy(data + sizeof(blockdata), copy_array, (middle) * pair_size);

                    memcpy(tdata, &temp_blockdata, sizeof(blockdata));   //de3i block
                    memcpy(tdata + sizeof(blockdata), copy_array + middle * pair_size,
                           (counter - middle + 1) * pair_size);
                    void *midval = NULL;    //the middle value that will ascend to the index block
                    midval = malloc(openfiles[fileIndex].attrLength1);

                    memcpy(midval, copy_array + middle * pair_size, openfiles[fileIndex].attrLength1);
                    BF_Block_SetDirty(temp);
                    BF_Block_SetDirty(block);

                    CALL_BF(BF_UnpinBlock(temp));
                    CALL_BF(BF_UnpinBlock(block));
                    BF_Block_Destroy(&block);

                    int index_block_num;

                    while (!isEmpty(stack)) {   //update the index blocks
                        char *index_data;
                        index_block_num = pop(stack);   //get the previous index block
                        CALL_BF(BF_GetBlock(openfiles[fileIndex].fd, index_block_num, temp));
                        index_data = BF_Block_GetData(temp);
                        blockdata *indexbd = (blockdata *) index_data;
                        if (indexbd->counter <
                            openfiles[fileIndex].max_entries_index) { //there is available space for entry in the index block
                            add_index_data(temp, midval, block_num, fileIndex);
                            indexbd->counter++;
                            memcpy(index_data, indexbd, sizeof(blockdata));
                            BF_Block_SetDirty(temp);
                            CALL_BF(BF_UnpinBlock(temp));
                            BF_Block_Destroy(&temp);
                            inserted_flag = 1;
                            break;
                        } else { // no available space in index block, so split index block and ascend entry
                            BF_Block *index_temp;
                            char *idata;
                            int rout_num = block_num + 1;
                            int pair_size = openfiles[fileIndex].attrLength1 + sizeof(int);
                            BF_Block_Init(&index_temp);
                            CALL_BF(BF_AllocateBlock(openfiles[fileIndex].fd, index_temp));
                            CALL_BF(BF_GetBlock(openfiles[fileIndex].fd, rout_num, index_temp));    //new right block

                            counter = indexbd->counter;
                            idata = BF_Block_GetData(index_temp); //right block
                            blockdata temp_indexdata;
                            temp_indexdata.type = 'I';
                            temp_indexdata.pointer = -1;
                            int middle;

                            copy_array = NULL;
                            void *index_copy_array;
                            index_copy_array = smash_index_data(temp, midval, block_num, fileIndex, &middle);

                            temp_indexdata.counter = indexbd->counter - middle; //right block counter
                            indexbd->counter = middle - 1; //left block counter

                            memcpy(index_data, indexbd, sizeof(blockdata)); //left
                            memset(index_data + sizeof(blockdata), '0',
                                   openfiles[fileIndex].max_entries_index * pair_size);
                            memcpy(index_data + sizeof(blockdata), index_copy_array,
                                   (middle - 1) * pair_size + sizeof(int));

                            memcpy(idata, &temp_indexdata, sizeof(blockdata));   //right block
                            memcpy(idata + sizeof(blockdata), index_copy_array + middle * pair_size, sizeof(int));
                            memcpy(idata + sizeof(blockdata) + sizeof(int), index_copy_array + (middle) * pair_size +
                                                                            sizeof(int),
                                   (counter - middle) * pair_size);
                            memcpy(midval, index_copy_array + (middle - 1) * pair_size + sizeof(int),
                                   openfiles[fileIndex].attrLength1);

                            BF_Block_SetDirty(index_temp);
                            BF_Block_SetDirty(temp);

                            CALL_BF(BF_UnpinBlock(temp));
                            CALL_BF(BF_UnpinBlock(index_temp));
                            BF_Block_Destroy(&temp);
                            if (index_block_num ==
                                openfiles[fileIndex].head) { //if we have reach the root we create a new one
                                CALL_BF(BF_AllocateBlock(openfiles[fileIndex].fd, index_temp));
                                idata = BF_Block_GetData(index_temp);
                                temp_indexdata.counter = 1;
                                int right_pointer = block_num + 1;

                                memcpy(idata, &temp_indexdata, sizeof(blockdata));
                                memcpy(idata + sizeof(blockdata), &index_block_num, sizeof(int));
                                memcpy(idata + sizeof(blockdata) + sizeof(int), midval,
                                       openfiles[fileIndex].attrLength1);
                                memcpy(idata + sizeof(blockdata) + sizeof(int) + openfiles[fileIndex].attrLength1,
                                       &right_pointer,
                                       sizeof(int));

                                openfiles[fileIndex].head = block_num + 2;  //we have created 3 new blocks so the number of root's block is block_num + 2
                                BF_Block_SetDirty(index_temp);
                                CALL_BF(BF_UnpinBlock(index_temp));
                                CALL_BF(BF_GetBlock(openfiles[fileIndex].fd, 0, index_temp));
                                idata = BF_Block_GetData(index_temp);
                                rout_num += 1;
                                memcpy(idata + sizeof(int), &rout_num, sizeof(int));    //update identifying block
                                BF_Block_SetDirty(index_temp);
                                CALL_BF(BF_UnpinBlock(index_temp));
                                BF_Block_Destroy(&index_temp);

                                break;

                            }
                            inserted_flag = 1;
                        }

                    }
                    inserted_flag = 1;

                }

            }
            if (inserted_flag == 1) {
                break;
            }

        }
    }
    return AME_OK;
}


int AM_OpenIndexScan(int fileDesc, int op,
                     void *value) { // ypothetoyme pws meta  tin openscan den ginetai kapoia insert gia na allaksei i domi tou dentrou
    int scan_value = -1;
    BF_Block *block;
    char *data;
    int fileIndex;
    int temp_pointer;

    BF_Block_Init(&block);

    for (int i = 0; i < MAXSCANS; ++i) {    //find available position in the scanarray
        if (scanarray[i].fileIndex == -1)
            scan_value = i;
    }

    for (int i = 0; i < MAX_OPEN_FILES; ++i) {  //find fileIndex's correct values
        if (openfiles[i].fd == fileDesc) {
            scanarray[scan_value].fileIndex = i;
            fileIndex = i;
        }
    }

    scanarray[scan_value].value = value;
    scanarray[scan_value].case_id = op;

    if (op == NOT_EQUAL || op == LESS_THAN || op == LESS_THAN_OR_EQUAL) {   //in these cases we go to the most left position in the data blocks
        CALL_BF(BF_GetBlock(fileDesc, openfiles[fileIndex].head, block));   //start from the root
        data = BF_Block_GetData(block);
        blockdata *block_data = (blockdata *) data;
        data = data + sizeof(blockdata);
        while (block_data->type == 'I') {   //we want to skip all index blocks

            memcpy(&temp_pointer, data, sizeof(int));
            if (temp_pointer == -1) {
                memcpy(&temp_pointer, data + sizeof(int) + openfiles[fileIndex].attrLength1, sizeof(int));
            }
            CALL_BF(BF_UnpinBlock(block));
            CALL_BF(BF_GetBlock(fileDesc, temp_pointer, block));
            data = BF_Block_GetData(block);
            block_data = (blockdata *) data;
            data = data + sizeof(blockdata);
        }
        if (Compare(value, data, openfiles[fileIndex].attrType1) < 0) { //value is greater than the value of data
            scanarray[scan_value].current_block_num = -100;
            scanarray[scan_value].current_position_in_block = -100;
        } else {
            scanarray[scan_value].current_block_num = temp_pointer;     //temp_pointer holds the number of block we want
            scanarray[scan_value].current_position_in_block = 0;        //start of block
        }
        CALL_BF(BF_UnpinBlock(block));
        BF_Block_Destroy(&block);
    } else if (op == EQUAL || op == GREATER_THAN || op == GREATER_THAN_OR_EQUAL) {  //we find the proper position according to the value
        CALL_BF(BF_GetBlock(fileDesc, openfiles[fileIndex].head, block));
        data = BF_Block_GetData(block);
        blockdata *block_data = (blockdata *) data;
        int pair_size = sizeof(int) + openfiles[fileIndex].attrLength1;
        int flag = 0;
        while (block_data->type == 'I') {   //we want to skip all index blocks
            data = data + sizeof(blockdata) + sizeof(int);
            for (int i = 0; i < block_data->counter; ++i) {
                if (Compare(value, data, openfiles[fileIndex].attrType1) == 0 && op != GREATER_THAN) {  //if they are equal we want to get the pointer to the right block since that's where we store the equal values
                    memcpy(&temp_pointer, data +openfiles[fileIndex].attrLength1, sizeof(int));
                    flag = 1;
                    break;
                } else if (Compare(value, data, openfiles[fileIndex].attrType1) < 0){   //if value is less than the value of data we get the pointer to the left block
                    memcpy(&temp_pointer, data - sizeof(int), sizeof(int));
                    flag = 1;
                    break;
                }
                data += pair_size;
            }
            if (flag == 0) {        //not found so we get the left pointer
                memcpy(&temp_pointer, data - sizeof(int), sizeof(int));
            }
            CALL_BF(BF_UnpinBlock(block));
            CALL_BF(BF_GetBlock(fileDesc, temp_pointer, block));
            data = BF_Block_GetData(block);
            block_data = (blockdata *) data;
        }
        data += sizeof(blockdata);
        scanarray[scan_value].current_block_num = temp_pointer;
        pair_size = openfiles[fileIndex].attrLength1 + openfiles[fileIndex].attrLength2;
        int j;
        for (j = 0; j < block_data->counter; ++j) { //we find the correct position in the block based on the op
            if (op == EQUAL) {

                if (Compare(value, data + j * pair_size, openfiles[fileIndex].attrType1) == 0) {
                    scanarray[scan_value].current_position_in_block = j;
                    break;
                }
            } else if (op == GREATER_THAN) {
                if (Compare(value, data + j * pair_size, openfiles[fileIndex].attrType1) < 0) {
                    scanarray[scan_value].current_position_in_block = j;
                    break;
                }
            } else if (op == GREATER_THAN_OR_EQUAL) {
                if (Compare(value, data + j * pair_size, openfiles[fileIndex].attrType1) <= 0) {
                    scanarray[scan_value].current_position_in_block = j;
                    break;
                }
            }
        }

        if (j == block_data->counter && op != EQUAL) {  //if we reach the end of a block in a non EQUAL case
            if (block_data->pointer == -1) {    //there isn't a next block so scan is invalid
                scanarray[scan_value].current_position_in_block = -100;
                scanarray[scan_value].current_block_num = -100;
            } else {    //we move to the next block
                scanarray[scan_value].current_block_num = block_data->pointer;
                scanarray[scan_value].current_position_in_block = 0;
            }
        }
        if (j == block_data->counter && op == EQUAL) {  //value doesn't exist
            scanarray[scan_value].current_position_in_block =-100;
            scanarray[scan_value].current_block_num = -100;
        }
        CALL_BF(BF_UnpinBlock(block));
        BF_Block_Destroy(&block);
    }

    return scan_value;
}


void *AM_FindNextEntry(int scanDesc) {
    int k;
    int op = scanarray[scanDesc].case_id;

    if (scanarray[scanDesc].current_block_num ==
        -1) { // den yparxoun alles  eggrafes, eimaste sto telos twn data blocks
        AM_errno = AME_EOF;
        return NULL;
    }
    if (scanarray[scanDesc].current_position_in_block == -100 &&
        scanarray[scanDesc].current_block_num == -100) {    //scan doesnt exist
        AM_errno = AME_SCAN_NOT_EXIST;
        AM_PrintError("AM error occurred\n");
        AM_errno = AME_EOF;
        return NULL;
    }

    //we could change the current position in block to -1 immediately , in cases like less than && less than or equal

    if (scanarray[scanDesc].current_position_in_block == -1) {
        AM_errno = AME_EOF;
        return NULL;
    }
    int fileIndex = scanarray[scanDesc].fileIndex;
    BF_Block *block;
    BF_Block_Init(&block);
    CALL_BF(BF_GetBlock(openfiles[fileIndex].fd, scanarray[scanDesc].current_block_num, block));
    char *data = BF_Block_GetData(block);
    blockdata *block_data = (blockdata *) data;
    void *NextEntry = NULL;
    data += sizeof(blockdata);
    switch (op) {
        case EQUAL:
            if (Compare(scanarray[scanDesc].value, data + scanarray[scanDesc].current_position_in_block *
                                                          (openfiles[fileIndex].attrLength1 +
                                                           openfiles[fileIndex].attrLength2),
                        openfiles[fileIndex].attrType1) == 0) {

                NextEntry = data + scanarray[scanDesc].current_position_in_block *
                                   (openfiles[fileIndex].attrLength1 + openfiles[fileIndex].attrLength2) +
                            openfiles[fileIndex].attrLength1;  //need value 2

            } else {    //we didn't find an equal
                NextEntry = NULL;
                AM_errno = AME_EOF;
                scanarray[scanDesc].current_position_in_block = -2;
            }
            break;
        case NOT_EQUAL:
            if (Compare(scanarray[scanDesc].value, data + scanarray[scanDesc].current_position_in_block *
                                                          (openfiles[fileIndex].attrLength1 +
                                                           openfiles[fileIndex].attrLength2),
                        openfiles[fileIndex].attrType1) != 0) { //we found the value we were looking for
                NextEntry = data + scanarray[scanDesc].current_position_in_block *
                                   (openfiles[fileIndex].attrLength1 + openfiles[fileIndex].attrLength2) +
                            openfiles[fileIndex].attrLength1;

            } else {
                while (Compare(scanarray[scanDesc].value, data + scanarray[scanDesc].current_position_in_block *
                                                                 (openfiles[fileIndex].attrLength1 +
                                                                  openfiles[fileIndex].attrLength2),
                               openfiles[fileIndex].attrType1) == 0) {  //we want to skip the values that are the same

                    scanarray[scanDesc].current_position_in_block++;
                    if (scanarray[scanDesc].current_position_in_block ==
                        openfiles[fileIndex].max_entries_data) { //periptwsi pou eimaste sto telos enos data block

                        scanarray[scanDesc].current_position_in_block = 0;
                        if (block_data->pointer == -1) {    //there isn't a next block
                            AM_errno = AME_EOF;
                            scanarray[scanDesc].current_position_in_block = -1;
                            return NULL;
                        } else {    //we move to the next block
                            scanarray[scanDesc].current_block_num = block_data->pointer;
                            CALL_BF(BF_UnpinBlock(block));
                            CALL_BF(BF_GetBlock(openfiles[fileIndex].fd, scanarray[scanDesc].current_block_num, block));
                            data = BF_Block_GetData(block);
                            block_data = (blockdata *) data;
                            data += sizeof(blockdata);
                        }
                    }
                }
                NextEntry = data + scanarray[scanDesc].current_position_in_block *
                                   (openfiles[fileIndex].attrLength1 + openfiles[fileIndex].attrLength2) +
                            openfiles[fileIndex].attrLength1;
            }
            break;
        case LESS_THAN:

            if (Compare(scanarray[scanDesc].value, data + scanarray[scanDesc].current_position_in_block *
                                                          (openfiles[fileIndex].attrLength1 +
                                                           openfiles[fileIndex].attrLength2),
                        openfiles[fileIndex].attrType1) > 0) {   //we compare with value,like strcmp

                NextEntry = data + scanarray[scanDesc].current_position_in_block *
                                   (openfiles[fileIndex].attrLength1 + openfiles[fileIndex].attrLength2) +
                            openfiles[fileIndex].attrLength1;

            } else {    // values less than value don't exist
                NextEntry = NULL;
                AM_errno = AME_EOF;
                scanarray[scanDesc].current_position_in_block = -2;
            }
            break;
        case GREATER_THAN:

            if (Compare(scanarray[scanDesc].value, data + scanarray[scanDesc].current_position_in_block *
                                                          (openfiles[fileIndex].attrLength1 +
                                                           openfiles[fileIndex].attrLength2),
                        openfiles[fileIndex].attrType1) < 0) {

                NextEntry = data + scanarray[scanDesc].current_position_in_block *
                                   (openfiles[fileIndex].attrLength1 + openfiles[fileIndex].attrLength2) +
                            openfiles[fileIndex].attrLength1;

            } else {
                NextEntry = NULL;
                AM_errno = AME_EOF;
                scanarray[scanDesc].current_position_in_block = -2;
            }
            break;
        case LESS_THAN_OR_EQUAL:
            if (Compare(scanarray[scanDesc].value, data + scanarray[scanDesc].current_position_in_block *
                                                          (openfiles[fileIndex].attrLength1 +
                                                           openfiles[fileIndex].attrLength2),
                        openfiles[fileIndex].attrType1) >= 0) {   //we compare with value,like strcmp

                NextEntry = data + scanarray[scanDesc].current_position_in_block *
                                   (openfiles[fileIndex].attrLength1 + openfiles[fileIndex].attrLength2) +
                            openfiles[fileIndex].attrLength1;

            } else {
                NextEntry = NULL;
                AM_errno = AME_EOF;
                scanarray[scanDesc].current_position_in_block = -2;
            }
            break;
        case GREATER_THAN_OR_EQUAL:
            if (Compare(scanarray[scanDesc].value, data + scanarray[scanDesc].current_position_in_block *
                                                          (openfiles[fileIndex].attrLength1 +
                                                           openfiles[fileIndex].attrLength2),
                        openfiles[fileIndex].attrType1) <= 0) {

                NextEntry = data + scanarray[scanDesc].current_position_in_block *
                                   (openfiles[fileIndex].attrLength1 + openfiles[fileIndex].attrLength2) +
                            openfiles[fileIndex].attrLength1;

            } else {
                NextEntry = NULL;
                AM_errno = AME_EOF;
                scanarray[scanDesc].current_position_in_block = -2;
            }
            break;
        default:
            printf("Out of range");
            break;
    }

    scanarray[scanDesc].current_position_in_block++;

    if (scanarray[scanDesc].current_position_in_block ==
        openfiles[fileIndex].max_entries_data || scanarray[scanDesc].current_position_in_block ==
                                                 block_data->counter) { //periptwsi pou eimaste sto telos enos data block
        scanarray[scanDesc].current_position_in_block = 0;
        if (block_data->pointer == -1) {
            scanarray[scanDesc].current_block_num = -1;
            scanarray[scanDesc].current_position_in_block = -1;
            AM_errno = AME_EOF;
        } else  //move to next block
            scanarray[scanDesc].current_block_num = block_data->pointer;
    }

    CALL_BF(BF_UnpinBlock(block));  //no set dirty , we did not change the block
    BF_Block_Destroy(&block);
    return NextEntry;
}

int AM_CloseIndexScan(int scanDesc) {   //we "close" a scan by having all the values in scanarray to show invalid values
    scanarray[scanDesc].fileIndex = -1;
    scanarray[scanDesc].value = NULL;
    scanarray[scanDesc].case_id = -1;
    scanarray[scanDesc].current_block_num = -1;
    scanarray[scanDesc].current_position_in_block = -1;
    return AME_OK;
}


void AM_PrintError(char *errString) {
    printf("%s", errString);
    switch (AM_errno) {
        case AME_MAXOPENFILES:
            printf("Max number of files opened\n");
            break;
        case AME_FILE_NOT_OPEN:
            printf("File doesn't exist\n");
            break;
        case AME_FILE_OPEN:
            printf("File is open\n");
            break;
        case AME_REMOVE_FAIL:
            printf("Error: unable to delete the file\n");
            break;
        case AME_INVALID_attrLENGTH:
            printf("Invalid attribute length given\n");
            break;
        case AME_SCAN_NOT_EXIST:
            printf("Element you are searching doesn't exist\n");
            break;
    }
    AM_errno = AME_OK;
}

void AM_Close() {
    free(openfiles);
    BF_Close();
}