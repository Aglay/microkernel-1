#include <userspace.h>

#define TIMESERVER_WAKEUP_QUEUE_SIZE 100

// -----------------------------------------------------------------------------------------------------------------------------------------------
// Timer
// -----------------------------------------------------------------------------------------------------------------------------------------------
void start_timer(){
	// Initialization
	int timerControlValue; 
	int *timerLoad = ( int * ) TIMER3_BASE;
	int *timerControl = ( int * ) ( TIMER3_BASE + CRTL_OFFSET ); 

	// First the load is added. 
	*timerLoad = TIMER_CYCLES_PER_TICK - 1;

	// The timer is enabled and configured.
	timerControlValue = *timerControl;
	timerControlValue = timerControlValue | TIMER_ENABLE_FLAG | TIMER_MODE;
	timerControlValue = timerControlValue & ~TIMER_CLKSEL;	// This enables the 2 kHz frequency. 
	*timerControl = timerControlValue;
}

// -----------------------------------------------------------------------------------------------------------------------------------------------
// Time Server Structs
// -----------------------------------------------------------------------------------------------------------------------------------------------
typedef struct Wakeup_record Wakeup_record;

struct Wakeup_record {
	int wakeup_time;
	int tid; 
	Wakeup_record *next_to_wakeup;
};

typedef struct {
	Wakeup_record records[TIMESERVER_WAKEUP_QUEUE_SIZE];
	Wakeup_record *first_to_wakeup;
	int size;
} Wakeup_list;

// -----------------------------------------------------------------------------------------------------------------------------------------------
// Time Server Methods
// -----------------------------------------------------------------------------------------------------------------------------------------------
void init_wakeup_list( Wakeup_list *list ) {
	list->size = 0;
}

void schedule_for_wakeup( Wakeup_list *list, int new_tid, int wakeup_time ) {
	bwassert( list->size < TIMESERVER_WAKEUP_QUEUE_SIZE,
		"Timeserver wakeup list should not overflow" );

	//Note: task id is a position of a correspondong record in a record list
	Wakeup_record *new_record = &(list->records[new_tid]);
	new_record->wakeup_time = wakeup_time;
	new_record->tid = new_tid; 

	if( list->size == 0 || wakeup_time < list->first_to_wakeup->wakeup_time ) {
		if ( list->first_to_wakeup )
			new_record->next_to_wakeup = list->first_to_wakeup; 
		else
			new_record->next_to_wakeup = 0;
		list->first_to_wakeup = new_record;
		
	} else {
		Wakeup_record *wake_before = list->first_to_wakeup;
		Wakeup_record *wake_after = wake_before->next_to_wakeup; 
		
		while( wake_after != 0 && wakeup_time > wake_after->wakeup_time ){
			wake_before = wake_after; 
			wake_after = wake_before->next_to_wakeup; 
		}
		
		wake_before->next_to_wakeup = new_record; 
		new_record->next_to_wakeup = wake_after; 
	}
	
	list->size++;
	bwdebug( DBG_SYS, "TIMESERVER: task %d is scheduled to run at %d ticks",
			new_tid, new_record->wakeup_time );
}

// Note: Returns -1 if there is no task to wakeup at the moment
int get_tid_to_wakeup( Wakeup_list *list, int current_time ) {	
	if( list->size == 0 || list->first_to_wakeup->wakeup_time > current_time ){
		return -1;
	}
	
	bwdebug( DBG_SYS, "TIMESERVER: list->records = %d, list->first_to_wakeup = %d ",
			list->records, list->first_to_wakeup );
	int tid_to_wakeup = list->first_to_wakeup->tid; 
	bwassert( tid_to_wakeup >= 0, "GET_TID_TO_WAKEUP: there is no magic in this world... :(" );
	
	list->first_to_wakeup = list->first_to_wakeup->next_to_wakeup;	
	list->size--;
	return tid_to_wakeup;
}

void clock_tick_notifier() {
	bwdebug( DBG_SYS, "TICK_NOTIFIER: start with tid %d", MyTid() );
	int timeserver_tid = WhoIs( "timeserver" );
	Msg_timeserver_request msg;
	msg.type = TICK_NOTIFICATION;

	FOREVER {
		bwdebug( DBG_SYS, "TICK_NOTIFIER: waiting for a tick " );
		AwaitEvent( TIMER1_INT_INDEX, 0 );
		bwdebug( DBG_SYS, "TICK_NOTIFIER: got a tick. Sending a poke to task %d",
			timeserver_tid);
		Send( timeserver_tid, (char *) &msg, sizeof(msg), (char *) 0, 0 );
	}
}

void timeserver() {
	bwdebug( DBG_SYS, "TIMESERVER: start" );
	
	start_timer();
	
	int mytid = MyTid();
	RegisterAs( "timeserver" );
	long current_time = 0;
	int sender_tid;
	int tid_to_unblock;
	Msg_timeserver_request request;
	Msg_timeserver_reply reply;
	Wakeup_list list;
	init_wakeup_list( &list );
	Create( 8, clock_tick_notifier );
	
	FOREVER {
		bwdebug( DBG_SYS, "TIMESERVER: listening as task %d...", mytid );
		Receive( &sender_tid, (char *) &request, sizeof(request) );
		switch( request.type ) {
		case TICK_NOTIFICATION:
			bwdebug( DBG_SYS, "TIMESERVER: tick notification recieved from task %d. "
				"[it's been %d ticks since start]",
				sender_tid , current_time);
			current_time++;
			tid_to_unblock = get_tid_to_wakeup( &list, current_time );
			if( tid_to_unblock >= 0 ) {
				bwdebug( DBG_SYS, "TIMESERVER: unblocking task %d",
					tid_to_unblock );
				Reply( tid_to_unblock, (char *) 0, 0 );
			}
			bwdebug( DBG_SYS, "TIMESERVER: tick notification reply dispatched" );
			Reply( sender_tid, (char *) 0, 0 );
			break;
		case TIME_REQUEST:
			bwdebug( DBG_SYS, "TIMESERVER: time request recieved from task %d",
				sender_tid );
			reply.type = TIME_REPLY;
			reply.num = current_time;
			Reply( sender_tid, (char *) &reply, sizeof(reply) );
			break;
		case DELAY_REQUEST:
			bwdebug( DBG_SYS, "TIMESERVER: delay for %d ticks request recieved "
				"from task %d", request.num, sender_tid );
			
			schedule_for_wakeup( &list, sender_tid, current_time + request.num );
			break;
		case DELAY_UNTIL_REQUEST:
			bwdebug( DBG_SYS, "TIMESERVER: delay until %d ticks request recieved "
				"from task %d", request.num, sender_tid );
			schedule_for_wakeup( &list, sender_tid, request.num );
			break;
		default:
			bwdebug( DBG_SYS, "TIMESERVER: *FATAL* unexpected message "
				"[type: %d from: %d]", request.type, sender_tid );
		}
	}

}
