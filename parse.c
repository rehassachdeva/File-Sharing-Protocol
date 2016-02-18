/********************************** For parsing commands ***************************/

#include "headers.h"

char* read_cmd() {
        int len=0,c;            
        char* cmd = malloc(sizeof(char)*MAX_BUF_LEN);
        while(1) {
                c = getchar();
                if(c == '\n') {
                        cmd[len++] = '\0';
                        break;
                }
                else
                        cmd[len++] = c;
        }
        return cmd;
}

int parse_cmd(char* cmd, char** cmd_tokens) {
        int tok = 0;
        char* token = strtok(cmd, CMD_DELIMS);
        while(token!=NULL) {
                cmd_tokens[tok++] = token;
                token = strtok(NULL, CMD_DELIMS);
        }
        return tok;
}
