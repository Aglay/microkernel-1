#include "kernel/sched.h"
#include "kernel/kernel_globals.h"
#include "lib/bwio.h"
#include "kernel/helpers.h"

// Initialize Schedule sturct
void init_schedule( int first_task_priority, void (*first_task_code) ( ), Kern_Globals *GLOBALS ) {
	
	assert(first_task_priority < SCHED_NUM_PRIORITIES && first_task_priority >= 0,
		"first task should have priority between 0 and 16" );

	Schedule *sched = &(GLOBALS->schedule);
	int p;
	for (p = 0; p < SCHED_NUM_PRIORITIES; ++p)
	{
		sched->priority[p].oldest = 0;
		sched->priority[p].newest = 0;
		sched->priority[p].size = 0;
	}

	// add first task to the queue
	
	Task_descriptor *first_td = &(GLOBALS->tasks[0]);

	first_td->state = READY_TASK;
	first_td->lr = (int *)first_task_code;
	
	sched->priority[first_task_priority].newest = 0;
	sched->priority[first_task_priority].oldest = 0;
	sched->priority[first_task_priority].size++;

	sched->last_issued_tid = 0;

}

// Add task to scheduler as ready to run
int add_task( int priority, void (*code) ( ), Kern_Globals *GLOBALS ) {
	
	if( priority < 0 || priority >= SCHED_NUM_PRIORITIES ) return -1;
	// ERROR: Scheduler was given a wrong task priority.

	Schedule *sched = &(GLOBALS->schedule);
	int new_tid;
	Task_descriptor *new_td;

	// Find a free task descriptor for a new task.
	new_tid = sched->last_issued_tid + 1;
	if( new_tid >= MAX_NUM_TASKS ) new_tid = 0;
	while( GLOBALS->tasks[new_tid].state != FREE_TASK ) {
		if( ++new_tid >= MAX_NUM_TASKS ) return -2;
			// ERROR: Scheduler is out of task descriptors. 
	}
	sched->last_issued_tid = new_tid;
	
	// Setup new task descriptor
	
	new_td = &(GLOBALS->tasks[new_tid]);
	new_td->state = READY_TASK;
	new_td->lr = (int *)code;

	// Add new task descriptor to a proper scheduler queue
	
	Task_queue *queue = &(sched->priority[priority]);

	assert( queue->size < SCHED_QUEUE_LENGTH, "Scheduler queue must not be full" );

	// if the queue is empty or the newest pointer is at the end of the td_ptrs buffer
	// put the next td_ptr at the beginning on the buffer  
	if (queue->newest++ >= SCHED_QUEUE_LENGTH || queue->size == 0) queue->newest = 0;
	
	// if the queue was empty newest and oldest elements are the same and are at the
	// beginning of the buffer
	if (queue->size == 0) queue->oldest = 0;

	queue->size++;

	queue->td_ptrs[queue->newest] = new_td;

	return new_tid;
}

int parent_tid_syscall( Schedule *sched ) {
	return 0;
}


void pass_syscall( ) {

}


// Return:
// tid of the next task to run
int schedule( Kern_Globals * GLOBALS) {
	int p = SCHED_NUM_PRIORITIES - 1;
	while( GLOBALS->schedule.priority[p].size == 0 ) {
		--p;
		if( p < 0 ) panic( "Scheduler has nothing in it's queues." );
	}

	Task_queue *queue = &(GLOBALS->schedule.priority[p]);

	Task_descriptor *next_td = queue->td_ptrs[queue->oldest];

	if( ++queue->oldest >= SCHED_QUEUE_LENGTH ) queue->oldest = 0;
	queue->size = 0;

	return next_td->tid;
}

// Start running the taks with specified tid
// Return:
// interrupt ID of the first recieved interrupt
int activate( int tid, Kern_Globals *GLOBALS ) {
	Task_descriptor *td = &(GLOBALS->tasks[tid]);
	
	assert( td->state == READY_TASK, "It should only be possible to activate a READY task" );
	td->state = ACTIVE_TASK;

	//
	//	GRACEFULLY EXIT KERNEL
	//

	// Save kernel register
	asm (	"mov	ip, sp"																								"\n\t" );
	asm (	"stmfd	sp!, {r4, r5, r6, r7, r8, r9, r10, fp, ip, lr, pc}"	"\n\t" );
	
	// set PSR of the next active process
	asm ( "msr	spsr, %0" 																							"\n\t"
				:: "r" (td->spsr) );

	// set lr to jump to the next active process 
	asm ( "mov	lr, %0" 																								"\n\t"
				:: "r" (td->lr) );

	// set system mode
	asm ( "msr	cpsr, #0xdf" 																					"\n\t" );
	
	// set stack pointer of hte active process
	asm ( "mov 	sp, %0" 																							"\n\t"
				:: "r" (td->sp) );

	// load all process registers from *it's* stack
	asm ( "ldmfd sp!, {r0-r12, lr}" 																	"\n\t");

	// go back to supervisor mode
	asm ( "msr cpsr, #0xd3"																						"\n\t");
	
	// Jump to start executing active task
	// NOTE:this should set cpsr after the jump to contents of spsr 
	asm ( "movs pc, lr"																								"\n\t");

	// DONE
	

	//
	//	GRACEFULLY ENTER THE KERNEL
	//

	// Save task registers
	// 
	// etc...
	// 

	// return the interrupt ID
	return 0;

}


int getNextRequest( Kern_Globals *GLOBALS ) {

  return activate( schedule( GLOBALS ), GLOBALS );
}
