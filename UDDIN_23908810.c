// Mahfuz Uddin
// CSCI 340 Project 2
/*
Instructions to Run Program:
1)
gcc UDDIN_23908810.c -o UDDIN_23908810.exe -pthread

2)Choose one of the below
./UDDIN_23908810.exe FCFS FCFS.txt UDDIN_23908810_FCFS.out         OR
./UDDIN_23908810.exe SJF SJF.txt UDDIN_23908810_SJF.out            OR
./UDDIN_23908810.exe PRIORITY PRIORITY.txt UDDIN_23908810_PRIORITY.out
*/

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>   //reading file
#include <stdlib.h>  //malloc
#include <string.h>  // strtok
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct {
  int id;                         // proccess id
  int (*someFunction)(int, int);  // stores the function
  int i;                          // parameter i
  int j;                          // parameter j
  int returnVal;                  // stores the return value of the function
  int priority;                   // priority used for priority scheduler
  int burstTime;                  // burstTime used for Shorest Job First scheduler
} miniPCB;

int f1, f2;  // files descriptors

const char* typeOfSchedAlg;  // argv[1] stores the name of the scheduler at run time
pthread_mutex_t mutex;

int sum(int i, int j);        // sum of numbers i to j
int product(int i, int j);    // factorial of j to i
int power(int i, int j);      // power of i to the j
int fibonacci(int i, int j);  // fibbonacci of j

miniPCB* fcfsScheduler(miniPCB** readyQueue);           // first come first serve scheduler
miniPCB* sjfScheduler(miniPCB** readyQueue);            // shortest job first scheduler
miniPCB* priorityScheduler(miniPCB** readyQueue);       // priority job scheduler
int myStringLength(char* string);                       // count the length of a string
int numLen(int number);                                 // count the length of a number
char* toString(int number);                             // convert a number to a string
int compareString(const char* str1, const char* str2);  // compare 2 strings
int getNumOfProcess(const char* argv);
// function pointer array with addresses of the sum, product, power, and fibonacci functions.
int (*mathFunctions[4])(int i, int j) = {sum, product, power, fibonacci};
// function pointer array with addresses of the FCFS scheduler, SJF scheduler, and Priority scheduler.
miniPCB* (*schedule[3])(miniPCB** readyQueue) = {fcfsScheduler, sjfScheduler, priorityScheduler};

miniPCB* FIFOqueue[1024];
int countFIFO = 0;
miniPCB* readyQueue[1024];
int countReady = 0;

// Implementing FIFO queue
void send(miniPCB* pcb) {
  if (countFIFO == 1024) {  // check if there is space in queue
    write(STDERR_FILENO, "no more space in queue", myStringLength("no more space in queue"));
    exit(0);
  }
  pthread_mutex_lock(&mutex);    // Entering critical section, so we lock
  FIFOqueue[countFIFO] = pcb;    // add pcb of proccess to queue
  countFIFO++;                   // increment size
  pthread_mutex_unlock(&mutex);  // Exiting critical section, so we unlock
}

miniPCB* receive() {
  if (countFIFO == 0) {  // check if queue is empty
    write(STDERR_FILENO, "No elements to remove", myStringLength("No elements to remove"));
    exit(0);
  }
  pthread_mutex_lock(&mutex);            // Entering critical section, so we lock
  miniPCB* result = FIFOqueue[0];        // remove the first pcb
  for (int i = 0; i < countFIFO; i++) {  // shift all the pcbs to the left
    FIFOqueue[i] = FIFOqueue[i + 1];
  }
  countFIFO--;                   // decrement size of FIFO queue
  pthread_mutex_unlock(&mutex);  // Exiting critical section, so we unlock
  return result;                 // return the pcb that was removed
}

// Implementing a Ready Queue
void insertIntoReadyQueue(miniPCB* pcb) {
  if (countReady == 1024) {  // Check if there is space in queue

    write(STDERR_FILENO, "no more space in queue", myStringLength("no more space in queue"));
    exit(0);
  }
  readyQueue[countReady] = pcb;  // add pcb of proccess to ready queue
  countReady++;                  // increment size
}

// remove from  queue and shift the rest of them to the left
miniPCB* removeFromReadyQueue(int position) {
  if (countReady == 0) {  // check if queue is empty
    write(STDERR_FILENO, "No elements to remove", myStringLength("No elements to remove"));
    exit(0);
  }
  miniPCB* result = readyQueue[position];  // remove the first pcb
  for (int i = position; i < countReady; i++) {
    readyQueue[i] = readyQueue[i + 1];  // shift all the pcbs to the left
  }
  countReady--;   // decrement size of FIFO queue
  return result;  // return the pcb that was removed
}

void* sched_dispatch_runner(void* arg) {
  miniPCB* toBeDispatched;
  char* F = "FCFS";
  char* S = "SJF";
  char* P = "PRIORITY";
  while (countReady != 0) {  // Keep looping until ready queue is empty
    // Based on the type of scheduler algorithm chosen, we dispatch accordingly
    if (compareString(typeOfSchedAlg, F) == 0) {
      toBeDispatched = fcfsScheduler(readyQueue);
    } else if (compareString(typeOfSchedAlg, S) == 0) {
      toBeDispatched = sjfScheduler(readyQueue);
    } else if (compareString(typeOfSchedAlg, P) == 0) {
      toBeDispatched = priorityScheduler(readyQueue);
    }
    // Call the math function and store the return value into PCB variable returnVal
    toBeDispatched->returnVal = (toBeDispatched->someFunction)(toBeDispatched->i, toBeDispatched->j);
    // Send to FIFO, so logger can then use it to write to output file
    send(toBeDispatched);
  }
  pthread_exit(NULL);  // End of thread, terminate thread
}
void* logger_runner() {
  while (countFIFO != 0) {
    miniPCB* recv = receive();  // Tecieve first PCB from FIFO queue

    int (*someFunction)(int, int) = recv->someFunction;  // Hold the function from PCB

    char* functionNameString;  // Convert function name to string
    if (someFunction == mathFunctions[2]) {
      functionNameString = "power";
    } else if (someFunction == mathFunctions[0]) {
      functionNameString = "sum";
    } else if (someFunction == mathFunctions[3]) {
      functionNameString = "fibonacci";
    } else if (someFunction == mathFunctions[1]) {
      functionNameString = "product";
    }
    // Convert each attribute of the PCB to string
    char* i_value_string = toString((int)recv->i);
    char* j_value_string = toString((int)recv->j);
    char* return_value_string = toString((int)recv->returnVal);

    // Write the attributes to the output file
    write(f2, functionNameString, myStringLength(functionNameString));
    write(f2, ",", 1);
    write(f2, i_value_string, myStringLength(i_value_string));
    write(f2, ",", 1);
    write(f2, j_value_string, myStringLength(j_value_string));
    write(f2, ",", 1);
    write(f2, return_value_string, myStringLength(return_value_string));
    write(f2, "\n", 1);

    // delete the pcb, free allocated memory
    free(recv);
  }

  pthread_exit(NULL);  // end of thread, terminate thread
}

int main(int argc, const char* argv[]) {
  if(argc != 4){
      return 1;      //improper arguments
  }
  if ((f2 = creat(argv[3], 0644)) == -1) {  // open output file
    write(STDERR_FILENO, "Can't Create Target File!", myStringLength("Can't Create Target File!"));
    exit(0);
  }

  typeOfSchedAlg = argv[1];          // Store the type of alogrithm chosen at run time
  pthread_mutex_init(&mutex, NULL);  // initalize mutex
  FILE* filePTR;                     // open input file
  filePTR = fopen(argv[2], "r");
  char singleLine[256];
  // read file until end of file
  while (!feof(filePTR)) {
    
    // allocate memory for PCB
    miniPCB* PCB_ptr = (miniPCB*)malloc(sizeof(miniPCB));
    fgets(singleLine, 256, filePTR);

    // read each attribute for a proccess and store it into PCB
    char* piece = strtok(singleLine, ",");
    PCB_ptr->id = atoi(piece);  // reading id

    if (compareString(argv[1], "FCFS") != 0) {
      piece = strtok(NULL, ",");

      if (compareString(argv[1], "SJF") == 0) {
        PCB_ptr->burstTime = atoi(piece);
      } else if (compareString(argv[1], "PRIORITY") == 0) {
        PCB_ptr->priority = atoi(piece);
      }
    }
    piece = strtok(NULL, ",");  // reading in the function name
    if (compareString(piece, "power") == 0) {
      PCB_ptr->someFunction = mathFunctions[2];
    } else if (compareString(piece, "sum") == 0) {
      PCB_ptr->someFunction = mathFunctions[0];
    } else if (compareString(piece, "fibonacci") == 0) {
      PCB_ptr->someFunction = mathFunctions[3];
    } else if (compareString(piece, "product") == 0) {
      PCB_ptr->someFunction = mathFunctions[1];
    }

    piece = strtok(NULL, ",");  // reading parameter i
    PCB_ptr->i = atoi(piece);

    piece = strtok(NULL, ",");  // reading parameter j
    PCB_ptr->j = atoi(piece);
    insertIntoReadyQueue(PCB_ptr);
  }

  fclose(filePTR);
  // thread id and attributes
  pthread_t thread_Id1;
  pthread_attr_t pthread_attributes1;
  pthread_attr_init(&pthread_attributes1);  // initialize thread 1

  pthread_t thread_Id2;  // thread1 id and attributes
  pthread_attr_t pthread_attributes2;
  pthread_attr_init(&pthread_attributes2);  // initialize thread 2

  // create both threads
  pthread_create(&thread_Id1, &pthread_attributes1, sched_dispatch_runner, NULL);
  pthread_create(&thread_Id2, &pthread_attributes2, logger_runner, NULL);

  // wait until both threads finish
  pthread_join(thread_Id1, NULL);
  pthread_join(thread_Id2, NULL);

  // free up resources
  pthread_mutex_destroy(&mutex);
  close(f1);
  close(f2);

  return 0;
}

// First come first served
miniPCB* fcfsScheduler(miniPCB** readyQueue) {
  return removeFromReadyQueue(0);  // return the the first PCB that enters the ready queue
}

// Shorest job (smallest burst time) first
miniPCB* sjfScheduler(miniPCB** readyQueue) {
  int currentMinBurstTime = 2147483647;
  int indexOfSmallestBurstTime = 0;

  // Find the PCB with the smallest burst time in the ready queue and return it
  for (int num = 0; num < countReady; num++) {
    miniPCB* current = readyQueue[num];

    if (current->burstTime < currentMinBurstTime) {
      currentMinBurstTime = readyQueue[num]->burstTime;
      indexOfSmallestBurstTime = num;
    }
  }
  miniPCB* processToBeExecuted = removeFromReadyQueue(indexOfSmallestBurstTime);
  return processToBeExecuted;
}
// highest priority first, priority 1 is highest
miniPCB* priorityScheduler(miniPCB** readyQueue) {
  int currentHighestPriority = 2147483647;
  int indexOfHighestPriority = 0;

  // Find the PCB with the highest priority in the ready Queue
  for (int num = 0; num < countReady; num++) {
    miniPCB* current = readyQueue[num];

    if (current->priority < currentHighestPriority) {
      currentHighestPriority = readyQueue[num]->priority;
      indexOfHighestPriority = num;
    }
  }
  miniPCB* processToBeExecuted = removeFromReadyQueue(indexOfHighestPriority);
  return processToBeExecuted;
}

int sum(int i, int j) {
  int sum = 0;
  for (int num = i; num <= j; num++) {
    sum += num;
  }
  return sum;
}

int product(int i, int j) {
  int facto = 1;

  if (i < 0) {
    write(STDOUT_FILENO, "Error! Factorial of a negative number doesn't exist.", myStringLength("Error! Factorial of a negative number doesn't exist."));
  } else {
    for (int num = j; num >= i; num--) {
      facto *= num;
    }
  }
  return facto;
}

int power(int i, int j) {
  int pow = 1;

  for (int num = 0; num < j; ++num) {
    pow *= i;
  }
  return pow;
}

int fibonacci(int i, int j) {
  int fibo[j + 2];
  int num;
  fibo[0] = 0;
  fibo[1] = 1;
  for (num = 2; num <= j; num++) {
    fibo[num] = fibo[num - 1] + fibo[num - 2];
  }
  return fibo[j];
}

// count the characters in a string until we a null character
int myStringLength(char* string) {
  int length = 0;
  int i = 0;
  while (string[i] != '\0') {
    ++length;
    ++i;
  }
  return length;
}

// converts a integer to a string
char* toString(int number) {
  int length = numLen(number);

  char* theNumber = malloc(length);
  for (int i = length; i > 0; i--) {
    theNumber[i - 1] = (number % 10) + '0';  // mod by 10 to get last digit and add '0' to get string represenation of the digit and store it in a[i]
    number = number / 10;                    // divide by 10 to get rid of last digit
  }
  return theNumber;
}

// returns length of a number
int numLen(int number) {
  if (number == 0) return 1;
  int count = 0;
  int n = number;
  while (n != 0) {
    n = n / 10;
    count++;
  }
  return count;
}

// compareString
int compareString(const char* str1, const char* str2) {
  while (*str1 && *str2) {
    if (*str1 != *str2) {
      break;
    }
    // move to the next pair of characters
    str1++;
    str2++;
  }

  // return the ASCII difference after converting `char*` to `unsigned char*`
  //if the strings are the same length and they have the same last char, the difference is 0
  //If the chars at the end don't match, their difference won't equal 0.
  return *(const unsigned char*)str1 - *(const unsigned char*)str2;
}