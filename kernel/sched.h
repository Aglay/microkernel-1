
#define SCHED_QUEUE_MAX_LENGTH 100
#define SCHED_NUM_PRIORITIES 16
#define SCHED_TID_MAX_VAL 2147483600

//Task descriptor
typedef struct {
	void (*code) ( );
	int tid;		//Task identifier, unique for each instance of the task
				//TODO: Task's state: READY, ACTIVE, ZOMBIE, etc.
				//TODO: Task's priority
				//TODO: Task identifier of the parent task
				//TODO: Stack pointer, which points to task's private memory
				//TODO: Task's saved program status register (SPSR)
				//TODO: Task's return value, which is to be return to the task during next execution
	int registers[16];
} Process;			//TODO: Rename to TD

typedef struct {
	Process buffer[SCHED_QUEUE_MAX_LENGTH];
	Process *oldest;
	Process *newest;
	int size;
} Process_queue;

typedef struct {
	Process_queue *priority[SCHED_NUM_PRIORITIES];
	int latest_tid;
} Schedule; 



int activate( int );

int schedule( int );

int Create( int, void (*) ( ) );

int getNextRequest( );

