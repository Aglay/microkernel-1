#ifndef __TEST_H__
#define __TEST_H__ 

// -----------------------------------------------------------------------------------------------------------------------------------------------
// Stress Tests
// -----------------------------------------------------------------------------------------------------------------------------------------------
void stress_test_uart1_getc();

void stress_test_uart2_getc(); 

void stress_test_uart2_putc();

// -----------------------------------------------------------------------------------------------------------------------------------------------
// Simple Tests
// -----------------------------------------------------------------------------------------------------------------------------------------------
void test_debug(); 

void test_syscalls();

void test_nameserver();

void test_timeserver();

void test_user_dashboard(); 

void test_uart1_send();

void test_uart1_receive();

void task_test_uart2();

void test_sensors_server(); 

void test_track_display();

#endif
