/*****************************Header Files & Macros*****************************************/

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define MAX_BUF_LEN 1024
#define CMD_DELIMS " \t\n"
#define FNAME_LEN 20
#define PORT 5555

/**********************************Function Declarations************************************/

char* read_cmdl();
int parse_cmd(char* cmd, char** cmd_tokens);
void set_prompt();
