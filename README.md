### FILE SHARING PROTOCOL 
Simple Application Level File Sharing Protocol with support for **download** of files and **indexed searching.** 

### TEAM MEMBERS:
Rehas Sachdeva  
Ayushi Goyal

### COMPILE: 
>gcc -w -o fsp fsp.c -lssl -lcrypto

### EXECUTE:
./fsp `<remoteIP address>` `<tcp remote port>` `<tcp local port>` `<udp remote port>` `<udp local port>`

example: ./fsp 127.0.0.1 3000 4000 5000 6000

### COMMANDS:

* IndexGet longlist  
   
* IndexGet shortlist `<start-time-stamp>` `<end-time-stamp>`  
example: IndexGet shortlist 12-02-2016-18:00:00 20-02-2016-18:00:00

* IndexGet regex `<regex pattern>`
example: IndexGet regex `*.c`

* FileHash verify `<Name-of-file>`

* FileHash checkall

* FileDownload `<TCP or UDP>` `<Name-of-file>`

* History
