#include <cerrno>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>

#include <arpa/inet.h>

#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <unistd.h>

constexpr unsigned int PORT = 8080;
constexpr unsigned int MAX_LINE = 256;

int
main()
{
  int server_socket = socket(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in server_address;
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(PORT);

  if (connect(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
    perror("Error in connect()\n");
    exit(EXIT_FAILURE);
  }

  while (true) {
    std::cout << "Enter data: ";
    char buf[MAX_LINE];
    std::cin >> buf;
    write(server_socket, buf, strlen(buf));

    bzero(buf, sizeof(buf));
    int nof_bytes_read = read(server_socket, buf, sizeof(buf));
    if (nof_bytes_read == -1) {
      perror("Error in read()\n");
      exit(EXIT_FAILURE);
    }
    if (nof_bytes_read == 0) {
      printf("EOF\n");
    }

    write(STDOUT_FILENO, buf, nof_bytes_read);
    std::cout << "\n";
  }
  close(server_socket);

  return 0;
}