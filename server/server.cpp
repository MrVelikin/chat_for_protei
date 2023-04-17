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
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>


#define BUF_SIZE 1024
#define MAX_NICKNAME 16
#define EPOLL_RUN_TIMEOUT -1
#define EPOLL_SIZE 10000
#define CMD_EXIT "EXIT"
#define CHK(eval) if(eval < 0){perror("eval"); exit(-1);}    // Макрос - в случае (eval < 0) выход из программы с выводом ошибки

namespace logging = boost::log;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;

using namespace std;

void init()
{   
    logging::add_file_log
    (
        keywords::file_name = "Client_%N.log",                                        
        keywords::rotation_size = 10 * 1024 * 1024,                                  
        keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0), 
        keywords::format = "[%TimeStamp%]: %Message%",                                 
        keywords::open_mode
        //keywords::auto_flush = true
    );
}

int setnonblocking(int sockfd)
{
   CHK(fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0)|O_NONBLOCK));
   return 0;
}

int handle_message(int new_fd,int id);

list<int> clients_list;
map <int,string> nickname_map;

int main(int argc, char *argv[])
{
    init();
    logging::add_common_attributes();
    using namespace logging::trivial;
    src::severity_logger< severity_level > lg;

    BOOST_LOG_SEV(lg, debug) << "Программа запущена";

    try
    {
        if (argc != 3)
        {
            BOOST_LOG_SEV(lg, info) << "Неправильный формат записи, количество параметров = "<<argc;
            std::cout << "Используйте:client <ip хоста> <порт>\n";
            return 1;
        }

        int listener;
        struct sockaddr_in addr, their_addr;
        addr.sin_family = PF_INET;
        addr.sin_port = htons(atoi(argv[2]));
        addr.sin_addr.s_addr = inet_addr(argv[1]);

        socklen_t socklen;
        socklen = sizeof(struct sockaddr_in);

        static struct epoll_event event, events[EPOLL_SIZE];
        event.events = EPOLLIN | EPOLLET;

        char message[BUF_SIZE];
    
        int epfd; //для просмотра эвентов в епол

        int client, result, epoll_events_count;

        listener=socket(PF_INET, SOCK_STREAM, 0);
        if(listener<0)
        {
            perror("listener");
            BOOST_LOG_SEV(lg, error) << "Ошибка создания listener";
            return -1;
        }
        printf("Создался listener(fd=%d) \n",listener);
        BOOST_LOG_SEV(lg, info) << "Создался listener";

        setnonblocking(listener);

        CHK(bind(listener, (struct sockaddr *)&addr, sizeof(addr)));
        printf("Listener связан с: %s\n", argv[1]);
        BOOST_LOG_SEV(lg, info) << "Listener связан с: "<<argv[1];

        CHK(listen(listener, 1));
        printf("Начинаем 'слушать': %s!\n", argv[1]);

        epfd=epoll_create(EPOLL_SIZE);
        if(epfd<0)
        {
            BOOST_LOG_SEV(lg, error) << "Ошибка в epoll_create";
            perror("epoll_create");
            return -1;
        }
        printf("Epoll(fd=%d) готов\n", epfd);
        BOOST_LOG_SEV(lg, info) << "Epoll успешно создался "<<argv[1];

        event.data.fd = listener;

        CHK(epoll_ctl(epfd, EPOLL_CTL_ADD, listener, &event));
        printf("listener(%d) добавлен в epoll\n", epfd);
        BOOST_LOG_SEV(lg, info) << "listener добавлен в epoll";

        // Основной цикл для ожидания событий epoll_wait
        while(1)
        {
           epoll_events_count=epoll_wait(epfd, events, EPOLL_SIZE, EPOLL_RUN_TIMEOUT);
           if(epoll_events_count<0)
           {
            BOOST_LOG_SEV(lg, error) << "Ошибка в epoll_create";
            perror("epoll_events_count");
            return -1;
           }
           for(int i = 0; i < epoll_events_count ; i++)
           {
     
                if(events[i].data.fd == listener)
                {
                    client=accept(listener, (struct sockaddr *) &their_addr, &socklen);
                    if(client<0)
                    {
                        BOOST_LOG_SEV(lg, error) << "Ошибка авторизации";
                        perror("accept");
                        return -1;
                    }     
                    BOOST_LOG_SEV(lg, debug) << "Соединение успешно установленно с client - "<<client;

                    setnonblocking(client);

                    event.data.fd = client;

                    CHK(epoll_ctl(epfd, EPOLL_CTL_ADD, client, &event));
                    BOOST_LOG_SEV(lg, debug) << "Ошибка отправки клиенту сообщения";

                    clients_list.push_back(client);
            
                    bzero(message, BUF_SIZE);
                    sprintf(message, "Добро пожаловать в чатик,гость. Назови себя:");
                    result=send(client, message, BUF_SIZE, 0);  
                    if(result<0)
                        {
                            perror("send");
                            BOOST_LOG_SEV(lg, error) << "Ошибка отправки клиенту сообщения";
                            return -1;
                        }                        
                }else { 
                        BOOST_LOG_SEV(lg, debug) << "Пришло новое сообщение";
                        result=handle_message(events[i].data.fd,epfd);
                        if(result<0)
                        {
                            perror("handle_message");
                            BOOST_LOG_SEV(lg, error) << "Ошибка выхова функции handle_message";
                            return -1;
                        }
                }
           }
       }
       close(listener);
       close(epfd);
    }catch (std::exception& except)
    {
       BOOST_LOG_SEV(lg, error) << except.what() ;
        std::cerr << "Exception: " << except.what() << "\n";
    }
    return 0;
}

    //handle для входящих сообщений
int handle_message(int client, int epfd_id)
{    
    char buf[BUF_SIZE], message[BUF_SIZE];
    bzero(buf, BUF_SIZE);
    bzero(message, BUF_SIZE);        
    int len;

    len=recv(client, buf, BUF_SIZE, 0);
    if(len<0)
    {
        perror("handle_message");
        return -1;     
    }

    if(len == 0){// клиент закрыл соединение
        CHK(close(client));
        clients_list.remove(client);
        nickname_map.erase(client);

    }else
    {

        if(nickname_map.count(client)==0)
        {  
            if(strlen(buf)> MAX_NICKNAME)
                {CHK(send(client, "Слишком длинный никнейм", strlen("Слишком длинный никнейм"), 0));}
            else  
            {
                nickname_map[client]=buf;
                sprintf(message, "Спасибо,теперь это ваш ник - %s",nickname_map[client].data());
                CHK(send(client, message, strlen(message), 0));
            }
            return len;
        }
        if(clients_list.size() == 1) 
        { 
            CHK(send(client, "В чате сейчас пусто", strlen("В чате сейчас пусто"), 0));
            return len;
        }
        sprintf(message, "%s>> %s",nickname_map[client].data(), buf);

        list<int>::iterator it;
        for(it = clients_list.begin(); it != clients_list.end(); it++){
                CHK(send(*it, message, BUF_SIZE, 0));
        }
    }
    return len;
}
