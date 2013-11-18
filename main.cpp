#include <stdlib.h>
#include <stdio.h>
#include<iostream>
#include "procsim.hpp"

using namespace std;
enum { FALSE=-1, TRUE=1, FULL=2, EMPTY=3, HAS_ROOM=4, UNINITIALIZED=-2, READY=-3, DONE=-4};

//Register structure
typedef struct regstr
{
int tag;
} regstr;

//Register file
regstr REG[32];   

//CDB node
typedef struct CDBbus{
       int line_number;
       int ind;
       int tag;
       int reg;
       int FU;
} CDBbus;

//Linked list node
typedef struct node{
       node *prev;
       node *next;
       proc_inst_t p_inst;
       int instruction_number;
       int ind;
       int destTag;
       int src1Tag;
       int src2Tag;
       int age;
       int fetch;
       int disp;
       int sched;
       int exec;
       int state;
       int retire;
} node;

//Execute
node** FUfork0;
node** FUfork1;
node** FUfork2;


//Pointers for Linked List
typedef struct llPointers{
       node* head;
       node* tail;
       int size;
       int availExec;
} llPointers;


//Structure for ROB
typedef struct ROB{
       proc_inst_t p_inst;
       int line_number;
       int destTag;
       int done;
       int fetch;
       int disp;
       int sched;
       int exec;
       int state;
       int retire;
} ROB;
//Pointers for circular FIFO array
typedef struct Tablepointer{
       int head;
       int tail;
       int size;
} Tablepointer;


//Initialization Parameters
uint64_t ROBwidth = 0;
uint64_t k0Units = 0;
uint64_t k1Units = 0;
uint64_t k2Units = 0;
uint64_t fetchwidth = 0;
uint64_t mulfactor = 0;


//Dispatcher
llPointers dispatchPointers;

//Scheduler
llPointers typek0Q;
llPointers typek1Q;
llPointers typek2Q;



int k0Num, k1Num, k2Num;

ROB *StructureROB;
Tablepointer ROBindicator;
int cycle_number = 1;

CDBbus* CDB;
CDBbus* tempCDB;
int CDBsize = 0;
int tempCDBsize = 0;


int instruction;

int readDoneFlag = 1;
int ToContinue = 1;

void printResult(int index)
{
cout<< StructureROB[index].line_number<<"   "<< StructureROB[index].fetch<< "   "<< StructureROB[index].disp<<"   "<<  StructureROB[index].sched<<"   "<<  StructureROB[index].exec<<"   "<<  StructureROB[index].state<< "   "<< StructureROB[index].retire<<endl;
}


int spaceInROB()
{

       if(ROBindicator.size == 0 && ROBindicator.tail==ROBindicator.head)
       {
               return EMPTY;
       }
else if(ROBindicator.head == ROBindicator.tail)
       {
               return FULL;
       }
else
       {
               return HAS_ROOM;
       }
}



int FillROB(node* beforeDispatch){
       int index = ROBindicator.tail;

       if (spaceInROB()!=FULL){                        //if there is room in the ROB
               //Put item into ROB table
               StructureROB[ROBindicator.tail].line_number = beforeDispatch->instruction_number;
               StructureROB[ROBindicator.tail].destTag = beforeDispatch->destTag;
               StructureROB[ROBindicator.tail].p_inst = beforeDispatch->p_inst;
               StructureROB[ROBindicator.tail].done = 0;
               ROBindicator.tail= (ROBindicator.tail+1)%ROBwidth;
               ROBindicator.size++;
       }
else
       {
               return FALSE;
       }

       return index;
}

/*
* updateROB
* updates ROB entry to done
*
* parameters:
* int index - index that has completed
*
* returns:
* none
*/
void updateROB(int index){
       //Mark as complete
       StructureROB[index].done = 1;
}

/*
* removeROB
* Removes head element from ROB
*
* parameters:
* none
*
* returns:
* none
*/
void removeROB(){
       //Update stats
       StructureROB[ROBindicator.head].retire = cycle_number;
       //Print stats
       //printResult(ROBindicator.head);

       StructureROB[ROBindicator.head].done = 0;

       //Fix ROB queue
       ROBindicator.head = (ROBindicator.head+1)%ROBwidth;
       ROBindicator.size--;
}

/*
* modifyROB
* Puts infor in node to ROB
*
* parameters:
* node* print
*
* returns:
* none
*/
void modifyROB(node* modify){
       StructureROB[modify->ind].line_number = modify->instruction_number;
       StructureROB[modify->ind].fetch = modify->fetch;
       StructureROB[modify->ind].disp = modify->disp;
       StructureROB[modify->ind].sched = modify->sched;
       StructureROB[modify->ind].exec = modify->exec;
       StructureROB[modify->ind].state = modify->state;
       StructureROB[modify->ind].retire = modify->retire;
}

/*
* statusLL
* Returns status of the linked list
*/

int statusLL(llPointers pointersIn)
{
       if (pointersIn.size==0){
               return FULL;
       }else if(pointersIn.head == NULL){
               return EMPTY;
       }else{
               return HAS_ROOM;
       }
}

/*
* addLL
* Adds element to tail of linked list
*
* parameters:
* llPointers* pointersIn - pointers to linked list
* node* nextNode - node to add
*
* returns:
* int - success
*/
int addLL(node* nextNode,llPointers* pointersIn){
       //Add new element if there is enough room
               pointersIn->size--;        //Decrease room

               //Fix next and prev pointers
               nextNode->prev = pointersIn->tail;
               nextNode->next = NULL;

               //Fix existing tail and head
               if (pointersIn->tail != NULL){
                       pointersIn->tail->next = nextNode;
               }else
               {                //means it was empty before
                       //Fix array pointers
                       pointersIn->head = nextNode;
               }

               //Fix tail pointer
               pointersIn->tail = nextNode;


       return TRUE;
}

/*
* removeLL
* Removes element from linked list
*
* parameters:
* llPointers* pointersIn - pointers to linked list
* node* deleteNode - node to delete
* int freeMem                         - free memory
*
* returns:
* none
*/
void removeLL(llPointers* pointersIn, node* deleteNode, int freeMem)
{
       if (deleteNode->prev==NULL){                        //Head element
               if (deleteNode->next==NULL){                //Special case where there is only 1 element
                       pointersIn->head = NULL;
                       pointersIn->tail = NULL;
               }else{
                       pointersIn->head = deleteNode->next;
                       pointersIn->head->prev = NULL;
               }
       }else if (deleteNode->next==NULL){                //Tail element
               pointersIn->tail = deleteNode->prev;
               pointersIn->tail->next = NULL;
       } else{                                                                        //Element in middle
               deleteNode->prev->next = deleteNode->next;
               deleteNode->next->prev = deleteNode->prev;               
       }
       //Add room to linked list
       pointersIn->size++;

       //Free the node if desired
       if (freeMem==TRUE){
               free(deleteNode);                //Free allocated memory
       }
}

/*
* createNode
* Creates node for dispatcher
*
* parameters:
* int line_number - line number of instruction
* proc_inst_t p_inst - instruction of the node
*
* returns:
* node* - node that has been created
*/
node* addHolder(proc_inst_t p_inst, int line_number){
       node* dataholder = (node*) malloc(sizeof(node));        //Create a new node

     
       dataholder->p_inst = p_inst;
       dataholder->instruction_number = line_number;
       dataholder->destTag = line_number;

      
       dataholder->src1Tag = UNINITIALIZED;
       dataholder->src2Tag = UNINITIALIZED;
       dataholder->age = UNINITIALIZED;

       //Return Data
       return dataholder;
}

/*
* createNodeforSched
* Modifies node for use in scheduler
*
* parameters:
* node* beforeDispatch - node to be modified
*
* returns:
* none
*/
void createNodeforSched(node* beforeDispatch){

       //Add valididty data
       if (beforeDispatch->p_inst.src_reg[0]!=-1){
               beforeDispatch->src1Tag = REG[beforeDispatch->p_inst.src_reg[0]].tag;
       }else{
               beforeDispatch->src1Tag = READY;
       }
       if (beforeDispatch->p_inst.src_reg[1]!=-1){
               beforeDispatch->src2Tag = REG[beforeDispatch->p_inst.src_reg[1]].tag;
       }else{
               beforeDispatch->src2Tag = READY;
       }

       //Fix register file
       if (beforeDispatch->p_inst.dest_reg!=-1){
               REG[beforeDispatch->p_inst.dest_reg].tag = beforeDispatch->destTag;
       }

}



//Instr fetch


void FetchCycle(){
      
       node* instruction_num;
   
       proc_inst_t* p_inst;
      p_inst = (proc_inst_t*) malloc(sizeof(proc_inst_t));
       int address = TRUE;

            for (int i = 0; i<fetchwidth && address==TRUE; i++)
{
               if ((dispatchPointers.size+k0Num+k1Num+k2Num) > 0)
{
                       address = read_instruction(p_inst);                                                                        //fetch instruction
                      
                     
                       if (address==TRUE)
{               
                               instruction++;                                                                       
                               instruction_num = addHolder(*p_inst, instruction);
                               instruction_num->fetch = cycle_number;
                               instruction_num->disp = cycle_number+1;
                              
                               addLL(instruction_num, &dispatchPointers);        //add to dispatch queue

                       }
                   else
                       {
                               readDoneFlag = 0;
                       }
      
               }
/*else{
                       break;
               }*/
       }

       //Free memory
       //free(p_inst);
}

///////////////////////////DISPATCH///////////////////////////////////
/*
* setUpRegs
* Reads the registers for instructions
*
* parameters:
* none
** returns:
* none
*/
void setUpRegs(){
       int i = 0;

       //Node
       node* beforeDispatch, *whichQueue;

       //Initialize initial node to be dispatched
       beforeDispatch = dispatchPointers.head;

       //Read from dispatch queue
       while( i++ < (k0Num+k1Num+k2Num) && beforeDispatch!=NULL){
               //Add scheduling info
               createNodeforSched(beforeDispatch);

               //Go to next item in scheduler
               whichQueue = beforeDispatch;
               beforeDispatch = beforeDispatch->next;
               //Remove item from dispatcher queue
               removeLL(&dispatchPointers, whichQueue, FALSE);


               //Add to queue linked list
               if ((whichQueue->p_inst.op_code == 0 || whichQueue->p_inst.op_code == -1 ))
               {
                       addLL(whichQueue,&typek0Q);       
               }
               //Add to queue linked list
               if (whichQueue->p_inst.op_code == 1)
               {
                       addLL(whichQueue,&typek1Q);
      
               }
               //Add to queue linked list
               if (whichQueue->p_inst.op_code == 2)
               {
                       addLL(whichQueue,&typek2Q);
      
               }

       }
}


/*
* IntoScheduler
* Dispatch to Scheduler
*
* parameters:
* none
*
* returns:
* none
*/
void IntoScheduler(){
       //Node
      
      proc_inst_t Disp_instr;
       node* beforeDispatch;
       int unique;
       int hasItDispatched = 1;
  
       //Initialize initial node to be dispatched
       beforeDispatch = dispatchPointers.head;                //Node for instruction in dispatch queue

       //Read from dispatch queue
       while(hasItDispatched!=-1 && beforeDispatch!=NULL)
{

               Disp_instr = beforeDispatch->p_inst;         //Get instruction

             
               if ((Disp_instr.op_code == 0 || Disp_instr.op_code == -1 ) && (typek0Q.size-k0Num)>0)
{

                       if (spaceInROB()!=FULL){
                               k0Num++;
                              
                               beforeDispatch->age = READY;   
                               beforeDispatch->sched = cycle_number+1;

                               unique = FillROB(beforeDispatch);
                               beforeDispatch->ind = unique;
                               beforeDispatch = beforeDispatch->next;
                       }
                       else
                       {
                               hasItDispatched = -1;
                       }


               }

else

if(Disp_instr.op_code == 1 && (typek1Q.size-k1Num)>0)
{
                       if (spaceInROB()!=FULL)
{
                               k1Num++;
                        beforeDispatch->age = READY;
                             
                               beforeDispatch->sched = cycle_number+1;
                             
                               unique = FillROB(beforeDispatch);       
                             
                               beforeDispatch->ind = unique;
                        beforeDispatch = beforeDispatch->next;
                       }
                       else
                       {
                               hasItDispatched = -1;
                       }

               }

else if(Disp_instr.op_code == 2 && (typek2Q.size-k2Num)>0)
{
                      
if (spaceInROB()!=FULL)
{
                               k2Num++;
    beforeDispatch->age = READY;
                              
                               beforeDispatch->sched = cycle_number+1;
                               unique = FillROB(beforeDispatch);  //Add to ROB 
                               beforeDispatch->ind = unique;
                               beforeDispatch = beforeDispatch->next;
                       }
                   else
                       {
                              
                               hasItDispatched = -1;
                       }
               }
                   else
               {
                       hasItDispatched = -1;
               }

       }
}


/*
* DispCycleOne
* Dispatch Instcutions
*
* parameters:
* none
*
* returns:
* none
*/
void DispCycleOne()
{
          k0Num = 0;
       k1Num = 0;
       k2Num = 0;
        IntoScheduler();
}

/*
* DispCycleTwo()
* Dispatch Instcutions
*
* parameters:
* none
*
* returns:
* none
*/
void DispCycleTwo()
{
setUpRegs();
}


//Schedule

int checkAge(int unit)
{
       int count = 0;

       if (unit == 0)
{
int ctr = 0;
              while(ctr< k0Units)
{
                       if (FUfork1[ctr]!=NULL && FUfork1[ctr]->age == 1 )
                       { count++;
                       }
                       if (count>=k0Units)
                       { return 0;
                       }
ctr++;               }
       }
if (unit == 1)
           {  int ctr=0;
               while(ctr<k1Units*2)
{
                       if (FUfork1[ctr]!=NULL && FUfork1[ctr]->age == 2)
                       {
                       count++;
                       }

                       if (count>=k1Units)
                       {
                               return 0;
                       }

ctr++;                }
       }    
if (unit == 2)
{
int ctr = 0;
               while(ctr<k2Units*2)
                   {
                       if (FUfork2[ctr]!=NULL && FUfork2[ctr]->age == 3)
                       {
                               count++;
                       }
                       if (count>=k2Units)
                       {
                               return 0;
                       }
ctr++; }
       }

       return 1;
}

/*
* scheduleUpdate
* Updates scheduler with new values

*/
void scheduleUpdate(){
      
        node* DataHolder;

        //modify k0Units queue
        DataHolder = typek0Q.head;

        while (DataHolder!=NULL)
{
                //go through CDB
                for (int ctr = 0;ctr<CDBsize; ctr++)
                       {
                        if(CDB[ctr].tag==DataHolder->src1Tag)
                        {
                                DataHolder->src1Tag = READY;
                        }
                        if (CDB[ctr].tag==DataHolder->src2Tag){
                                DataHolder->src2Tag = READY;
                        }
                }
                //go to next element
                DataHolder = DataHolder->next;
        }

        //modify k1Units queue
        DataHolder = typek1Q.head;
        while (DataHolder!=NULL)
               {
                //go through CDB
                for (int ctr = 0;ctr<CDBsize; ctr++)
                   {
                        if(CDB[ctr].tag==DataHolder->src1Tag)
                       {
                                DataHolder->src1Tag = READY;
                        }
                        if (CDB[ctr].tag==DataHolder->src2Tag)
                       {
                                DataHolder->src2Tag = READY;
                        }
                }
                 //go to next element
                DataHolder = DataHolder->next;
        }

        //modify k2Units queue
        DataHolder = typek2Q.head;
        while (DataHolder!=NULL){
                //go through CDB
                for (int ctr = 0;ctr<CDBsize; ctr++){
                        if(CDB[ctr].tag==DataHolder->src1Tag){
                                DataHolder->src1Tag = READY;
                        }
                        if (CDB[ctr].tag==DataHolder->src2Tag){
                                DataHolder->src2Tag = READY;
                        }
                }
                 //go to next element
                DataHolder = DataHolder->next;
        }
}


/*
* AddtoFU
* Schedule Instrutions to FU
*
* parameters:
* none
*
* returns:
* none
*/
void AddtoFU()
{
      
       node* temp0 = typek0Q.head;
       node* temp1 = typek1Q.head;
       node* temp2 = typek2Q.head;

       //Do while there is room in all schedulers
       while(temp0 != NULL || temp1 != NULL || temp2 != NULL )
{

               //Check if item can be put in k0Units execute
               if (temp0!=NULL && typek0Q.availExec>0 && temp0->src1Tag == READY && temp0->src2Tag == READY && temp0->age == READY && checkAge(0))
{
//Add new node to list
                       typek0Q.availExec--;
                       temp0->age = 1;

                       //Add cycle_number info
                       temp0->exec = cycle_number+1;

                       //Store pointers for things currently in FU       
                       for (int ctr = 0; ctr<k0Units; ctr++){
                               if (FUfork0[ctr]==NULL){
                                       FUfork0[ctr] = temp0;
                                       break;
                               }
                       }
      
                       //Go to next element
                       temp0 = temp0->next;
               }
else if (temp0 != NULL)
               {
                       //Go to next element

                       temp0 = temp0->next;
               }

               //Check if item can be put in k1Units execute
               if (temp1!=NULL && typek1Q.availExec>0 && temp1->src1Tag == READY && temp1->src2Tag== READY && temp1->age == READY && checkAge(1))
                   {
                       //Add new node to list
                       typek1Q.availExec--;
                       temp1->age = 2;
                      
                       //Add cycle_number info
                       temp1->exec = cycle_number+1;

                       //Store pointers for things currently in FU       
                       for (int ctr = 0; ctr<k1Units*2; ctr++)
{
                               if (FUfork1[ctr]==NULL)
{
                                       FUfork1[ctr] = temp1;
                                       break;
                               }
                       }

                       //Go to next element
                       temp1 = temp1->next;
               }
else if (temp1 != NULL)
               {
                       //Go to next element
                       temp1 = temp1->next;
               }
              

               //Check if item can be put in k2Units execute
               if (temp2!=NULL && typek2Q.availExec>0 && temp2->src1Tag == READY && temp2->src2Tag== READY && temp2->age == READY && checkAge(2)){
                       //Add new node to list
                       typek2Q.availExec--;
                       temp2->age = 3;

                       //Add cycle_number info
                       temp2->exec = cycle_number+1;

                       //Store pointers for things currently in FU       
                       for (int ctr = 0; ctr<k2Units*3; ctr++){
                               if (FUfork2[ctr]==NULL){
                                       FUfork2[ctr] = temp2;
                                       break;
                               }
                       }       

                       //Go to next element
                       temp2 = temp2->next;
               }else if (temp2 != NULL){
                       //Go to next element
                       temp2 = temp2->next;
               }
       }

}

/*
* ScheduleCycleTwo()
* Schedule Instrutions
*
* parameters:
* none
*
* returns:
* none
*/
void ScheduleCycleOne()
{
       //Put on FU
       AddtoFU();
}
/*
* ScheduleCycleOne()
* Schedule Instrutions
*
* parameters:
* none
*
* returns:
* none
*/
void ScheduleCycleTwo()
{
       scheduleUpdate();
}

//EXECUTE

void updateReg(){
       //modify register file
       for (int i = 0; i < tempCDBsize; i++)
               {
               if (REG[tempCDB[i].reg].tag == tempCDB[i].tag){
                       REG[tempCDB[i].reg].tag = READY;
               }
       }
}


/*
* removeFU
* Removes Items from functional units
*
* parameters:
* none
*
* returns:
* none
*/
void removeFU(){
       //Execute k0Units instructions
       for (int ctr= 0; ctr<k0Units; ctr++){
               if (FUfork0[ctr] != NULL){
                       //Check if instructions is done
                       if (FUfork0[ctr]->age == DONE){
                               typek0Q.availExec++;
                               FUfork0[ctr] = NULL;
                       }
               }
       }

       //Execute k1Units instructions
       for (int ctr= 0; ctr<k1Units*2; ctr++){
               if (FUfork1[ctr] != NULL){
                       //Check if instructions is done
                       if (FUfork1[ctr]->age == DONE){
                               typek1Q.availExec++;
                               FUfork1[ctr] = NULL;
                       }
               }
       }       

       //Execute k2Units instructions
       for (int ctr= 0; ctr<k2Units*3; ctr++){
               if (FUfork2[ctr] != NULL){
                       //Check if instructions is done
                       if (FUfork2[ctr]->age == DONE){
                               typek2Q.availExec++;
                               FUfork2[ctr] = NULL;
                       }
               }
       }
}

/*
* incrementTimer
* Increment Age
*
* parameters:
* none
*
* returns:
* none
*/
void incrementTimer(){
       //Reset CDB bus
       tempCDBsize = 0;

       //Execute k0Units instructions
       for (int ctr= 0; ctr<k0Units; ctr++){
               if (FUfork0[ctr] != NULL){
                       //Decrease time left
                       FUfork0[ctr]->age--;
                       //Check if instructions is done
                       if (FUfork0[ctr]->age == 0){
                               tempCDB[tempCDBsize].tag = FUfork0[ctr]->destTag;
                               tempCDB[tempCDBsize].ind = FUfork0[ctr]->ind;
                               tempCDB[tempCDBsize].FU = 0;
                               tempCDB[tempCDBsize].line_number = FUfork0[ctr]->instruction_number;
                               tempCDB[tempCDBsize++].reg = FUfork0[ctr]->p_inst.dest_reg;
                               //Add cycle_number info
                               FUfork0[ctr]->state = cycle_number+1;

                               //Fix up FU array
                               FUfork0[ctr]->age = DONE;
                       }
               }
       }

       //Execute k1Units instructions
       for (int ctr= 0; ctr<k1Units*2; ctr++){
               if (FUfork1[ctr] != NULL){
                       //Decrease time left
                       FUfork1[ctr]->age--;
                       //Check if instructions is done
                       if (FUfork1[ctr]->age == 0){
                               tempCDB[tempCDBsize].tag = FUfork1[ctr]->destTag;
                               tempCDB[tempCDBsize].ind = FUfork1[ctr]->ind;
                               tempCDB[tempCDBsize].FU = 1;
                               tempCDB[tempCDBsize].line_number = FUfork1[ctr]->instruction_number;
                               tempCDB[tempCDBsize++].reg = FUfork1[ctr]->p_inst.dest_reg;       

                               //Add cycle_number info
                               FUfork1[ctr]->state = cycle_number+1;                               
                               //Fix up FU array                                       
                               FUfork1[ctr]->age = DONE;
                       }
               }
       }       

       //Execute k2Units instructions
       for (int ctr= 0; ctr<k2Units*3; ctr++){
               if (FUfork2[ctr] != NULL){
                       //Decrease time left
                       FUfork2[ctr]->age--;
                       //Check if instructions is done
                       if (FUfork2[ctr]->age == 0){
                               tempCDB[tempCDBsize].tag = FUfork2[ctr]->destTag;
                               tempCDB[tempCDBsize].ind = FUfork2[ctr]->ind;
                               tempCDB[tempCDBsize].FU = 2;
                               tempCDB[tempCDBsize].line_number = FUfork2[ctr]->instruction_number;
                               tempCDB[tempCDBsize++].reg = FUfork2[ctr]->p_inst.dest_reg;
                               //Add cycle_number info
                               FUfork2[ctr]->state = cycle_number+1;                                       
                               //Fix up FU array                                       
                               FUfork2[ctr]->age = DONE;
                       }
               }
       }

}

/*
* exchangeCDB
* Replace CDB
*
* parameters:
* none
*
* returns:
* none
*/
void exchangeCDB(){
       //Put temporary in correct
       for (int i = 0; i<tempCDBsize ;i++){
               CDB[i] = tempCDB[i];
       }
       CDBsize = tempCDBsize;
}
/*
* orderCDB
* Order CDB
*
* parameters:
* none
*
* returns:
* none
*/
void orderCDB(){
       //Temporary CDB holder
       CDBbus tempCDB2;

       //Go though all elements and switch as necessary
       for(int i=0; i<tempCDBsize; i++){
       for(int ctr=i; ctr<tempCDBsize; ctr++){
                  if(tempCDB[i].line_number > tempCDB[ctr].line_number){
                          tempCDB2=tempCDB[i];
                         tempCDB[i]=tempCDB[ctr];
                      tempCDB[ctr]=tempCDB2;
                  }
       }
   }

}

/*
* executeInstructions1
* Execute Instcutions
*
* parameters:
* none
*
* returns:
* none
*/
void ExecCycleOne(){
       //Fix aging of instructions in execute
       incrementTimer();
       //modify register
       updateReg();
       //Give up FU
       removeFU();
       //Order the CDB
       orderCDB();
}

/*
* executeInstructions2
* Execute Instcutions
*
* parameters:
* none
*
* returns:
* none
*/
void ExecCycletwo(){
       //Create teh correct CDB
       exchangeCDB();
}

/*
* markROBDone
* Mark instructions done in ROB
*
* parameters:
* none
*
* returns:
* none
*/
void markROBDone(){
       for (int i= 0; i < CDBsize; i++){
               updateROB(CDB[i].ind);
       }
}

/*
* DeleteFromSQ
* Remove completed instructions from scheduler
*
* parameters:
* none
*
* returns:
* none
*/
void DeleteFromSQ(){
       //Node for access to scheduler
       node* DataHolder;

       for(int ctr = 0;ctr<CDBsize; ctr++){
               if (CDB[ctr].FU == 0){
                       //Navigate though k0Units scheduler
                       DataHolder = typek0Q.head;
                        while (DataHolder!=NULL){
                                if (CDB[ctr].tag==DataHolder->destTag){
                                        DataHolder->retire = cycle_number;
                                        modifyROB(DataHolder);
                                        removeLL(&typek0Q, DataHolder,TRUE);
                                        break;
                                }
                                //go to next node
                                DataHolder = DataHolder->next;
                       }
               }else if(CDB[ctr].FU == 1){
                       //Navigate though k0Units scheduler
                       DataHolder = typek1Q.head;
                        while (DataHolder!=NULL){
                                if (CDB[ctr].tag==DataHolder->destTag){
                                        DataHolder->retire = cycle_number;
                                        modifyROB(DataHolder);
                                        removeLL(&typek1Q, DataHolder,TRUE);
                                        break;
                                }
                                //go to next node
                                DataHolder = DataHolder->next;
                       }
               }else if (CDB[ctr].FU == 2){
                       //Navigate though k0Units scheduler
                       DataHolder = typek2Q.head;
                        while (DataHolder!=NULL){
                                if (CDB[ctr].tag==DataHolder->destTag){
                                        DataHolder->retire = cycle_number;
                                        modifyROB(DataHolder);
                                        removeLL(&typek2Q, DataHolder,TRUE);
                                        break;
                                }
                                //go to next node
                                DataHolder = DataHolder->next;
                       }

               }
       }
}

/*
* Retirement
* Retire completed instruction in ROB
*
* parameters:
* none
*
* returns:
* none
*/
void Retirement(){
       int ROBiterator;
       int headNow = ROBindicator.head;

     
       for (int i = 0; i<fetchwidth; i++)
               {
               ROBiterator = (headNow + i)%ROBwidth;
             
               if ((cycle_number-StructureROB[ROBiterator].state)>0 && StructureROB[ROBiterator].done ==1)
               {      
                       removeROB();
               }
               else
               {            
                       break;
               }
       }

      
       if (readDoneFlag == 0 && spaceInROB()==EMPTY){
               ToContinue = 0;
       }
}


/*
* StateUpdateCycleone
* Update States
*
* parameters:
* none
*
* returns:
* none
*/
void StateUpdateCycleone(){
       markROBDone();
}

/*
* updateState2
* Update States
*
* parameters:
* none
*
* returns:
* none
*/
void StateUpdateCycletwo()
{
       DeleteFromSQ();
       Retirement();
}


/**
* Subroutine for initializing the processor. You many add and initialize any global or heap
* variables as needed.
* XXX: You're responsible for completing this routine
*
* @ROBwidth ROB size
* @k0Units Number of k0 FUs
* @k1Units Number of k1 FUs
* @k2Units Number of k2 FUs
* @fetchwidth Number of instructions to fetch
* @mulfactor Schedule queue multiplier
*/
void setup_proc(uint64_t r, uint64_t k0, uint64_t k1, uint64_t k2, uint64_t f, uint64_t m) {
      
        ROBwidth = r;
        k0Units = k0;
        k1Units = k1;
        k2Units = k2;
        fetchwidth = f;
        mulfactor = m;

       //cout<<"INST\tFETCH\tDISP\tSCHED\tEXEC\tSTATE\tRETIRE\n"<<endl;
        CDB = (CDBbus*) malloc((k0Units+k1Units+k2Units+20)*sizeof(CDBbus));
        StructureROB = (ROB*)malloc(ROBwidth*sizeof(ROB));   
        tempCDB = (CDBbus*) malloc((k0Units+k1Units+k2Units+20)*sizeof(CDBbus));
        ROBindicator = {0,0,0};
        for (int ctr = 0; ctr<32; ctr++)
        {
                REG[ctr].tag = READY;
        }

        FUfork0 = (node**) calloc(k0Units, k0Units*sizeof(node*));
        FUfork1 = (node**) calloc(k1Units, k1Units*2*sizeof(node*));
        FUfork2 = (node**) calloc(k2Units,k2Units*3*sizeof(node*));

      

        dispatchPointers ={NULL, NULL, (int)ROBwidth , (int)0};
        typek0Q = {NULL, NULL, (int)(mulfactor*k0Units) , (int)k0Units};
        typek1Q = {NULL, NULL, (int)(mulfactor*k1Units) , (int)k1Units*2};
        typek2Q = {NULL, NULL, (int)(mulfactor*k2Units) , (int)k2Units*3};

}

/**
* Subroutine that simulates the processor.
* The processor should fetch instructions as appropriate, until all instructions have executed
* XXX: You're responsible for completing this routine
*
* @p_stats Pointer to the statistics structure
*/
void run_proc(proc_stats_t* p_stats)
{ instruction = 0;
cycle_number = 0;

       while(ToContinue)
       { StateUpdateCycletwo();
               ExecCycletwo();
               ScheduleCycleTwo();
               DispCycleTwo();
               cycle_number++;

               StateUpdateCycleone();
               ExecCycleOne();
               ScheduleCycleOne();
               DispCycleOne();
               FetchCycle();
       }

       cycle_number = cycle_number - 1;
}

/**
* Subroutine for cleaning up any outstanding instructions and calculating overall statistics
* such as average IPC or branch prediction percentage
* XXX: You're responsible for completing this routine
*
* @p_stats Pointer to the statistics structure
*/
void complete_proc(proc_stats_t *p_stats)

{      cout<< "Hardware cost: "<<(ROBwidth*2+ (mulfactor +1)*(k0Units+k1Units+k2Units)+fetchwidth)<<endl;
       
p_stats->cycle_count = cycle_number;
       p_stats->retired_instruction = instruction;
       p_stats->avg_inst_retired = ((float)(instruction)/cycle_number);
}
