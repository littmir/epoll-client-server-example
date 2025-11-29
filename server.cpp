#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <iostream>

#include <arpa/inet.h>

#include <fcntl.h>

#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <unistd.h>

constexpr int PORT = 8080;
constexpr int MAX_CONNECTIONS = 10;
constexpr int MAX_EVENTS = 10;
constexpr int BUF_SIZE = 16;
constexpr int MAX_LINE = 256;

// TODO: задавать порт через входной аргумент
// TODO: перенести однотипные действия в отдельные функции
int
main()
{
  int listen_socket = socket(AF_INET, SOCK_STREAM, 0);

  sockaddr_in server_addr {};
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(PORT);

  int bind_status = bind(listen_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
  if (bind_status == -1) {
    perror("Error in bind()\n");
    exit(EXIT_FAILURE);
  }

  if (fcntl(listen_socket, F_SETFL, 
      fcntl(listen_socket, F_GETFL, 0) | O_NONBLOCK) == -1) {
    perror("Error in fctl() for listen socket\n");
    exit(EXIT_FAILURE);
  }

  int listen_status = listen(listen_socket, MAX_CONNECTIONS); 
  if (listen_status == -1) {
    perror("Error in listen()\n");
    exit(EXIT_FAILURE);
  }
  std::cout << "Listen on " << ntohs(server_addr.sin_port) << " for connections...\n";

  int epoll = epoll_create(1);

  struct epoll_event ev;
  ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
  ev.data.fd = listen_socket;
  if (epoll_ctl(epoll, EPOLL_CTL_ADD, listen_socket, &ev) == -1) {
    perror("Error in epol_ctl() for listen_socket\n");
    exit(1);
  }

  epoll_event events[MAX_EVENTS];
  while (true) {
    int nof_fds = epoll_wait(epoll, events, MAX_EVENTS, -1);
    for(int i = 0; i < nof_fds; ++i) {
      // Новое соединение
      if (events[i].data.fd == listen_socket) {
        sockaddr_in client_addr;
        socklen_t client_socklen = sizeof(client_addr);
        int client_socket = accept(listen_socket, (struct sockaddr *)&client_addr, &client_socklen);

        char buf_address[BUF_SIZE];
        strcpy(buf_address, inet_ntoa(client_addr.sin_addr));
        std::cout << "New connection from " << buf_address << ":" << ntohs(client_addr.sin_port) << "\n";

        if (fcntl(client_socket, F_SETFL,
                  fcntl(client_socket, F_GETFL, 0) | O_NONBLOCK) == -1) {
          perror("Error in fctl() for client_socket\n");
          exit(EXIT_FAILURE);
        }

        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLHUP;
        ev.data.fd = client_socket;
        if (epoll_ctl(epoll, EPOLL_CTL_ADD, client_socket, &ev) == -1) {
          perror("Error in epol_ctl() for clinet_socket\n");
          exit(EXIT_FAILURE);
        }
      }

      // Данные
      if (events[i].events & EPOLLIN) {
        while(true) {
          char read_data[MAX_LINE] = {};
          int nof_read = read(events[i].data.fd, read_data,sizeof(read_data));
          if (nof_read <= 0) {
            break;
          } else {
            struct sockaddr_in client_addr;
            socklen_t client_addr_size = sizeof(client_addr);
            getpeername(events[i].data.fd, (struct sockaddr *)&client_addr, &client_addr_size);
            char buf_address[BUF_SIZE];
            strcpy(buf_address, inet_ntoa(client_addr.sin_addr));
            std::cout << "Message: \"" << read_data << "\" from " << buf_address << ntohs(client_addr.sin_port) <<"\n";
            
            write(events[i].data.fd, read_data,strlen(read_data));
          }
        }
      } 

      // Закрытие соединения
      if (events[i].events & (EPOLLRDHUP | EPOLLHUP)) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_size = sizeof(client_addr);
        getpeername(events[i].data.fd, (struct sockaddr *)&client_addr,&client_addr_size);
        char buf_address[BUF_SIZE];
        strcpy(buf_address, inet_ntoa(client_addr.sin_addr));
        std::cout << "Connection closed from "<< buf_address << ntohs(client_addr.sin_port) <<"\n";

        epoll_ctl(epoll, EPOLL_CTL_DEL,events[i].data.fd, nullptr);
        close(events[i].data.fd);
        continue;
      }
    }
  }

  return 0;
}