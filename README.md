# Project 2 Synchronization, IOS, 2 semster
## Evaluation: 15/15
## Task
In the system, we have three types of processes: (0) main process, (1) skibus, and (2) skier. 
Each skier goes to a bus stop after breakfast, where they wait for the bus to arrive. When 
the bus arrives at the bus stop, the skiers board the bus. If the bus reaches its capacity, 
the remaining skiers wait for the next bus. The bus then proceeds to service all the bus 
stops and takes the skiers to the drop-off point at the ski lift. If there are still more 
skiers wanting to be transported, the bus continues with another round.
## Usage
+ to execute use command "make" in the directory with proj.c and makefile
+ file for writing output will be created, now you can execute code using command
## Execution
+ $ ./proj2 L Z K TL TB
+ L is number of skyers, L < 20.000
+ Z is number of bus stops, 0 < Z <= 10
+ K is capacity of the skibus, 10 <= K <= 100
+ TL is maximum time in microseconds that a skier waits before arriving at the bus stop, 0 <= TL <= 10000
+ TB is maximum travel time of the bus between two stops, 0 <= TB <= 1000
+ The sequence of executed operations are stored in proj.out file after executing
