#include "headers.h"

  void
init_sockaddr (struct sockaddr_in *name,
    const char *hostname,
    uint16_t port)
{
  struct hostent *hostinfo;

  name->sin_family = AF_INET;
  name->sin_port = htons (port);
  hostinfo = gethostbyname (hostname);
  if (hostinfo == NULL)
  {
    fprintf (stderr, "Unknown host %s.\n", hostname);
    exit (EXIT_FAILURE);
  }
  name->sin_addr = *(struct in_addr *) hostinfo->h_addr;
}

  int
make_socket (uint16_t port)
{
  int sock;
  struct sockaddr_in name;

  /* Create the socket. */
  sock = socket (PF_INET, SOCK_STREAM, 0);
  if (sock < 0)
  {
    perror ("socket");
    exit (EXIT_FAILURE);
  }

  /* Give the socket a name. */
  name.sin_family = AF_INET;
  name.sin_port = htons (port);
  name.sin_addr.s_addr = htonl (INADDR_ANY);
  if (bind (sock, (struct sockaddr *) &name, sizeof (name)) < 0)
  {
    perror ("bind");
    exit (EXIT_FAILURE);
  }

  return sock;
}

  void
serverTCP(int port) 
{
}

int main() {
  
  int sock;
  fd_set active_fd_set, read_fd_set;
  int i;
  struct sockaddr_in clientname;
  size_t size;

  /* Create the socket and set it up to accept connections. */
  sock = make_socket (PORT);
  if (listen (sock, 1) < 0)
  {
    perror ("listen");
    exit (EXIT_FAILURE);
  }

  /* Initialize the set of active sockets. */
  FD_ZERO (&active_fd_set);
  FD_SET (sock, &active_fd_set);

  while (1)
  {
    /* Block until input arrives on one or more active sockets. */
    read_fd_set = active_fd_set;
    if (select (FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0)
    {
      perror ("select");
      exit (EXIT_FAILURE);
    }

    /* Service all the sockets with input pending. */
    for (i = 0; i < FD_SETSIZE; ++i)
      if (FD_ISSET (i, &read_fd_set))
      {
        if (i == sock)
        {
          /* Connection request on original socket. */
          int new;
          size = sizeof (clientname);
          new = accept (sock,
              (struct sockaddr *) &clientname,
              &size);
          if (new < 0)
          {
            perror ("accept");
            exit (EXIT_FAILURE);
          }
          fprintf (stderr,
              "Server: connect from host %s, port %hd.\n",
              inet_ntoa (clientname.sin_addr),
              ntohs (clientname.sin_port));
          FD_SET (new, &active_fd_set);
        }
        else
        {
          /* Data arriving on an already-connected socket. */
          if (read_from_client (i) < 0)
          {
            close (i);
            FD_CLR (i, &active_fd_set);
          }
        }
      }
  }
}





