#include "kernel/kernel_globals.h"
#include "kernel/sysfuncs.h"
#include "kernel/helpers.h"
#include "lib/bwio.h"

int sys_create( int priority, void (*code) ( ), Task_descriptor *td, Kern_Globals *GLOBALS ) {
	
	debug( "sys_create: ENTERED" );

	// ERROR: Scheduler was given a wrong task priority.
	if( priority < 0 || priority >= SCHED_NUM_PRIORITIES ) return -1;
	
	// Getting the schedule
	Schedule *sched = &(GLOBALS->schedule);
	int new_tid;
	Task_descriptor *new_td;

	// Find a free task descriptor for a new task.
	new_tid = sched->last_issued_tid + 1;
	if( new_tid >= MAX_NUM_TASKS ) new_tid = 0;
	while( GLOBALS->tasks[new_tid].state != FREE_TASK ) {
		// ERROR: Scheduler is out of task descriptors. 
		if( ++new_tid >= MAX_NUM_TASKS ) return -2;
	}

	//Updating the schedule
	sched->last_issued_tid = new_tid;
	sched->tasks_alive++;
	
	// Setup new task descriptor
	new_td = &(GLOBALS->tasks[new_tid]);
	new_td->state = READY_TASK;
	new_td->priority = priority;
	new_td->lr = (int *)code;

	// Add new task descriptor to a proper scheduler queue
	Task_queue *queue = &(sched->priority[priority]);

	// ASSERT: Verifying the size of the queue
	assert( queue->size < SCHED_QUEUE_LENGTH, "Scheduler queue must not be full" );

	// If the queue is empty or the newest pointer is at the end of the td_ptrs buffer
	// put the next td_ptr at the beginning on the buffer  
	if (queue->size == 0 || ++(queue->newest) >= SCHED_QUEUE_LENGTH) queue->newest = 0;
	
	// If the queue was empty then newest and oldest elements are the same 
	// and are at the beginning of the buffer
	if (queue->size == 0) queue->oldest = 0;

	// Updating the queue
	queue->size++;
	queue->td_ptrs[queue->newest] = new_td;

	// Rescheduling the task
	sys_reschedule(td, GLOBALS);

	return new_tid;
}

int sys_mytid(Task_descriptor *td, Kern_Globals *GLOBALS )
{
	debug( "sys_mytid: ENTERED" );

	sys_reschedule(td, GLOBALS);
	return td->tid;
}

int sys_myparenttid(Task_descriptor *td, Kern_Globals *GLOBALS )
{
	debug( "sys_myparenttid: ENTERED");

	sys_reschedule(td, GLOBALS);
	return td->parent_tid;
}

void sys_pass(Task_descriptor *td, Kern_Globals *GLOBALS )
{
	debug( "sys_pass: ENTERED" );

	sys_reschedule(td, GLOBALS);
}

void sys_exit(Task_descriptor *td, Kern_Globals *GLOBALS ) 
{
	debug( "sys_exit: ENTERED" );

	// Getting task properties
	int priority = td->priority;
	
	// Getting the schedule
	Schedule *sched = &(GLOBALS->schedule);
	Task_queue *pqueue = &(sched->priority[priority]);

	// Removing the first task from the queue
	if (++(pqueue->oldest) >= SCHED_QUEUE_LENGTH) pqueue->oldest = 0;

	// Updating the task's state
	td->state = ZOMBIE_TASK;

	// Updating the queue
	(pqueue->size)--;

	// Updating the schedule
	sched->tasks_alive--;
}

void sys_reschedule(Task_descriptor *td, Kern_Globals *GLOBALS ){
	debug( "sys_reschedule: ENTERED" );

	// Getting task properties
	int priority = td->priority;
	
	// Getting the schedule
	Schedule *sched = &(GLOBALS->schedule);
	// Getting the priority queue
	Task_queue *pqueue = &(sched->priority[priority]);

	//If there are more than one task in the queue
	if(pqueue->size > 1)
	{

		// Removing the first task from the queue
		if (++(pqueue->oldest) >= SCHED_QUEUE_LENGTH) pqueue->oldest = 0;

		// Adding the task to the end of the queue
		if (++(pqueue->newest) >= SCHED_QUEUE_LENGTH) pqueue->newest = 0;
		pqueue->td_ptrs[pqueue->newest] = td;
	}

	// Updating the task's state
	td->state = READY_TASK;
}
