#include "kernelspace.h"

extern void swi_main_handler();
extern void main_interrupt_handler(); // This should be used for both, SWI and HWI. 

// ----------------------------------------------------------------------------
// Interrupt Handling
// - This part deals with returning control from a task to the kernel (due to a 
//   HW or SW interrupt). 
// ----------------------------------------------------------------------------
asm(
												"\n\t"
	// ------------------------------------------------------------------------
	// Variables
	// ------------------------------------------------------------------------
	// TODO: Put this in a header file. (Check how to compile and include it.)
	".EQU	IRQ_MODE,			0x92"			"\n\t"
	".EQU	SVC_MODE,			0x93"			"\n\t"
	".EQU	SVC_MODE_NO_INTS,	0x13"				"\n\t"
	".EQU	SYS_MODE,			0x9F"			"\n\t"

	// ------------------------------------------------------------------------
	// Debugging
	// ------------------------------------------------------------------------
	/*
	 * This is a method used to bwdebug assembly code. It uses bwputr, but does
	 * not modify the registers. Therefore, it doesn't corrupt the state. 
	 * NOTE: This method prints the value in register 4 since it's never used. 
	 */										
	"asm_debug_bwputr:"							"\n\t"
		"STMFD	sp!, { r0-r3 }"					"\n\t"
		"MOV	r0, #1"							"\n\t"
		"MOV	r1, r4"							"\n\t"
		"BL		bwputr"							"\n\t"
		"LDMFD	sp!, { r0-r3 }"					"\n\t"
		"MOV	pc, lr"							"\n\t"

	// ------------------------------------------------------------------------
	// Interrupt Handler
	// ------------------------------------------------------------------------
    /*
     * This method is used for both, SW and HW interrupts. 
     */
	"main_interrupt_handler:"					"\n\t"

		// Determine the mode.
		"SUB	sp, sp, #4"						"\n\t"	// Add space for the return value. 
		"STMFD	sp!, { r0, r1 }"				"\n\t"		
		"MRS	r0, cpsr"						"\n\t"
		"AND	r0, r0, #0x1F"					"\n\t"	// Get only the mode (the last 5 bits represent the mode.)
		"TEQ	r0, #SVC_MODE_NO_INTS"			"\n\t"	// Is this SVC mode? This will set the Z flag that will be read later. 
		"BNE	hw_interrupt"					"\n\t"

	// -- SW Interrupt --------------------------------------------------------
												"\n"
	"sw_interrupt:"								"\n\t"
		"LDR	r0, [ lr, #-4 ]"				"\n\t"	// For SW interrupts the return value is the SWI parameter. 
		"BIC	r0, r0, #0xff000000"			"\n\t"
		"STR	r0, [ sp, #8 ]"					"\n\t"
		"LDMFD	sp!, { r0, r1 }"				"\n\t"	// Retrieve the temporarily stored registers. 
		"B		store_task_registers"			"\n\t"

	// -- HW Interrupt --------------------------------------------------------
												"\n"
	"hw_interrupt:"								"\n\t"
		"MRS	r0, spsr"						"\n\t"	// Copy the LR and SPSR from IRQ to SVC. 
		"MOV	r1, lr"							"\n\t"
		"SUB	r1, r1, #4"						"\n\t"	// The LR in HW interrupts is shifted by 4 by the HW. Adjust it here. 
		"MSR	cpsr_c, #SVC_MODE"				"\n\t"	// Switch to SVC mode. 
		"MOV	lr, r1"							"\n\t"
		"MSR	spsr, r0"						"\n\t"
		"SUB	sp, sp, #4"						"\n\t"	// Store the return value.
		"MOV	r0, #-1"						"\n\t"	// The return value for HW interrupts is -1. 
		"STR	r0, [ sp, #0 ]"					"\n\t"
		"MSR	cpsr_c, #IRQ_MODE"				"\n\t"	// Return to IRQ mode and retrieve the temporarily stored registers.
		"LDMFD	sp!, { r0, r1 }"				"\n\t"
		"ADD	sp, sp, #4"						"\n\t"

	// -- SWI AND HW ---------------------------------------------------------
												"\n"
	"store_task_registers:"						"\n\t"
		"MSR	cpsr_c, #SYS_MODE"				"\n\t"  // Switch to system mode.
		"SUB	sp, sp, #4"						"\n\t"  // Leave space for SPSR. 
		"STMFD	sp!, { r0-r12, lr }"			"\n\t"  // Store all the registers (except 13-sp- and 15-pc-)
		"MOV    r0, sp"							"\n\t"  // Store the task's SP so that it can be later stored in the TR. 
		// "MOV	r1, lr"							"\n\t"	// DELETE THIS!!!
		"MSR	CPSR_c, #SVC_MODE"				"\n\t"	// Switch to SVC mode

		// Restore the kernel state. 
		"LDR	r3, [ sp, #0 ]"					"\n\t"
		"ADD	sp, sp, #4"						"\n\t"
		"MOV	r1, lr"							"\n\t"	// This seems suspicious. 
		"MRS	r2, spsr"						"\n\t"
		"STR	r2, [ r0, #14*4 ]"				"\n\t"	// Store the retrieved SPSR in the stack. 
		"LDMFD	sp!, { r4-r11 }"				"\n\t"

		// Save the TD information. 
		// R0 - Task stack pointer. 
		// R1 - Next address to execute (LR).
		// R2 - Task decriptor pointer. 
		"LDR	r2, [ sp, #0 ]"					"\n\t"  // The pointer to the active TD is loaded into register 2.
		"STR	r3, [ sp, #0 ]"					"\n\t"	// Store the return value so that it's not overwritten. 
		"BL		StoreTaskInformation"			"\n\t"

		// DEBUG!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		//"MRS	r1, spsr"						"\n\t"
		//"MOV	r0, #1"							"\n\t"
		//"BL		bwputr"						"\n\t"
		// DEBUG!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

		// Return control to the kernel. 
		"LDR	r0, [ sp, #0 ]"					"\n\t"	// Load the return value. 
		"SUBS	sp, fp, #12"					"\n\t"	//NOTE: This instruction also resets the Z flag (activated during TEQ.)
		"LDMFD	sp, { fp, sp, pc }"				"\n\t"
); 


void
StoreTaskInformation( unsigned int taskSP, unsigned int lr, unsigned int activeTD ) {
	
	// Update the task descriptor
	Task_descriptor *td = (Task_descriptor *) activeTD;
	bwdebug( DBG_KERN, CONTEXT_SW_DEBUG_AREA, "EXECUTE_CSWI_HANDLER: enters [return address: %d, tid: %d]", lr, td->tid );
	td->sp = (int *) taskSP;
	td->lr = (int *) lr;
}

void
handle_request( int request, Kern_Globals *GLOBALS ) {
	
	bwdebug( DBG_KERN, CONTEXT_SW_DEBUG_AREA, "HANDLE_Request: entered [request id: %d]", request );
	if ( request < 0 ) {
		handle_hwi( GLOBALS ); 
	}
	else {
		handle_swi( request, GLOBALS ); 
	}
}

void 
handle_hwi( Kern_Globals *GLOBALS ){
	
	int *vic1_hw_interrupt = ( int * ) INT_CONTROL_BASE_1 + IRQ_STATUS_OFFSET;
	int *vic2_hw_interrupt = ( int * ) INT_CONTROL_BASE_2 + IRQ_STATUS_OFFSET;
	
	Task_descriptor *td = &(GLOBALS->tasks[GLOBALS->scheduler.last_active_tid]);
	bwdebug( DBG_KERN, HWI_DEBUG_AREA, "HANDLE_HWI: entered [VIC1 interrupt id: %d VIC2 interrupt id: %d caller: ]", 
			*vic1_hw_interrupt, *vic2_hw_interrupt, td->tid );
	
	// There could be multiple interrupts enabled. Therefore this is handled
	// in a while loop and not a switch statement. 
	//while( *vic1_hw_interrupt || *vic2_hw_interrupt ) {
	if ( *vic1_hw_interrupt || *vic2_hw_interrupt ){
		
		
		// Timer
		if ( *vic2_hw_interrupt & TIMER3_INT ){
			bwdebug( DBG_KERN, HWI_DEBUG_AREA, "TIMER_INTERRUPT: handling" );
			timer_hwi_handler( GLOBALS );
			bwdebug( DBG_KERN, HWI_DEBUG_AREA, "TIMER_INTERRUPT: handled" );
		}
		
		// UART 1
		if ( *vic2_hw_interrupt & UART1_INT ){
			bwdebug( DBG_KERN, HWI_DEBUG_AREA, "UART1_INTERRUPT: handling" );
			uart1_hwi_handler( GLOBALS ); 
			bwdebug( DBG_KERN, HWI_DEBUG_AREA, "UART1_INTERRUPT: handled" );
		}
		
		// UART 2
		if ( *vic2_hw_interrupt & UART2_INT ){
			bwdebug( DBG_KERN, HWI_DEBUG_AREA, "UART2_INTERRUPT: handling" );
			uart2_hwi_handler( GLOBALS ); 
			bwdebug( DBG_KERN, HWI_DEBUG_AREA, "UART2_INTERRUPT: handled" );
		}
	}
	
	//Rescheduling the interrupted task
	sys_reschedule( td, GLOBALS );
}

// NOTE: Shutdown syscall gets handles in kernel.c and never gets here
void 
handle_swi( int request, Kern_Globals *GLOBALS ){
	bwdebug( DBG_KERN, SWI_DEBUG_AREA, "HANDLE_SWI: entered [request id: %d]", request );
	
	// Create a placeholder for the arguments.
	int sysCallArguments[MAX_NUM_ARGUMENTS];

	Task_descriptor *td = &( GLOBALS->tasks[GLOBALS->scheduler.last_active_tid] );
	
	int returnValue;
	unsigned int taskSP = (unsigned int) td->sp;

	switch( request ) {
	case CREATE_SYSCALL:
		RetrieveSysCallArgs( sysCallArguments, CREATE_ARGS, taskSP );
		returnValue = sys_create(
							sysCallArguments[0],
							(void *) sysCallArguments[1],
							td, GLOBALS );
		
		SetSysCallReturn( returnValue, taskSP );
		break;
	case MYTID_SYSCALL:
		returnValue = sys_mytid( td, GLOBALS );
		SetSysCallReturn( returnValue, taskSP );
		break;
	case MYPARENTTID_SYSCALL:
		returnValue = sys_myparenttid( td, GLOBALS );
		SetSysCallReturn( returnValue, taskSP );
		break;
	case PASS_SYSCALL:
		sys_pass( td, GLOBALS );
		break;
	case EXIT_SYSCALL:
		sys_exit( td, GLOBALS );
		break;
	case SYSTEM_DATA_SYSCALL:
		RetrieveSysCallArgs( sysCallArguments, SYSTEM_DATA_ARGS, taskSP );
		sys_get_system_data( (System_data *) sysCallArguments[0], td, GLOBALS );
		break;
	case TESTCALL_SYSCALL:
		RetrieveSysCallArgs( sysCallArguments, TESTCALL_ARGS, taskSP );
		returnValue = sys_testcall(
						sysCallArguments[0], 
						sysCallArguments[1], 
						sysCallArguments[2], 
						sysCallArguments[3], 
						sysCallArguments[4], 
						sysCallArguments[5] );
		SetSysCallReturn( returnValue, taskSP );
		break;
	case SEND_SYSCALL:
		RetrieveSysCallArgs( sysCallArguments, SEND_ARGS, taskSP );
		returnValue = sys_send(
						(int) 		sysCallArguments[0], 
						(char *) 	sysCallArguments[1], 
						(int) 		sysCallArguments[2],
					   	(char *) 	sysCallArguments[3], 
					   	(int) 		sysCallArguments[4],
					   	td, GLOBALS );
		SetSysCallReturn( returnValue, taskSP );
		break;
	case RECEIVE_SYSCALL:
		RetrieveSysCallArgs( sysCallArguments, RECEIVE_ARGS, taskSP );
		returnValue = sys_receive(
						(int *)		sysCallArguments[0], 
						(char *) 	sysCallArguments[1], 
						(int) 		sysCallArguments[2],
						td, GLOBALS);
		SetSysCallReturn( returnValue, taskSP );
		break;
	case REPLY_SYSCALL:
		RetrieveSysCallArgs( sysCallArguments, REPLY_ARGS, taskSP );
		returnValue = sys_reply(
						(int)		sysCallArguments[0], 
						(char *)	sysCallArguments[1], 
						(int)		sysCallArguments[2],
						td, GLOBALS );
		SetSysCallReturn( returnValue, taskSP );
		break;
	case AWAIT_EVENT_SYSCALL:
		RetrieveSysCallArgs( sysCallArguments, AWAIT_EVENT_ARGS, taskSP );
		returnValue = sys_await_event(
						(int)		sysCallArguments[0],
						(int)		sysCallArguments[1],
						td, GLOBALS );
		SetSysCallReturn( returnValue, taskSP );
		break;
	default:
		bwpanic( "handle_swi: Unknown syscall [ tid: %d request: %d ]", td->tid , request);
	}
}

void
RetrieveSysCallArgs( int *sysCallArguments, int numArguments, unsigned int taskSP ) {

	// The arguments are stored in the memory addresses that hold R0-R3 of the user task.
	// If there are more arguments they are stored in the user task's stack. 	
	int *ptr = ( int * ) taskSP; 
	
	int i; 
	for ( i = 0; i < numArguments && i < MAX_NUM_ARGUMENTS ; i++ )
	{
		// Copy the arguments.
		*( sysCallArguments++ ) = *( ptr++ );

		// The last register that holds arguments. 
		//The next place to look arguments is the normal user task stack.
		if ( i == 3 )
		{
			//WHITE MAGIC
			ptr += 20;
		}
	}
}

void
SetSysCallReturn( int returnValue, unsigned int taskSP ) {
	// The return value is in the address that currently holds R0 for the task. 
	int *ptr = ( int * ) taskSP; 
	*( ptr ) = returnValue; 
}

// ----------------------------------------------------------------------------
// User Task Execution
// - This part deals with executing a user task. This means that the kernel
//   gives control of the processor to the user task. 
// ----------------------------------------------------------------------------
/*
	execute_user_task: 
	This method should be executed whenever the kernel wants to return control
	to an user task. It performs the opposite task to the swi_main_handler. 
	Parameters:
	- The SP of the user task.  
	- The next instruction to execute in the user task.
	- The TID of the current ( active ) user task. 
*/
int
execute_user_task( unsigned int task_sp, unsigned int task_lr, unsigned int task_td ) {

	int ret;

	asm (										"\n"


	// // -- DEBUG ----------------------------------------------------------------
	// "STMFD	sp!, { r0, r1, ip, lr }"		"\n\t"
	// "MOV	r1, r0"								"\n\t"
	// "MOV	r0, #1"								"\n\t"
	// "BL		bwputr"							"\n\t"
	// "LDMFD	sp!, { r0, r1, ip, lr }"		"\n\t"
	// // -- DEBUG ----------------------------------------------------------------

	// Store the information about the kernel as would happen in a normal task. 
	"MOV	ip, sp"								"\n\t"
	"STMFD	sp!, { fp, ip, lr, pc }"			"\n\t"
	"SUB		fp, ip, #4"						"\n\t"
	
	// Store the kernel state.
	"SUB		sp, sp, #4"						"\n\t"

	// Store the TID
	"STR		r2, [ sp, #0 ]"					"\n\t"
	"STMFD	sp!, { r4-r11 }"					"\n\t"	// Store all the registers of the kernel ( fp, ip and lr were saved before). // This was modified

	// Store information temporarily
	//"ADD		sp, sp, #4"						"\n\t"

	"LDR		r3, [ r0, #14*4 ]"				"\n\t"	// Load the spsr of the task into r3. 
	//"SUB		sp, sp, #4"						"\n\t"	// Remove the space that had been used for the spsr
	"BIC		r3, r3, #0x80"					"\n\t"	
	"MSR		spsr, r3"						"\n\t"	// Store the cpsr of the task with interrupts enabled into the  spsr 
	"MOV	lr, r1"								"\n\t"	// Set the address of the task to return to into the lr. 
	
	// Load the state of the task. 
	"MSR		cpsr_c, #SYS_MODE"				"\n\t"	// Switch to system mode
	"MOV	sp, r0"								"\n\t"		
	"LDMFD	sp!, { r0-r12, lr }"				"\n\t"	
	"ADD		sp, sp, #4"						"\n\t"	// Remove the stored "counter"
	
	// Return to supervisor mode to re-enable HW interrupts and finally give control back to the task. 
	"MSR		cpsr_c, #SVC_MODE"				"\n\t"
	"MOVS	pc, lr"								"\n\t"
	
	// This is never executed. 
	"MOV	%0, r0" "\n\r" : "=r" (ret)
	);
	return ret;	
}



// ----------------------------------------------------------------------------
// Context Switch Initialization
// ----------------------------------------------------------------------------
void 
clean_system_state(){
	
	// Make sure that the CSPR doesn't have FIQ and IRQ enabled. 
	asm(
		// Disable both FIQ and IRQ in the CPSR
		"MRS		r0, cpsr"					"\n\t"
		"ORR		r0, r0, #0xC0"				"\n\t"	// Disables FIQ and IRQ
		"MSR		cpsr, r0"					"\n\t"
	
		// Enable caches
		"MOV		r0, #0"						"\n\t"
		"MCR		p15, 0, r0, c7, c5, 0"		"\n\t"	// Invalidate caches
		"MRC		p15, 0, r0, c1, c0, 0"		"\n\t"	// Get the current value in the co-processor
		"ORR		r0, r0, #4096"				"\n\t"	// Enable caches

		// Enable fast processor clocking
		"MOV		r1, #1"						"\n\t"	// TODO: Make sure this works fine
		"MOV		r1, r1, LSL#30"				"\n\t"
		"ORR		r0, r0, r1"					"\n\t"

		"MCR		p15, 0, r0, c1, c0, 0"		"\n\t"	// Add the changes to the processor
	);
	
	// Disable all HW interrupts. 
	*( ( int * ) INT_CONTROL_BASE_1 + INT_ENABLE_OFFSET ) = INT_RESET_VALUE; 
	*( ( int * ) INT_CONTROL_BASE_2 + INT_ENABLE_OFFSET ) = INT_RESET_VALUE; 
	
	// Disable all HW interrupts caused by SW. 
	*( ( int * ) INT_CONTROL_BASE_1 + INT_SOFT_OFFSET ) = INT_RESET_VALUE;
	*( ( int * ) INT_CONTROL_BASE_2 + INT_SOFT_OFFSET ) = INT_RESET_VALUE;
	
	// Clean the timer. 
	int *timerControl = ( int * ) ( TIMER3_BASE + CRTL_OFFSET ); 
	*timerControl = 0; 
}

void 
install_handlers(){
	// Handler for SW interrupts. 
	install_interrupt_handler( 
			( unsigned int ) main_interrupt_handler, ( unsigned int * ) SWI_ENRTY_ADDRESS );
	
	// Handler for HW interrupts. 
	install_interrupt_handler( 
			( unsigned int ) main_interrupt_handler, ( unsigned int * ) ISR_ENTRY_ADDRESS );
}

void
initialize_interrupts(){
	int *vic1EnablePointer = ( int * )( INT_CONTROL_BASE_1 + INT_ENABLE_OFFSET );
	int *vic2EnablePointer = ( int * )( INT_CONTROL_BASE_2 + INT_ENABLE_OFFSET );
	
	int initialInterruptsVIC1 = INT_RESET_VALUE;
	int initialInterruptsVIC2 = INT_RESET_VALUE;
	
	initialInterruptsVIC1 = initialInterruptsVIC1;
	*vic1EnablePointer = initialInterruptsVIC1; 
	
	// Enable here the interrupts found in VIC2 ( The last 32 ). 
	initialInterruptsVIC2 = initialInterruptsVIC2 | UART1_INT | UART2_INT | TIMER3_INT;
	*vic2EnablePointer = initialInterruptsVIC2;
}

int 
initialize_context_switching() {
	
	// Clean the state of the system to avoid possible errors caused by dirty
	// state left by other programs. 
	clean_system_state(); 
	
	// Install handlers. 
	install_handlers(); 
	
	// TODO: Remember to initialize the tasks' SPSR to enable interrupts. 
	
	// Initialize specific interrupts that will be handled. 
	initialize_interrupts(); 
	
	return 0; 
}

int install_interrupt_handler( unsigned int handlerLoc, unsigned int *vector ) {
    unsigned int vec = ( ( handlerLoc - ( unsigned int ) vector - 0x8 ) >> 2 );
    if ( vec & 0xFF000000 )
    {
		return 1; // There was a problem 
    }
    
    vec = 0xEA000000 | vec;
    *vector = vec; 

    return 0; 
}
