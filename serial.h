#ifndef SERIAL_H
#define SERIAL_H

#include "config.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// Buffer
extern char rx_buffer[RX_BUFFER_SIZE];
extern int rx_index;

// Functions
void usart_setup(void);
void process_command(char *line);

// Standard IO support
int _write(int file, char *ptr, int len);

#endif // SERIAL_H
