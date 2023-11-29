#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>

char ARGS[] = "Wrong number of arguments\n";
char FATAL[] = "Fatal error\n";

char MSG[] = "client %d: ";
char LEAVE[] = "server: client %d just left\n";
char JOIN[] = "server: client %d just arrived\n";

fd_set rfds, wfds, fds;

int maxid = 0;
int maxfd = 0;

char *msgs[70000] = {};
int ids[70000] = {};

char wbuf[1025] = {};
char rbuf[1025] = {};

int extract_message(char **buf, char **msg)
{
  char *newbuf;
  int i;

  *msg = 0;
  if (*buf == 0)
    return (0);
  i = 0;
  while ((*buf)[i])
  {
    if ((*buf)[i] == '\n')
    {
      newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
      if (newbuf == 0)
        return (-1);
      strcpy(newbuf, *buf + i + 1);
      *msg = *buf;
      (*msg)[i + 1] = 0;
      *buf = newbuf;
      return (1);
    }
    i++;
  }
  return (0);
}

char *str_join(char *buf, char *add)
{
  char *newbuf;
  int len;

  if (buf == 0)
    len = 0;
  else
    len = strlen(buf);
  newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
  if (newbuf == 0)
    return (0);
  newbuf[0] = 0;
  if (buf != 0)
    strcat(newbuf, buf);
  free(buf);
  strcat(newbuf, add);
  return (newbuf);
}

void puterr(char *msg)
{
  write(2, msg, strlen(msg));
  exit(1);
}

void fatal()
{
  puterr(FATAL);
}

void argerr()
{
  puterr(ARGS);
}

void notify(int client, char *msg)
{
  for (int fd = 0; fd <= maxfd; fd++)
    if (FD_ISSET(fd, &wfds) && client != fd)
      send(fd, msg, strlen(msg), 0);
}

void handle_join(int client)
{
  if (client > maxfd)
    maxfd = client;
  ids[client] = maxid++;
  msgs[client] = NULL;
  FD_SET(client, &fds);
  sprintf(wbuf, JOIN, ids[client]);
  notify(client, wbuf);
}

void handle_leave(int client)
{
  sprintf(wbuf, LEAVE, ids[client]);
  notify(client, wbuf);
  free(msgs[client]);
  msgs[client] = NULL;
  FD_CLR(client, &fds);
  close(client);
}

void handle_msg(int client, int n)
{
  rbuf[n] = 0;
  msgs[client] = str_join(msgs[client], rbuf);
  char *msg = NULL;
  while (extract_message(&msgs[client], &msg))
  {
    sprintf(wbuf, MSG, ids[client]);
    notify(client, wbuf);
    notify(client, msg);
    free(msg);
    msg = NULL;
  }
}
int main(int argc, char **argv)
{
  if (argc != 2)
    argerr();
  FD_ZERO(&fds);

  int server = socket(AF_INET, SOCK_STREAM, 0);
  if (server == -1)
    fatal();

  FD_SET(server, &fds);
  maxfd = server;

  struct sockaddr_in servaddr;
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(2130706433); // 127.0.0.1
  servaddr.sin_port = htons(atoi(argv[1]));

  if ((bind(server, (const struct sockaddr *)&servaddr, sizeof(servaddr))) == -1)
    fatal();
  if (listen(server, 10) == -1)
    fatal();
  while (69)
  {
    rfds = wfds = fds;
    if (select(maxfd + 1, &rfds, &wfds, NULL, NULL) == -1)
      fatal();

    for (int fd = 0; fd <= maxfd; fd++)
    {
      if (!FD_ISSET(fd, &rfds))
        continue;
      if (fd == server)
      {
        int client = accept(fd, NULL, NULL);
        if (client == -1)
          continue;
        handle_join(client);
      }
      else
      {
        int n = recv(fd, rbuf, 1024, 0);
        if (n <= 0)
          handle_leave(fd);
        else
          handle_msg(fd, n);
      }
      break;
    }
  }
}