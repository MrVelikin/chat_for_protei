#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <cstring>
#include <list>
#include <time.h>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>


// Default buffer size
#define BUF_SIZE 1024

// Default timeout 
#define EPOLL_RUN_TIMEOUT -1

#define MAX_NICKNAME 16

// Count of connections that we are planning to handle (just hint to kernel)
#define EPOLL_SIZE 10000

// Commad to exit
#define CMD_EXIT "EXIT"

// Macros - exit in any error (eval < 0)
#define CHK(eval) if(eval < 0){perror("eval"); exit(-1);}

namespace logging = boost::log;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;

void init()
{   
    logging::add_file_log
    (
        keywords::file_name = "Client_%N.log",                                        
        keywords::rotation_size = 10 * 1024 * 1024,                                  
        keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0), 
        keywords::format = "[%TimeStamp%]: %Message%",                                 
        keywords::open_mode
    );
}

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
        char message[BUF_SIZE];

        int sock, pid, pipe_fd[2], epfd;

        struct sockaddr_in addr;
        addr.sin_family = PF_INET;
        addr.sin_port = htons(atoi(argv[2]));
        addr.sin_addr.s_addr = inet_addr(argv[1]);

        static struct epoll_event event, events[2]; // Socket(in|out) & Pipe(in)
        event.events = EPOLLIN | EPOLLET;

        bool continue_to_work = true;


        sock=socket(PF_INET, SOCK_STREAM, 0);
        if(sock<0)
        {
            BOOST_LOG_SEV(lg, error) << "Ошибка создания сокета";
            perror("socket");
            return -1;
        }
        BOOST_LOG_SEV(lg, info) << "Создан сокет - "<< sock;
        CHK(connect(sock, (struct sockaddr *)&addr, sizeof(addr)) );//<0

        BOOST_LOG_SEV(lg, info) << "Соединение прошло успешно "<< sock;

        CHK(pipe(pipe_fd));
        BOOST_LOG_SEV(lg, info) << "Создался pipe с pipe_fd[0](для чтения): '"<<pipe_fd[0]<< "' и pipe_fd[1](для записи): "<<pipe_fd[1];

        epfd=epoll_create(EPOLL_SIZE);
        if (epfd<0)
        {
            BOOST_LOG_SEV(lg, error) << "Ошибка создания epoll";
            perror("epoll_create");
            return -1;
        }
        
        BOOST_LOG_SEV(lg, info) << "Создался epoll c fd: "<<epfd;
        
        event.data.fd = sock;
        CHK(epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &event));
         BOOST_LOG_SEV(lg, info) << "Сокет добавлен в epoll fd="<<sock;

        event.data.fd = pipe_fd[0];
        CHK(epoll_ctl(epfd, EPOLL_CTL_ADD, pipe_fd[0], &event));

        // Fork
        pid=fork();
        if(pid<0)
        {
            BOOST_LOG_SEV(lg, error) << "Ошибка создания fork";
            perror("fork");
            return -1;
        }
        BOOST_LOG_SEV(lg, info) << "Создался fork(программа разветлилась)";
        switch(pid){
            case 0: // child
                close(pipe_fd[0]); 
                printf("Введите 'exit' для выхода из чатика\n");
                while(continue_to_work){
                    bzero(&message, BUF_SIZE);
                    fgets(message, BUF_SIZE, stdin);

                    if(strncasecmp(message, CMD_EXIT, strlen(CMD_EXIT)) == 0){
                        continue_to_work = false;
                    }else 
                    {
                        BOOST_LOG_SEV(lg, debug) << "Передача упраления родительскому потоку";
                        CHK(write(pipe_fd[1], message, strlen(message) - 1));
                    }
                }
                break;
            default: //parent 
                    close(pipe_fd[1]); 

                    int epoll_events_count, result;

                    // Основной цикл epoll_wait
                    while(continue_to_work) {
                        epoll_events_count=epoll_wait(epfd, events, 2, EPOLL_RUN_TIMEOUT);
                        if (epoll_events_count<0)
                        {
                            BOOST_LOG_SEV(lg, error) << "Ошибка epoll_wait";
                            perror("epoll_wait");
                        }

                        for(int i = 0; i < epoll_events_count ; i++){
                            bzero(&message, BUF_SIZE);

                            // EPOLLIN event from server( new message from server)
                            if(events[i].data.fd == sock){
                                BOOST_LOG_SEV(lg, info) << "Сервер прислал сообщение";
                                result=recv(sock, message, BUF_SIZE, 0);
                                if(result<0)
                                {
                                    BOOST_LOG_SEV(lg, error) << "Ошибка recv";
                                    perror("recv");
                                    return -1;
                                }

                                if(result == 0){
                                        BOOST_LOG_SEV(lg, info) << "Сервер закрыл соединение";
                                        CHK(close(sock));
                                        continue_to_work = false;
                                }else 
                                {
                                    BOOST_LOG_SEV(lg, debug) << "Пришло сообщение - "<<message;
                                    printf("%s\n", message);
                                }
                            }else{
                                result=read(events[i].data.fd, message, BUF_SIZE);
                                if (result<0)
                                {
                                    BOOST_LOG_SEV(lg, error) << "Ошибка чтения";
                                    perror("read");
                                    return -1;
                                }

                                if(result == 0) continue_to_work = false;
                                else{
                                        CHK(send(sock, message, BUF_SIZE-MAX_NICKNAME, 0));
                                        BOOST_LOG_SEV(lg, info) << "Сообщение '"<<message<<"' отправлено";
                                }
                            }
                        }
                    }
        }
        if(pid)
        {
            BOOST_LOG_SEV(lg, info) << "Отключился родительский поток";
            close(pipe_fd[0]);
            close(sock);
        }else
        {
            BOOST_LOG_SEV(lg, info) << "Отключился дочерний поток";
            close(pipe_fd[1]);
        }
    }
    catch (std::exception& except)
    {
        BOOST_LOG_SEV(lg, error) <<"Exception" <<except.what() ;
        std::cerr << "Exception: " << except.what() << "\n";
    }
    return 0;
}


