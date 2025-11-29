#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>

#include <arpa/inet.h>

#include <fcntl.h>

#include <netinet/in.h>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <unistd.h>

constexpr int PORT = 8080;
constexpr int MAX_CONNECTIONS = 10;
constexpr int MAX_EVENTS = 10;
constexpr int BUF_SIZE = 16;
constexpr int MAX_LINE = 256;

struct stats {
  unsigned int nof_all_connections {0};
  unsigned int nof_current_connections {0};
  bool shutdown = false;
};

bool
process_input_data(const char *input_data, const int fd, stats *stats)
{
  if (input_data[0] == '/') {
    // Вывод времени
    if (strcmp(input_data, "/time") == 0) {
      std::time_t time = std::time(0);
      std::tm  *now = std::localtime(&time);
      char buf[80];
      std::strftime(buf, sizeof(buf), "%Y-%m-%d %X", now);
      write(fd, buf, strlen(buf));
    }
    // Вывод статистики
    else if (strcmp(input_data, "/stats") == 0) {
      std::string str = "Number of all connections = "
        + std::to_string(stats->nof_all_connections) + "\n"
        + "Number of current connections = "
        + std::to_string(stats->nof_current_connections) + "\n";
      write(fd, str.c_str(), str.length());
    }
    // Завершение работы
    else if (strcmp(input_data, "/shutdown") == 0) {
      stats->shutdown = true;
    }
    else {
      std::string message = "Wrong command!\n";
      write(fd, message.c_str(), message.length());
    }
    return true;
  }
  return false;
}

// TODO: задавать порт через входной аргумент
// TODO: перенести однотипные действия в отдельные функции
int
main()
{
  stats stats;
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
        ++stats.nof_all_connections;
        ++stats.nof_current_connections;
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
            
            bool process_status = process_input_data(read_data, events[i].data.fd, &stats);
            if (!process_status) {
              write(events[i].data.fd, read_data,strlen(read_data));
            }
          }
        }
      } 

      // Закрытие соединения
      if (events[i].events & (EPOLLRDHUP | EPOLLHUP)) {
        --stats.nof_current_connections;
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

      if (stats.shutdown) {
        break;
      }
    }
    if (stats.shutdown) {
      std::cout << "Shutting down...\n";
      for(int i = 0; i < nof_fds; ++i) {
        close(events[i].data.fd);
      }
      shutdown(listen_socket, SHUT_RDWR);
      close(listen_socket);
      close(epoll);
      break;
    }
  }

  return 0;
}