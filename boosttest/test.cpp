   #include <iostream>
   #include <sys/types.h>
   #include <sys/socket.h>
   #include <netinet/in.h>
   #include <arpa/inet.h>
   #include <sys/epoll.h>
   #include <pthread.h>
   #include <fcntl.h>
   #include <errno.h>
   #include <unistd.h>
   #include <stdio.h>
   #include <list>
   #include <vector>
   #include <array>
   #include <map>
   #include <time.h>
  #include <cstring>

   // Default buffer size
   #define BUF_SIZE 1024

   // Default buffer size
   #define MAX_NICKNAME 16

   // Default port
   #define SERVER_PORT 44444

   // seChat server ip, you should change it to your own server ip address
   #define SERVER_HOST "127.0.0.1"

   // Default timeout - http://linux.die.net/man/2/epoll_wait
   #define EPOLL_RUN_TIMEOUT -1

   // Count of connections that we are planning to handle (just hint to kernel)
   #define EPOLL_SIZE 10000

   // First welcome message from server
   #define STR_WELCOME "Добро пожаловать в чатик,гость. Назови себя:"

   // Format of message population
   #define STR_MESSAGE "Client #%s>> %s"

   // Warning message if you alone in server
   #define STR_NOONE_CONNECTED "Noone connected to server except you!"

   // Commad to exit
   #define CMD_EXIT "EXIT"

   // Macros - exit in any error (eval < 0) case
   #define CHK(eval) if(eval < 0){perror("eval"); exit(-1);}

   // Macros - same as above, but save the result(res) of expression(eval)
   #define CHK2(res, eval) if((res = eval) < 0){perror("eval"); exit(-1);}

   // Preliminary declaration of functions
   int setnonblocking(int sockfd);
   //void debug_epoll_event(epoll_event ev);
   int print_incoming(int fd);

int setnonblocking(int sockfd)
{
   CHK(fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0)|O_NONBLOCK));
   return 0;
}

   using namespace std;
   int handle_message(int new_fd,int nicknameid);
   
   
   // To store client's socket list
   list<int> clients_list;
   //vector<array<char, MAX_NICKNAME>> nickname_vector;
   map <int,string> nickname_map;
   vector<string> nickname_vector;
   int main(int argc, char *argv[])
   {
        
    //string client_name;
       int listener;
       bool nameflag=0; 
       struct sockaddr_in addr, their_addr;
       addr.sin_family = PF_INET;
       addr.sin_port = htons(atoi(argv[2]));
       addr.sin_addr.s_addr = inet_addr(argv[1]);

       //     size of address
       socklen_t socklen;
       socklen = sizeof(struct sockaddr_in);

       //     event template for epoll_ctl(ev)
       //     storage array for incoming events from epoll_wait(events)
       //        and maximum events count could be EPOLL_SIZE
       static struct epoll_event event, events[EPOLL_SIZE];
       //     watch just incoming(EPOLLIN)
       //     and Edge Trigged(EPOLLET) events
       event.events = EPOLLIN | EPOLLET;

       //     chat message buffer
       char message[BUF_SIZE];

       //     epoll descriptor to watch events
       int epfd;

       //     to calculate the execution time of a program
       clock_t tStart;

       // other values:
       //     new client descriptor(client)
       //     to keep the results of different functions(res)
       //     to keep incoming epoll_wait's events count(epoll_events_count)
       int client, res, epoll_events_count;


       // *** Setup server listener
       //     create listener with PF_INET(IPv4) and
       //     SOCK_STREAM(sequenced, reliable, two-way, connection-based byte stream)
       CHK2(listener, socket(PF_INET, SOCK_STREAM, 0));
       printf("Main listener(fd=%d) created! \n",listener);

       //    setup nonblocking socket
       setnonblocking(listener);

       //    bind listener to address(addr)
       CHK(bind(listener, (struct sockaddr *)&addr, sizeof(addr)));
       printf("Listener binded to: %s\n", SERVER_HOST);

       //    start to listen connections
       CHK(listen(listener, 1));
       printf("Start to listen: %s!\n", SERVER_HOST);

       // *** Setup epoll
       //     create epoll descriptor
       //     and backup store for EPOLL_SIZE of socket events
       CHK2(epfd,epoll_create(EPOLL_SIZE));
       printf("Epoll(fd=%d) created!\n", epfd);
      
       //     set listener to event template
       event.data.fd = listener;

       //     add listener to epoll
       CHK(epoll_ctl(epfd, EPOLL_CTL_ADD, listener, &event));
       printf("Main listener(%d) added to epoll!\n", epfd);

       // *** Main cycle(epoll_wait)
       while(1)
       {
           CHK2(epoll_events_count,epoll_wait(epfd, events, EPOLL_SIZE, EPOLL_RUN_TIMEOUT));
           
           // setup tStart time
           //tStart = clock();

           for(int i = 0; i < epoll_events_count ; i++)
           {
     
                   // EPOLLIN event for listener(new client connection)
                   if(events[i].data.fd == listener)
                   {
                           CHK2(client,accept(listener, (struct sockaddr *) &their_addr, &socklen));
     
                           // setup nonblocking socket
                           setnonblocking(client);

                           // set new client to event template
                           event.data.fd = client;

                           // add new client to epoll
                           CHK(epoll_ctl(epfd, EPOLL_CTL_ADD, client, &event));

                           // save new descriptor to further use
                           clients_list.push_back(client); // add new connection to list of clients


                           // send initial welcome message to client
                           bzero(message, BUF_SIZE);
                           res = sprintf(message, STR_WELCOME, client);
                           CHK2(res, send(client, message, BUF_SIZE-MAX_NICKNAME, 0));
                          /* bzero(message, BUF_SIZE);
                           CHK2(res,recv(client, message, BUF_SIZE, 0));
                           nickname_list.push_back(message);
                           //CHK2(res, send(client, "Списибки", BUF_SIZE, 0));*/
                            

                   }else { // EPOLLIN event for others(new incoming message from client)
                           CHK2(res,handle_message(events[i].data.fd,epfd));
                           //pthread_create(NULL, NULL, (LPTHREAD_START_ROUTINE)handle_message(events[i].data.fd ,1), NULL, NULL);
                   }
           }
           // print epoll events handling statistics
          /* printf("Statistics: %d events handled at: %.2f second(s)\n",
                                           epoll_events_count,
                                           (double)(clock() - tStart)/CLOCKS_PER_SEC);
                                           */
       }

       close(listener);
       close(epfd);

       return 0;
   }

   //void
//int counter =0;
   // *** Handle incoming message from clients
   int handle_message(int client, int nickname_id)
   {    
        
        /*if(nickname_vector.size() < clients_list.size()){  
                return get_nickname(client);
        }*/
        

       cout<<"''''"<<client<<endl;
       cout<<"----"<<nickname_id<<endl;
       // get row message from client(buf)
       //     and format message to populate(message)
       char buf[BUF_SIZE], message[BUF_SIZE];
       bzero(buf, BUF_SIZE);
       bzero(message, BUF_SIZE);

       // to keep different results
       int len;

       // try to get new raw message from client

       CHK2(len,recv(client, buf, BUF_SIZE-MAX_NICKNAME, 0));

       // zero size of len mean the client closed connection
       if(len == 0){
           CHK(close(client));
           clients_list.remove(client);

       // populate message around the world
       }else{

           if(clients_list.size() == 1) { // this means that noone connected to server except YOU!
                   CHK(send(client, STR_NOONE_CONNECTED, strlen(STR_NOONE_CONNECTED), 0));
                   return len;
           }
           if(nickname_vector.size() < clients_list.size()){  
                //nickname_map[client]=buf;
                nickname_vector.push_back(buf);
                  return len;
        }
          
        // format message to populate
       // cout<<"nick "<< nickname_map[client]<<endl;
         sprintf(message, "%s>> %s",nickname_vector[client-nickname_id-1].data(), buf);
        //cout<< *nickname_vector.end()<<endl;
           // populate message around the world ;-)...
           list<int>::iterator it;
           for(it = clients_list.begin(); it != clients_list.end(); it++){
                   CHK(send(*it, message, BUF_SIZE-MAX_NICKNAME, 0));
                   
          }
       }
       return len;
   }
