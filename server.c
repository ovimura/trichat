#include "libs.h"
#define MAXPENDING 10
#define MAXUSERS 20
#define PORT 4444

void* start_rtn(void* arg);

typedef struct pthread_arg_t {
  int sockfd;
  struct sockaddr_in newAddr;
  char *user; 
  int id;
} pthread_arg_t;

int sockfd, newSocket, num_users;
struct sockaddr_in serverAddr, newAddr;
struct init_pkt *p;
pthread_attr_t pthread_attr;
pthread_arg_t* pthread_arg;
pthread_t pthread;
socklen_t len;
char **users;
pthread_mutex_t lock;

int new_user(char *name)
{
  if(num_users == MAXUSERS){
    printf("Maximum users connected, cannot connect at this time\n");
    return -1;
  }
  for(int i = 0; i < num_users; ++i){
    if(!strcmp(users[i], name)){
      printf("%s already taken. Please enter another username: ", name);
//    PROMPT USER AGAIN
    }
  }

  users[num_users] = malloc(strlen(name)+1);
  if(!users[num_users]){
    perror("malloc for new username failed");
    return -1;
  }
  strcpy(users[num_users], name);
  ++num_users;
  return 0; 
}

int main(int argc, char **argv)
{ 
  int ret;
  // Create socket file descriptor
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if(sockfd < 0){
    printf("[-]Error in connection\n");
    exit(EXIT_FAILURE);
  }
  printf("[+]Server Socket is created.\n");
  pthread_mutex_init(&lock, 0);

  memset(&serverAddr, '\0', sizeof(serverAddr));

  // Filling server information
  serverAddr.sin_family = AF_INET; // IPv4
  serverAddr.sin_addr.s_addr = inet_addr("0.0.0.0");
  serverAddr.sin_port = htons(PORT);

  // Bind the socket with the server address

  ret = bind(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
  if(ret < 0){
    perror("[-]Error in binding\n");
    exit(EXIT_FAILURE);
  }
  printf("[+]Bind to port %d\n", PORT);

  if(listen(sockfd, MAXPENDING))
    printf("[-]Error in listening\n");
  else
    printf("[+]Listening...\n");

  if(pthread_attr_init(&pthread_attr)){
    perror("pthread_attr_init failed");
    exit(EXIT_FAILURE);
  }

  if(pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED)){
    perror("pthread_attr_setdetachstate failed");
    exit(EXIT_FAILURE);
  }

  for(;;){
    pthread_arg = (pthread_arg_t*) malloc(sizeof(pthread_arg_t));
    if(!pthread_arg){
      perror("malloc for pthread_arg failed");
      continue;
    }

    len = sizeof(pthread_arg->newAddr);
    pthread_arg->sockfd = accept(sockfd, (struct sockaddr*) &pthread_arg->newAddr, &len);

    if(pthread_arg->sockfd == -1){
      perror("connecting to socket failed");
      free(pthread_arg);
      continue;
    }
    else
      printf("Connection accepted from %s:%d\n", inet_ntoa(newAddr.sin_addr), ntohs(newAddr.sin_port));

    if(pthread_create(&pthread, &pthread_attr, start_rtn, (void*)pthread_arg)){
      perror("creating new thread failed");
      free(pthread_arg);
      continue;
    }
  }

  //  close(sockfd); // Do in signal handler?

  return 0;
}

int thr_cleanup(char *d, pthread_arg_t *arg, int fd)
{
  if(d) free(d);
  if(arg) free(arg);
  close(fd);
  return 0;
}

void* start_rtn(void* arg)
{
  unsigned char *d = malloc(1024), *u;
  pthread_arg_t* pthread_arg = (pthread_arg_t*) arg;
  int thr_sockfd, n;
  struct sockaddr_in thr_addr;
  char data[1024];

  printf("New client thread created with tid %ld\n", pthread_self());
  thr_sockfd = pthread_arg->sockfd;
  thr_addr = pthread_arg->newAddr;

  n = recv(thr_sockfd, d, 1024, 0);
  printf("bytes received %d\n", n);
  memcpy(d, data, n);
  d[n] = '\0';
  u = unhide_zeros((unsigned char*)d);
  struct init_pkt *pkt = (struct init_pkt*)deser_init_pkt(u);
  if(pkt->type != INIT){
    printf("error receiving INIT packet");
    return thr_cleanup(d, arg, thr_sockfd);
  }
  
  pthread_mutex_lock(&lock);
  int ret = new_user(pkt->username);
  pthread_mutex_unlock(&lock);
  if(!ret)
    printf("User %s has connected\n", pkt->username); 
  else if(ret == 1)
    ;
    // PROMPT FOR ANOTHER NAME
  else
    return thr_cleanup(d, arg, thr_sockfd);

  for(;;){
    n = recv(thr_sockfd, data, 1024, 0);
    printf("bytes received %d\n", n);
    memcpy(d, data, n);
    d[n] = '\0';
    u = unhide_zeros((unsigned char*)d);

    for(int i=0; i<1024; i++)
    {
      printf("%02x ", u[i]); 
    }

    if(u[0] == 0x01){
      printf("received multiple INIT packets in same thread");
      return thr_cleanup(d, arg, thr_sockfd);
    }

    if(u[0] == 0x03)
    {
      struct data_pkt *pp = (struct data_pkt*)deser_data_pkt(u);
      printf("DATA PACKET:!!!!!!!!!\n");
      printf("type: %d\n", pp->type);
      printf("id: %d\n", pp->id);
      printf("src: %s\n", pp->src);
      printf("dst: %s\n", pp->dst);
      printf("data: %s\n", pp->data);
    } else
      if(!strcmp(data, ":exit")){
        printf("Disconnected %s:%d\n", inet_ntoa(thr_addr.sin_addr), ntohs(thr_addr.sin_port));
        break;
      } else{
        printf("Client: %s\n", data);
        send(thr_sockfd, data, strlen(data), 0);
        bzero(data, sizeof(data));
      }
  }

  return thr_cleanup(d, arg, sockfd);
}


