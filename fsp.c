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
#include <openssl/md5.h>
#include <openssl/hmac.h>
#include <netdb.h>

#define MAXBUF 10240 
#define MAXLEN 100
#define MAXLINE 1024
#define CMD_DELIMS " \t\n"

char cmdHistory[MAXLINE][MAXLEN], res[MAXBUF];
int numCmds, fDataIndex, cDataIndex, dDataIndex;
int TCP_REMOTE_PORT, TCP_LOCAL_PORT, UDP_REMOTE_PORT, UDP_LOCAL_PORT;

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

struct downloadData {
	char filename[MAXLEN];
	off_t size;
	time_t mtime;
	unsigned char checksum[MD5_DIGEST_LENGTH];
} dData[MAXBUF];

int serverTCP();
int serverUDP();
void longlist();
int shortlist(time_t sTime, time_t eTime);
int regex(char *expr) ;
int indexGetCommand(char** cmd_tokens, int tok);
int fileHashCommand(char** cmd_tokens, int tok);
int parse_cmd(char* cmd, char** cmd_tokens);
int check_cmd_type(char** cmd_tokens, int tok);
void error(const char *msg);
int client(char *remoteIP);
void checkall();
void verify(char *filename);
int fileHashCommand(char** cmd_tokens, int tok);
void getDownloadData(char *filename) ;

int main(int argc, char *argv[])
{
	if (argc != 6) {
		fprintf(stderr, "Usage: %s <remoteIP address> <tcp remote port> <tcp local port> <udp remote port> <udp local port>\n", argv[0]);
		exit(1);
	}
	TCP_REMOTE_PORT = atoi(argv[2]);  
	TCP_LOCAL_PORT = atoi(argv[3]);  
	UDP_REMOTE_PORT = atoi(argv[4]);  
	UDP_LOCAL_PORT = atoi(argv[5]);  
	pid_t pid = fork();
	if(pid < 0) error("Fork error\n");
	else if(pid == 0) {
		pid_t pd = fork();
		if(pd < 0) error("Fork error\n");
		if(pd == 0) 
			serverTCP();
		else 
			serverUDP();
	}
	else if(pid > 0) client(argv[1]);
	return 0;
}

void error(const char *msg)
{
	perror(msg);
	exit(1);
}

int parse_cmd(char* cmd, char** cmd_tokens) 
{
	int tok = 0;
	char* token = strtok(cmd, CMD_DELIMS);
	while(token!=NULL) {
		cmd_tokens[tok++] = token;
		token = strtok(NULL, CMD_DELIMS);
	}
	return tok;
}

int check_cmd_type(char** cmd_tokens, int tok) {
	if(strcmp(cmd_tokens[0], "IndexGet") == 0) return 1;
	else if(strcmp(cmd_tokens[0], "FileHash") == 0) return 2;
	else if(strcmp(cmd_tokens[0], "FileDownload") == 0) return 3;
	else if(strcmp(cmd_tokens[0], "History") == 0) {
		int k;
		for(k = 0; k<numCmds; k++) printf("%s\n", cmdHistory[k]);
		printf("==============================================================================================================================================\n");
		return 4;
	}
	else {
		printf("No such command\n");
		return -1;
	}
}

int serverTCP()
{
	int typeCommand, localSockfd, remoteSockfd;
	char readBuffer[MAXBUF];
	char writeBuffer[MAXBUF];
	char command[MAXLEN];

	struct sockaddr_in serv_addr;
	int n;
	localSockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (localSockfd < 0) error("ERROR opening socket");
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(TCP_LOCAL_PORT);

	if (bind(localSockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) error("ERROR on binding");
	listen(localSockfd, 5);
	remoteSockfd = accept(localSockfd, (struct sockaddr *)NULL, NULL);
	if (remoteSockfd < 0) error("ERROR on accept");

	while(1) {
		bzero(readBuffer, sizeof(readBuffer));
		bzero(writeBuffer, sizeof(writeBuffer));
		bzero(res, sizeof(res));

		n = read(remoteSockfd, readBuffer, sizeof(readBuffer));

		if(n > 0) {
			char** cmd_tokens = malloc((sizeof(char)*MAXBUF)*MAXBUF);
			printf("Command Received: %s$> ", readBuffer);

			fflush(stdout);

			int tok = parse_cmd(readBuffer, cmd_tokens);
			typeCommand = check_cmd_type(cmd_tokens, tok);
			if(typeCommand == 1)
			{
				indexGetCommand(cmd_tokens, tok);
				strcat(writeBuffer, res);
				int j;
				for(j=0; j<fDataIndex; j++)
				{
					sprintf(res, "%-20s  %-10d  %-10c %-20s\n", fData[j].filename, fData[j].size, fData[j].type, ctime(&fData[j].mtime));
					strcat(writeBuffer,res);
				}
				strcat(writeBuffer, "==============================================================================================================================================");
				write(remoteSockfd, writeBuffer, strlen(writeBuffer));
			}
			else if(typeCommand == 2) 
			{
				fileHashCommand(cmd_tokens, tok);
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
				strcat(writeBuffer, "==============================================================================================================================================");
				write(remoteSockfd, writeBuffer, strlen(writeBuffer));
			}
			else if(typeCommand == 3) {
				FILE* file;
				file = fopen(cmd_tokens[2], "rb");
				size_t bytesRead;
				while(!feof(file))
				{
					bytesRead = fread(res, 1, MAXLINE, file);
					memset(writeBuffer, 0, sizeof(writeBuffer));
					memcpy(writeBuffer, res, bytesRead);
					write(remoteSockfd, writeBuffer, bytesRead);
					memset(writeBuffer, 0, sizeof(writeBuffer));
					memset(res, 0, sizeof(res));
				}
				memcpy(writeBuffer,"END",3);
				write(remoteSockfd , writeBuffer , 3);
				memset(writeBuffer, 0, sizeof(writeBuffer));
				fclose(file);
				getDownloadData(cmd_tokens[2]);
				int k;
				strcat(writeBuffer, res);
				int j;
				for(j=0; j<dDataIndex; j++)
				{
					sprintf(res, "%-20s  %-10d", dData[j].filename, dData[j].size);
					strcat(writeBuffer,res);
					for(k = 0; k<MD5_DIGEST_LENGTH; k++) {
						sprintf(res, "%02x", dData[j].checksum[k]);
						strcat(writeBuffer,res);
					}
					sprintf(res, "           %-20s", ctime(&dData[j].mtime));
					strcat(writeBuffer,res);
				}
				strcat(writeBuffer, "==============================================================================================================================================");
				write(remoteSockfd, writeBuffer, strlen(writeBuffer));
			} 
		} 
	}
	close(remoteSockfd);
	close(localSockfd);
	wait(NULL);
}

int serverUDP()
{
	int typeCommand, localSockfd, remoteSockfd;
	char readBuffer[MAXBUF];
	char writeBuffer[MAXBUF];
	char command[MAXLEN];
	int clientlen;

	struct sockaddr_in serv_addr, client_addr;
	int n;
	localSockfd = socket(AF_INET, SOCK_DGRAM, 0); 
	if (localSockfd < 0) error("ERROR opening socket");
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(UDP_LOCAL_PORT);

	if (bind(localSockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) error("ERROR on binding");

	clientlen = sizeof(client_addr);
	while(1) {
		bzero(readBuffer, sizeof(readBuffer));
		bzero(writeBuffer, sizeof(writeBuffer));
		bzero(res, sizeof(res));

		n = recvfrom(localSockfd, readBuffer, sizeof(readBuffer), 0, (struct sockaddr *) &client_addr, &clientlen);

		if(n > 0) {
			char** cmd_tokens = malloc((sizeof(char)*MAXBUF)*MAXBUF);
			printf("Command Received: %s$> ", readBuffer);

			fflush(stdout);

			int tok = parse_cmd(readBuffer, cmd_tokens);
			typeCommand = check_cmd_type(cmd_tokens, tok);
			if(typeCommand == 1)
			{
				indexGetCommand(cmd_tokens, tok);
				strcat(writeBuffer, res);
				int j;
				for(j=0; j<fDataIndex; j++)
				{
					sprintf(res, "%-20s  %-10d  %-10c %-20s\n", fData[j].filename, fData[j].size, fData[j].type, ctime(&fData[j].mtime));
					strcat(writeBuffer,res);
				}
				strcat(writeBuffer, "==============================================================================================================================================");
				write(localSockfd, writeBuffer, strlen(writeBuffer));
			}
			else if(typeCommand == 2) 
			{
				fileHashCommand(cmd_tokens, tok);
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
				strcat(writeBuffer, "==============================================================================================================================================");
				write(localSockfd, writeBuffer, strlen(writeBuffer));
			}
			else if(typeCommand == 3) {
				FILE* file;
				file = fopen(cmd_tokens[2], "rb");
				size_t bytes_read;
				while(!feof(file))
				{
					bytes_read = fread(res, 1, 1024, file);
					memset(writeBuffer, 0, sizeof(writeBuffer));
					memcpy(writeBuffer,res,bytes_read);
					sendto(localSockfd , writeBuffer , bytes_read, 0, (struct sockaddr *) &client_addr, clientlen);
					memset(writeBuffer, 0, sizeof(writeBuffer));
					memset(res, 0, sizeof(res));
				}
				memcpy(writeBuffer,"END",3);
				sendto(localSockfd , writeBuffer , 3, 0, (struct sockaddr *) &client_addr, clientlen);
				memset(writeBuffer, 0, sizeof(writeBuffer));
				fclose(file);
				getDownloadData(cmd_tokens[2]);
				int k;
				strcat(writeBuffer, res);
				int j;
				for(j=0; j<dDataIndex; j++)
				{
					sprintf(res, "%-20s  %-10d", dData[j].filename, dData[j].size);
					strcat(writeBuffer,res);
					for(k = 0; k<MD5_DIGEST_LENGTH; k++) {
						sprintf(res, "%02x", dData[j].checksum[k]);
						strcat(writeBuffer, res);
					}
					sprintf(res, "           %-20s", ctime(&dData[j].mtime));

					strcat(writeBuffer, res);

				}
				strcat(writeBuffer, "==============================================================================================================================================");
				sendto(localSockfd, writeBuffer, strlen(writeBuffer), 0, (struct sockaddr *) &client_addr, clientlen);
			} 
		} 
	}
	close(localSockfd);
	wait(NULL);
}

int client(char *remoteIP) {
	FILE* file;
	int st, n, getDownloadFlag = 0;
	int serverlen;
	int typeCommand;
	int localSockfd, localSockfdUdp;
	char readBuffer[MAXBUF];
	char writeBuffer[MAXBUF];
	char command[MAXBUF];
	char downloadFilename[MAXLEN];

	struct sockaddr_in serv_addr, serv_addr_udp;
	localSockfd = socket(AF_INET, SOCK_STREAM, 0);
	localSockfdUdp = socket(AF_INET, SOCK_DGRAM, 0); 
	if (localSockfd < 0) error("ERROR opening socket");
	if (localSockfdUdp < 0) error("ERROR opening socket");
	bzero((char *) &serv_addr, sizeof(serv_addr));
	bzero((char *) &serv_addr_udp, sizeof(serv_addr_udp));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(TCP_REMOTE_PORT);
	serv_addr_udp.sin_family = AF_INET;
	serv_addr_udp.sin_port = htons(UDP_REMOTE_PORT);
	if ((st = inet_pton(AF_INET, remoteIP, &serv_addr.sin_addr)) <= 0) error("ERROR with inet_pton");
	if ((st = inet_pton(AF_INET, remoteIP, &serv_addr_udp.sin_addr)) <= 0) error("ERROR with inet_pton");
	while(1) {
		if (connect(localSockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
			continue;
		else {
			printf("\nConnected!\n\n");
			break;
		}
	}
	serverlen = sizeof(serv_addr_udp);
	while(1) {
		memset(readBuffer, 0, sizeof(readBuffer));
		bzero(writeBuffer, sizeof(writeBuffer)); 
		bzero(command, sizeof(command)); 
		bzero(downloadFilename, sizeof(command)); 
		if(getDownloadFlag == 1) {
			n = read(localSockfd, readBuffer, sizeof(readBuffer) - 1);
			if (n < 0) error("ERROR reading from socket");
			printf("%s\n", readBuffer);
			getDownloadFlag = 0;

		}
		else if(getDownloadFlag == 2) {
			n = recvfrom(localSockfdUdp, readBuffer, sizeof(readBuffer) - 1, 0, &serv_addr_udp, &serverlen );
			if (n < 0) error("ERROR reading from socket");
			printf("%s\n", readBuffer);
			getDownloadFlag = 0;

		}

		printf("$> ");
		fgets(writeBuffer, sizeof(writeBuffer), stdin);
		if(strlen(writeBuffer) < 2) continue;
		strcpy(cmdHistory[numCmds++], writeBuffer);
		strcpy(command, writeBuffer);
		char** cmd_tokens = malloc((sizeof(char)*MAXBUF)*MAXBUF);
		int tok = parse_cmd(command, cmd_tokens);
		typeCommand = check_cmd_type(cmd_tokens, tok);
		if(typeCommand == 1 || typeCommand == 2)
		{
			n = write(localSockfd, writeBuffer, strlen(writeBuffer));

			if (n < 0) error("ERROR writing to socket");


			n = read(localSockfd, readBuffer, sizeof(readBuffer) - 1);
			if (n < 0) error("ERROR reading from socket");
			printf("%s\n", readBuffer);
		}
		else if(typeCommand == 3) {
			if(tok != 3 || !(strcmp(cmd_tokens[1], "TCP") == 0 || strcmp(cmd_tokens[1], "UDP") == 0)) {
				fprintf(stderr, "Usage: FileDownload <protocol> <filename>\n");
				continue;
			}
			if(strcmp(remoteIP, "127.0.0.1") == 0) strcat(downloadFilename, "downloaded_");
			strcat(downloadFilename, cmd_tokens[2]);
			file = fopen(downloadFilename, "wb");
			if(strcmp(cmd_tokens[1], "TCP") == 0) 
				n = write(localSockfd, writeBuffer, strlen(writeBuffer));
			else
				n = sendto(localSockfdUdp, writeBuffer, strlen(writeBuffer), 0, &serv_addr_udp, serverlen);
			if (n < 0) error("ERROR writing to socket");
			while(1) {
				memset(readBuffer, 0, strlen(readBuffer));
				if(strcmp(cmd_tokens[1], "TCP") == 0) 
					n = read(localSockfd, readBuffer, sizeof(readBuffer) - 1);
				else
					n = recvfrom(localSockfdUdp, readBuffer, sizeof(readBuffer) - 1, 0, &serv_addr_udp, &serverlen);
				if(n < 0) {
					fprintf(stderr, "ERROR reading from socket");

					break;
				}
				readBuffer[n] = 0;
				if(readBuffer[n-3] == 'E' && readBuffer[n-2] == 'N' && readBuffer[n-1] == 'D')
				{
					readBuffer[n-3] = 0;
					fwrite(readBuffer, 1 , n-3, file);
					fclose(file);
					printf("File successfully downloaded!\n");
					if(strcmp(cmd_tokens[1], "TCP") == 0) getDownloadFlag = 1;
					else getDownloadFlag = 2;
					break;
				}
				else fwrite(readBuffer, 1, n, file);
			}

		}
	}
	close(localSockfd);
	close(localSockfdUdp);
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
			sprintf(res, "Usage: IndexGet ​shortlist <starttimestamp> <end​timestamp>\n TimeStamp format is date-month-year-hrs:minutes:seconds\n");
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
	else {
		sprintf(res, "No such command\n");
		return;

	}
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
			sprintf(res, "No such file or directory\n");
		}
		closedir (remoteDir);
	}
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
	else {
		sprintf(res, "Usage: IndexGet ​regex <regular expression>\n");
		return;
	}
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
	if(cDataIndex == 0) {
		sprintf(res, "No such file or directory\n");
	}
	closedir (remoteDir);
}

void getDownloadData(char *filename) 
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
			else if(strcmp(filename, fptr->d_name) != 0) continue;
			else
			{
				strcpy(dData[i].filename, fptr->d_name);
				dData[i].size = fileStatus.st_size;
				dData[i].mtime = fileStatus.st_mtime;
				file = fopen(fptr->d_name, "rb");
				if(!file) {
					sprintf(res, "ERROR opening file %s\n", fptr->d_name);
					break;
				}
				if(!MD5_Init (&c)) {
					sprintf(res, "ERROR with MD5\n");
					break;
				}
				while( (len = fread(readBuffer, 1, MAXLINE, file)) != 0 ) {
					MD5_Update(&c, readBuffer, len);
				}
				MD5_Final(dData[i].checksum, &c);
				fclose(file);
				i++;
			}
		}
		dDataIndex = i;
		if(dDataIndex == 0) {
			sprintf(res, "No such file or directory\n");
		}
		closedir (remoteDir);
	}
}
