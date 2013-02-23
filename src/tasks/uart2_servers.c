#include <userspace.h>

void uart2_sender_notifier() {
	int *uart_flags = (int *)( UART2_BASE + UART_FLAG_OFFSET );

	int server_tid = WhoIs("uart2_sender");
	UART_request request;

	FOREVER {
		//If UART2 is not ready to receive a character
		if(!( *uart_flags & TXFF_MASK )) {
			//Notify the server
			request.type = UART2_SEND_NOTIFIER_REQUEST;
			request.ch = 0;
			Send(server_tid, (char *) &request,
					sizeof(request), (char *) 0, 0);

			//Wait until UART2 is ready
			AwaitEvent(EVENT_UART2_SEND_READY, 0, 0);
		}

		//UART2 is ready
		//Notify the server
		request.type = UART2_SEND_NOTIFIER_REQUEST;
		request.ch = 1;
		Send(server_tid, (char *) &request,
				sizeof(request), (char *) 0, 0);
	}
}

void uart2_sender_server() {
	//Register the server
	RegisterAs("uart2_sender");
	
	//Create the notifier
	Create(UART_SENDER_NOTIFIER_PRIORITY, &uart2_sender_notifier); //TODO: Priority for the notifier
	
	//Request & Reply
	UART_request request;
	UART_reply reply;
	int UART_ready = 0;
	int *uart_data = (int *)( UART2_BASE + UART_DATA_OFFSET );

	//Buffer queues
	Char_queue cqueue;
	init_char_queue( &cqueue );

	FOREVER {
		//Receive request from system function
		int sender_tid = -1;
		Receive(&sender_tid, (char *) &request, sizeof(request));

		switch(request.type){
			case UART2_SEND_REQUEST:
				//Reply to unblock the system function (Putc)
				reply.type = UART2_SEND_REPLY;
				reply.ch = 0;
				Reply(sender_tid, (char *) &reply, sizeof(reply));
				
				//Put the string from the request to queue
				enqueue_char_queue( request.ch, &cqueue );	//TODO: Reimplement to support string
				break;

			case UART2_SEND_NOTIFIER_REQUEST:
				//Reply to unblock the notifier
				reply.type = UART2_SEND_NOTIFIER_REPLY;
				reply.ch = 0;
				Reply(sender_tid, (char *) &reply, sizeof(reply));
				
				//Change the state of the UART
				UART_ready = request.ch;

				//If UART is ready - write the character
				if(UART_ready){
					*uart_data = dequeue_char_queue( &cqueue );
				}

				break;
				
			default:
				//Invalid request
				reply.type = INVALID_REQUEST;
				reply.ch = 0;
				Reply(sender_tid, (char *) &reply, sizeof(reply));
				break;
		}
	}
}



void uart2_receiver_notifier() {
	int server_tid = WhoIs("uart2_receiver");
	UART_request request;

	char receive_buffer[1];

	FOREVER {
		//Wait until there is data in UART2

		//AwaitEvent(EVENT_UART2_RECEIVE_READY, receive_buffer, 1);
		todo_debug( 40, 0 );
		AwaitEvent( UART2_RECEIVE_READY, receive_buffer, 1 );
		todo_debug( 41, 0 );
		
		//Configure the request
		request.type = UART2_RECEIVE_NOTIFIER_REQUEST;
		request.ch = receive_buffer[0];
		
		//Send data to the server (uart2_receiver_server)
		Send(server_tid, (char *) &request, sizeof(request), (char *) 0, 0);
	}
}

void uart2_receiver_server() {
	//Current TID
	//int server_tid = MyTid();
	
	//Register the server
	int ret = RegisterAs("uart2_receiver");
	todo_debug( ret + 1, 1 );
	
	//Create the receive notifier
	//int notifier_tid = 0;
	Create(UART_RECEIVER_NOTIFIER_PRIORITY, &uart2_receiver_notifier); //TODO: Set priority for the notifier
	
	//Request & Reply
	UART_request request;
	UART_reply reply;
	
	//Buffers
	Char_queue cqueue;
	init_char_queue( &cqueue );
	Int_queue iqueue;
	init_int_queue( &iqueue );
	
	FOREVER {
		//Utility variables
		int sender_tid = -1;
		int target_tid = -1;
		
		todo_debug( 30, 0 );
		//Receive request from:
		//	system function (Getc)
		//	notifier (uart2_receiver_notifier)
		Receive(&sender_tid, (char *) &request, sizeof(request));

		switch(request.type){
			case UART2_RECEIVE_REQUEST:
				todo_debug( 31, 0 );
				//Enqueue the system function tid to reply later
				enqueue_int_queue( sender_tid, &iqueue );
				
				break;
			
			case UART2_RECEIVE_NOTIFIER_REQUEST:
				todo_debug( 32, 0 );
				//Reply to unblock the notifier
				Reply(sender_tid, (char *) 0, 0);
			
				//Enqueue received character
				enqueue_char_queue( request.ch, &cqueue );
			
				break;
			
			default:
				todo_debug( 33, 0 );
				reply.type = INVALID_REQUEST;
				reply.ch = 0;
				Reply(sender_tid, (char *) &reply, sizeof(reply));
				
				break;
		}

		//If there are system functions to receive
		//and characters to be sent
		while(cqueue.size > 0 && iqueue.size > 0){
			todo_debug( 34, 0 );
			//Prepare the reply to the system function
			target_tid = dequeue_int_queue( &iqueue );
			reply.type = UART1_RECEIVE_REPLY;
			reply.ch = dequeue_char_queue( &cqueue );

			//Perform the reply
			todo_debug( 35, 0 );
			//todo_debug( reply.ch, 1 );
			Reply(target_tid, (char *) &reply, sizeof(reply));
		}
	}
}









