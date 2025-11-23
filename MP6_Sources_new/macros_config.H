/* 
    File: macros_config.H
    Author: Nithin Gowtham Saravanan
            Department of Electrical and Computer Engineering
            Texas A&M University
    Date  : 11/01/2025
	
    Description: This file is used to handle the macro definitions across 
                 different source code files (including kernel.C)
*/

#define _USES_SCHEDULER_        // uncomment to enable FIFO scheduling
#define _THREADSAFE_DISK_       // uncomment to verify thread safe disk behavior
// #define _TERMINATING_FUNCTIONS_ // uncomment to terminate threads 1 and 2 in kernel.c
// #define _RR_SCHEDULER_          // uncomment to enable Round-Robin