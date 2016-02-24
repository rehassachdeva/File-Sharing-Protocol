#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <openssl/md5.h>
#include <openssl/hmac.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/wait.h>

#define MAXBUF 10240 
#define MAXLEN 100
#define MAXLINE 1024
#define CMD_DELIMS " \t\n"
#define TCP_REMOTE_PORT 5000
#define TCP_LOCAL_PORT 6000
#define UDP_REMOTE_PORT 7000
#define UDP_LOCAL_PORT 8000

char cmdHistory[MAXBUF][MAXLEN];
int  numCmds = 0;
char res[MAXBUF];
int fDataIndex, cDataIndex;

int server(int port);
void longlist();
int indexGetCommand(char** cmd_tokens, int tok);
int fileHashCommand(char** cmd_tokens, int tok);
int parse_cmd(char* cmd, char** cmd_tokens);
int check_cmd_type(char** cmd_tokens, int tok);
void error(const char *msg);
int client(char *remoteIP, int port);

struct fileData {
  char filename[MAXLEN];
  off_t size;
  time_t mtime;
  char type;
} fData[MAXBUF];

struct checksumData {
  char filename[MAXLEN];
  time_t mtime;
  unsigned char checksum[MD5_DIGEST_LENGTH];
} cData[MAXBUF];

int main(int argc, char *argv[])
{
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <remoteIP address>\n", argv[0]);
    exit(1);
  }

  int localPort = localPORT;
  int remotePort = remotePORT;
  char *remoteIP = argv[3];

  pid_t pid = fork();

  if(pid < 0) error("Fork error\n");
  else if(pid == 0) server(localPort);
  else if(pid > 0) client(remoteIP, remotePort);

  return 0;
}

void longlist() 
{
  int i = 0; 
  DIR *remoteDir;
  struct dirent *fptr;
  struct stat fileStatus;

  remoteDir = opendir ("./");

  if( remoteDir == NULL) sprintf(res, "ERROR opening the directory\n");
  else
  {
    while (fptr = readdir (remoteDir))
    {
      if(stat(fptr->d_name, &fileStatus) < 0) {
        sprintf(res, "ERROR stat unsuccessful\n");
        break;
      }
      else
      {
        strcpy(fData[i].filename, fptr->d_name);
        fData[i].size = fileStatus.st_size;
        fData[i].mtime = fileStatus.st_mtime;
        fData[i].type = (S_ISDIR(fileStatus.st_mode)) ? 'd' : '-';
        i++;
      }
    }
    fDataIndex = i;
    closedir (remoteDir);
  }
}

int shortlist(time_t sTime, time_t eTime)
{
  int i = 0;
  DIR *remoteDir;
  struct dirent *fptr;
  struct stat fileStatus;

  remoteDir = opendir ("./");

  if( remoteDir == NULL) sprintf(res, "ERROR opening the directory\n");
  else {
    while (fptr = readdir (remoteDir))
    {
      if(stat(fptr->d_name, &fileStatus) < 0) {
        sprintf(res, "ERROR stat unsuccessful\n");
        break;
      }
      else if(difftime(fileStatus.st_mtime, sTime) >= 0 && difftime(fileStatus.st_mtime, eTime) <= 0)
      {
        strcpy(fData[i].filename, fptr->d_name);
        fData[i].size = fileStatus.st_size;
        fData[i].mtime = fileStatus.st_mtime;
        fData[i].type = (S_ISDIR(fileStatus.st_mode)) ? 'd' : '-';
        i++;
      }
    }
    fDataIndex=i;
    closedir (remoteDir);
  }
}

int regex(char *expr) 
{

  FILE *pipe;
  char command[MAXLEN] = "ls ";

  char line[1024];
  char readBuffer[MAXBUF];

  strcat(command, expr);
  strcat(command, " 2>/dev/null");

  int i = 0;
  DIR *remoteDir;
  struct dirent *fptr;
  struct stat fileStatus;

  remoteDir = opendir ("./");

  if( remoteDir == NULL) sprintf(res, "ERROR opening the directory\n");
  else {
    while (fptr = readdir (remoteDir))
    {
      if(stat(fptr->d_name, &fileStatus) < 0) {
        sprintf(res, "ERROR stat unsuccessful\n");
        break;
      }
      else if(( pipe = popen(command, "r")) == NULL) {
        sprintf(res, "ERROR with popen\n");
        break;
      }
      else {
        while(fgets(readBuffer, MAXLINE, pipe))
        {
          if(strncmp(readBuffer, fptr->d_name, strlen(readBuffer) - 1) == 0) 
          {
            strcpy(fData[i].filename, fptr->d_name);
            fData[i].size = fileStatus.st_size;
            fData[i].mtime = fileStatus.st_mtime;
            fData[i].type = (S_ISDIR(fileStatus.st_mode)) ? 'd' : '-';
            i++;
          }
        }
      }
    }
    fDataIndex=i;
    if(fDataIndex == 0) {
      sprintf(res, "ERROR: No such file or directory\n");
    }
    closedir (remoteDir);
  }
}

int indexGetCommand(char** cmd_tokens, int tok) {
  struct tm timeStamp;
  time_t sTime , eTime;


  if(strcmp(cmd_tokens[1], "longlist") == 0) {
    if(tok != 2)
      sprintf(res, "Usage: IndexGet longlist\n");

    else {
      longlist();
    }
  }
  else if(strcmp(cmd_tokens[1], "shortlist") == 0) {

    if(tok != 4) {
      sprintf(res, "Usage: IndexGet ​shortlist <starttimestamp> <end​timestamp>\n");
      return;
    }
    if(strptime(cmd_tokens[2], "%d-%m-%Y-%H:%M:%S", &timeStamp) == NULL) {
      sprintf(res, "Incorrect Format for Timestamp. The correct format is: Date-Month-Year-hrs:min:sec\n");
      return;
    }
    else 
      sTime = mktime(&timeStamp);
    if(strptime(cmd_tokens[3], "%d-%m-%Y-%H:%M:%S", &timeStamp) == NULL) {
      sprintf(res, "Incorrect Format for Timestamp. The correct format is: Date-Month-Year-hrs:min:sec\n");
      return;
    }
    else
      eTime = mktime(&timeStamp);
    shortlist(sTime, eTime);

  }
  else if(strcmp(cmd_tokens[1], "regex") == 0) {
    if(tok != 3) {
      sprintf(res, "Usage: IndexGet ​regex <regular expression>\n");
      return;
    }
    regex(cmd_tokens[2]);
  }
  /*else if(strcmp(cmd_tokens[1], "history") == 0) {
    if(tok != 2)
    fprintf(stderr, "Usage: IndexGet history\n");
    else {
    history();
    }
    } */
}

void checkall() 
{
  int i = 0; 
  DIR *remoteDir;
  FILE *file;
  struct dirent *fptr;
  struct stat fileStatus;
  char readBuffer[MAXBUF];
  MD5_CTX c;
  unsigned long len;
  remoteDir = opendir ("./");
  if( remoteDir == NULL) sprintf(res, "ERROR opening the directory\n");
  else
  {
    while (fptr = readdir (remoteDir))
    {
      if(stat(fptr->d_name, &fileStatus) < 0) {
        sprintf(res, "ERROR stat unsuccessful\n");
        break;
      }
      else
      {
        file = fopen(fptr->d_name, "rb");
        if(!file) {
          sprintf(res, "ERROR opening file %s\n", fptr->d_name);
          break;
        }

        strcpy(cData[i].filename, fptr->d_name);
        cData[i].mtime = fileStatus.st_mtime;
        if(!MD5_Init (&c)) {
          sprintf(res, "ERROR with MD5\n");
          break;
        }
        while( (len = fread(readBuffer, 1, MAXLINE, file)) != 0 ) {
                  MD5_Update(&c, readBuffer, len);
        }
        MD5_Final(cData[i].checksum, &c);
        fclose(file);
        i++;
      }
    }
  }
  cDataIndex = i;
  closedir (remoteDir);
}  

void verify(char *filename) 
{
  printf("hello");
    int i = 0; 
  DIR *remoteDir;
  FILE *file;
  struct dirent *fptr;
  struct stat fileStatus;
  char readBuffer[MAXBUF];
  MD5_CTX c;
  unsigned long len;
  remoteDir = opendir ("./");
  if( remoteDir == NULL) sprintf(res, "ERROR opening the directory\n");
  else
  {
    while (fptr = readdir (remoteDir))
    {
      if(stat(fptr->d_name, &fileStatus) < 0) {
        sprintf(res, "ERROR stat unsuccessful\n");
        break;
      }
        else if(strcmp(filename, fptr->d_name) != 0) continue;
      else
      {
        file = fopen(fptr->d_name, "rb");
        if(!file) {
          sprintf(res, "ERROR opening file %s\n", fptr->d_name);
          break;
        }

        strcpy(cData[i].filename, fptr->d_name);
        cData[i].mtime = fileStatus.st_mtime;
        if(!MD5_Init (&c)) {
          sprintf(res, "ERROR with MD5\n");
          break;
        }
        while( (len = fread(readBuffer, 1, MAXLINE, file)) != 0 ) {
                  MD5_Update(&c, readBuffer, len);
        }
        MD5_Final(cData[i].checksum, &c);
        fclose(file);
        i++;
      }
    }
  }
  cDataIndex = i;
  closedir (remoteDir);
 
}

int fileHashCommand(char** cmd_tokens, int tok) {

  if(strcmp(cmd_tokens[1], "checkall") == 0) {
    if(tok != 2)
      sprintf(res, "Usage: FileHash checkall\n");
    else {
      checkall();
    }
  }
  else if(strcmp(cmd_tokens[1], "verify") == 0) {

    if(tok != 3) {
      sprintf(res, "Usage: FileHash ​verify <filename>\n");
    }
    else verify(cmd_tokens[2]);
  }
}
void tcpDownload() {

}

void udpDownload() {

}

int fileDownloadCommand(char** cmd_tokens, int tok, int localSockfd) {

  if(strcmp(cmd_tokens[1], "TCP") == 0) {
    if(tok != 3)
      sprintf(res, "Usage: FileDownload TCP <filename>\n");
    else {
      tcpDownload(cmd_tokens[2]);
    }
  }
  else if(strcmp(cmd_tokens[1], "UDP") == 0) {

    if(tok != 3) {
      sprintf(res, "Usage: FileDownload UDP <filename>\n");
    }
    else udpDownload(cmd_tokens[2]);
  }
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

int check_cmd_type(char** cmd_tokens, int tok, int localSockfd) {
  if(strcmp(cmd_tokens[0], "IndexGet") == 0) {
    indexGetCommand(cmd_tokens, tok);
    return 1;
  }
  else if(strcmp(cmd_tokens[0], "FileHash") == 0) {
    fileHashCommand(cmd_tokens, tok);
    return 2;
  }
  else if(strcmp(cmd_tokens[0], "FileDownload") == 0) {
    fileDownloadCommand(cmd_tokens, tok, localSockfd);
    return 3;
  }
}

void error(const char *msg)
{
  perror(msg);
  exit(1);
}

int server(int port)
{
  int typeCommand, localSockfd, remoteSockfd;

  char readBuffer[MAXBUF];
  char writeBuffer[MAXBUF];

  struct sockaddr_in serv_addr;

  int n;

  localSockfd = socket(AF_INET, SOCK_STREAM, 0); 

  if (localSockfd < 0) error("ERROR opening socket");

  bzero((char *) &serv_addr, sizeof(serv_addr));
  bzero(readBuffer, sizeof(readBuffer));
  bzero(writeBuffer, sizeof(writeBuffer)); 

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(port);

  if (bind(localSockfd, (struct sockaddr *) &serv_addr,
        sizeof(serv_addr)) < 0)  
    error("ERROR on binding");

  listen(localSockfd, 5);

  remoteSockfd = accept(localSockfd, 
      (struct sockaddr *)NULL, NULL); 

  if (remoteSockfd < 0)  
    error("ERROR on accept");
  while(1) {
    bzero(readBuffer, sizeof(readBuffer));
    bzero(writeBuffer, sizeof(writeBuffer));

    n = read(remoteSockfd, readBuffer, sizeof(readBuffer));
    if(n > 0) {
      //printf("%s\n", readBuffer);
      char** cmd_tokens = malloc((sizeof(char)*MAXBUF)*MAXBUF);
      int tok = parse_cmd(readBuffer,cmd_tokens);
      printf("Command Received: %s %s\n", cmd_tokens[0], cmd_tokens[1]);

      typeCommand = check_cmd_type(cmd_tokens, tok);
        memset(writeBuffer, 0, sizeof(writeBuffer));

      if(typeCommand == 1)
      {
        strcat(writeBuffer, res);
        int j;
        for(j=0; j<fDataIndex; j++)
        {
          sprintf(res, "%-20s  %-10d  %-10c %-20s\n", fData[j].filename, fData[j].size, fData[j].type, ctime(&fData[j].mtime));
          strcat(writeBuffer,res);
        }
        strcat(writeBuffer, "~@~");
        write(remoteSockfd, writeBuffer, strlen(writeBuffer));
      }

      else if(typeCommand == 2) 
      {

            int k;

        strcat(writeBuffer, res);
        int j;
        for(j=0; j<cDataIndex; j++)
        {
        

          sprintf(res, "%-20s  ", cData[j].filename);
          strcat(writeBuffer,res);
        for(k = 0; k<MD5_DIGEST_LENGTH; k++) {
          sprintf(res, "%02x", cData[j].checksum[k]);
          strcat(writeBuffer,res);
        }
        sprintf(res, "          %-30s\n", ctime(&cData[j].mtime));
          strcat(writeBuffer,res);



        }
        strcat(writeBuffer, "~@~");
        write(remoteSockfd, writeBuffer, strlen(writeBuffer));
      }
      else if(typeCommand == 3) {

            FILE* file;
            file = fopen(cmd_tokens[2],"rb");
            size_t bytes_read;
            while(!feof(fp))
            {
                bytes_read = fread(res, 1, 1024, fp);
                memcpy(writeBuffer,res,bytes_read);
                write(remoteSockfd , writeBuffer , bytes_read);
                memset(writeBuffer, 0, sizeof(writeBuffer));
                memset(res, 0, sizeof(res));
            }
            memcpy(writeBuff,"~@~",3);
            write(remoteSockfd , writeBuffer , 3);
            memset(writeBuffer, 0, sizeof(writeBuffer));
            fclose(file);

      }
      /* char *command = malloc(MAXBUF);
         strcpy(command, readBuffer);
         strcpy(cmdHistory[numCmds++], command);

         typeCommand = parse(command);*/
    }
  }

  //if (n < 0) error("ERROR reading from socket");

  //printf("Here is the message: %s\n",buffer);

  //n = write(newsockfd,"I got your message",18);

  //if (n < 0) error("ERROR writing to socket");

  close(remoteSockfd);
  close(localSockfd);
  wait(NULL);
}

int client(char *remoteIP, int port) {
  int st;
  int typeCommand;
  int localSockfd;

  char readBuffer[MAXBUF];
  char writeBuffer[MAXBUF];
  char downloadFile[MAXBUF];
  char uploadFile[MAXBUF];

  struct sockaddr_in serv_addr;

  int n;

  localSockfd = socket(AF_INET, SOCK_STREAM, 0); 

  if (localSockfd < 0) error("ERROR opening socket");

  bzero((char *) &serv_addr, sizeof(serv_addr));
  bzero(readBuffer, sizeof(readBuffer));
  bzero(writeBuffer, sizeof(writeBuffer)); 

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);


  if ((st = inet_pton(AF_INET, remoteIP, &serv_addr.sin_addr)) <= 0) {
    error("ERROR with inet_pton");
  }

  while(1) {
    if (connect(localSockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
      continue;
    else break;
  }

  while(1) {
    printf("$> ");
    fgets(writeBuffer,sizeof(writeBuffer),stdin);
    n = write(localSockfd, writeBuffer, strlen(writeBuffer));
    if (n < 0) 
      error("ERROR reading from socket");

    n = read(localSockfd, readBuffer, sizeof(readBuffer) - 1);
    printf("%s\n",readBuffer);

    //char* cmd = read_cmd();
    //char** cmd_tokens = malloc((sizeof(char)*MAXBUF)*MAXBUF);
    //int tokens = parse_cmd(strdup(cmd), cmd_tokens);
    //int typeCommand = check_cmd_type(cmd_tokens, tokens);

  }
  close(localSockfd);
}

