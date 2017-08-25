#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<ctype.h>
#include<netdb.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<arpa/inet.h>   // inet_ntop
#include<signal.h>
#include<fcntl.h>       // open, O_WRONLY, O_TRUNC, O_CREAT, etc
#include<unistd.h>      // fork, dup, exec, read/write, getdtablesize, etc
#include<string.h>      // memset
#include<string>        // c++ string
#include<vector>
#include<sstream>       // convert string to stream
#include<iostream>
#include<algorithm>     // remove, find 
#include<utility>       // pair

using namespace std;
const int MAX_CLIENT = 10;
int PORT = 65320;
int BIND_PORT = 62982;

void signal_handler(int signo)
{
	switch (signo)
	{
	case SIGCHLD:
		waitpid(-1, NULL, WNOHANG);
		break;
	case SIGUSR1:
		// write(my_fd, shm_who_list[my_index].getmsg1(), strlen(shm_who_list[my_index].getmsg1()));
		break;
	case SIGUSR2:
		// write(my_fd, shm_who_list[my_index].getmsg2(), strlen(shm_who_list[my_index].getmsg2()));
		break;
	case SIGINT:
		// /* detach from the segment: */
		// if (shmdt(shm_msg) == -1 || shmdt(shm_who_list) == -1 || shmdt(shm_public_pipe) == -1) {
		//     perror("shmdt");
		//     exit(1);
		// }
		// shmctl(shmid_msg, IPC_RMID, NULL);
		// shmctl(shmid_who_list, IPC_RMID, NULL);
		// shmctl(shmid_public_pipe, IPC_RMID, NULL);

		// sem_rm(semid_who_list);
		// sem_rm(semid_public_pipe);

		// exit(0);
		break;
	default:
		return;
	}
}

string read_input(int newfd) {
	char buf[65536];
	memset(buf, 0, sizeof(buf));
	string s = "";
	while (true) {
		int len = read(newfd, buf, sizeof(buf));

		if (buf[len - 1] == '\n' || (int)buf[len - 1] == 10 || (int)buf[len - 1] == 13) {
			s += string(buf);
			return s;
		}
		else {
			s += string(buf);
		}
	}
}

int firewall_check(char* ipaddr) {
	char buff[65536];
	char tmpip[100];
	strncpy(tmpip, ipaddr, sizeof(tmpip));
	char *tmp3 = strtok(tmpip, ".");
	char ip[4][20];
	int c = 0;
	while (tmp3 != NULL) {
		strncpy(ip[c], tmp3, sizeof(ip[c]));
		c++;
		tmp3 = strtok(NULL, ".");
	}

	FILE *p = fopen("socks.conf", "r");
	if (p == NULL)
		return 1;

	int matched = 0;
	while (fgets(buff, sizeof(buff), p) != NULL) {
		char action[100];
		char addr[100];
		char *tmp = strtok(buff, " ");
		strcpy(action, tmp);
		tmp = strtok(NULL, " ");
		strcpy(addr, tmp);
		// printf("%s---%s\n", action, addr);
		if (!strcmp(action, "permit")) {
			char *tmp2 = strtok(addr, ".\r\n");
			c = 0;
			int match_cnt = 0;
			while (tmp2 != NULL) {
				// printf("%s >> << %s\n", tmp2, ip[c]);
				if ( !strcmp(tmp2, "*") || !strcmp(tmp2, ip[c]) ) {
					match_cnt++;
				}
				else {
					break;
				}

				if (match_cnt == 4)
					matched = 1;

				c++;
				tmp2 = strtok(NULL, ".\r\n");
			}
		}
	}

	if (matched == 1)
		return 1;
	else
		return -1;
}

void socks_proxy(int srcfd) {

	unsigned char buf[65536];
	// read SOCKS request
	memset(buf, 0, sizeof(buf));
	read(srcfd, buf, sizeof(buf));

	unsigned char VN = buf[0] ;
	unsigned char CD = buf[1] ;
	unsigned int DST_PORT = buf[2] << 8 |
	                        buf[3] ;
	unsigned int DST_IP = buf[4] << 24 |
	                      buf[5] << 16 |
	                      buf[6] << 8 |
	                      buf[7] ;
	unsigned char* USER_ID = buf + 8;

	printf("VN: %d CD: %d DST_IP: %d.%d.%d.%d DST_PORT: %d USER_ID: %s\n", VN, CD, buf[4], buf[5], buf[6], buf[7], DST_PORT, USER_ID);

	unsigned char reply[8];
	char ip_addr[100];
	snprintf(ip_addr, sizeof(ip_addr), "%d.%d.%d.%d", buf[4], buf[5], buf[6], buf[7]);
	int sock_fd;
	struct hostent *he;
	struct sockaddr_in  client_sin, cli_addr;
	int nfds = getdtablesize();
	fd_set rfds, a_rfds; // read fds, active read fds
	FD_ZERO(&a_rfds);

	if (firewall_check(ip_addr) != -1) { // accept connection

		if (CD == 1) { // CONNECT mode
			printf("SOCKS CONNECT mode permitted\n");
			reply[0] = 0;
			reply[1] = (unsigned char) 90 ;
			reply[2] = DST_PORT / 256;
			reply[3] = DST_PORT % 256;
			reply[4] = DST_IP >> 24 ;
			// ip = ip in SOCKS4_REQUEST for connect mode
			// ip = 0 for bind mode
			reply[5] = (DST_IP >> 16) & 0xFF;
			reply[6] = (DST_IP >> 8)  & 0xFF;
			reply[7] = DST_IP & 0xFF;
			write(srcfd, reply, 8);

			if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
				perror("socket");
				exit(1);
			}

			if ((he = gethostbyname(ip_addr)) == NULL) {
				perror("gethostbyname");
				exit(1);
			}


			memset(&client_sin, 0, sizeof(client_sin));
			client_sin.sin_family = AF_INET;
			client_sin.sin_addr = *((struct in_addr *)he->h_addr);
			client_sin.sin_port = htons(DST_PORT);

			// set non-blocking socket
			// int flag = fcntl(sock_fd[i], F_GETFL, 0);
			// fcntl(sock_fd[i], F_SETFL, flag | O_NONBLOCK);

			// status[i] = F_CONNECTING;
			if ( connect(sock_fd, (struct sockaddr *)&client_sin, sizeof(client_sin)) < 0) {
				perror("connect");
				exit(1);
			}

			FD_SET(sock_fd, &a_rfds);
			FD_SET(srcfd, &a_rfds);

			while (true) {
				memcpy(&rfds, &a_rfds, sizeof(rfds));

				if ( select(nfds, &rfds, NULL, NULL, NULL) < 0) {
					perror("select");
					exit(1);
				}

				if (FD_ISSET(sock_fd, &rfds)) {
					memset(buf, 0, sizeof(buf));
					int len = read(sock_fd, buf, sizeof(buf));
					if (len == 0)
						break;
					write(srcfd, buf, len);
				}

				if (FD_ISSET(srcfd, &rfds)) {
					memset(buf, 0, sizeof(buf));
					int len = read(srcfd, buf, sizeof(buf));
					if (len == 0)
						break;
					write(sock_fd, buf, len);
				}
			}
		}
		else if (CD == 2) { // BIND mode
			printf("SOCKS BIND mode permitted\n");
			if ( (sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
			{
				perror("socket");
				exit(1);
			}

			int optval = 1;
			setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

			memset(&client_sin, 0, sizeof(client_sin));
			client_sin.sin_family = AF_INET;
			client_sin.sin_addr.s_addr = htonl(INADDR_ANY);
			client_sin.sin_port = htons(BIND_PORT);

			if ( bind(sock_fd, (struct sockaddr*)&client_sin, sizeof(client_sin)) < 0)
			{
				perror("bind");
				exit(1);
			}

			listen(sock_fd, MAX_CLIENT);


			reply[0] = 0;
			reply[1] = (unsigned char) 90 ;
			reply[2] = BIND_PORT / 256;
			reply[3] = BIND_PORT % 256;
			reply[4] = 0 ;
			// ip = ip in SOCKS4_REQUEST for connect mode
			// ip = 0 for bind mode
			reply[5] = 0;
			reply[6] = 0;
			reply[7] = 0;
			write(srcfd, reply, 8);


			// set non-blocking socket
			// int flag = fcntl(sock_fd[i], F_GETFL, 0);
			// fcntl(sock_fd[i], F_SETFL, flag | O_NONBLOCK);

			// status[i] = F_CONNECTING;
			int newfd;
			socklen_t slen = sizeof(cli_addr);
			newfd = accept(sock_fd, (struct sockaddr*)&cli_addr, &slen);
			if (newfd < 0) {
				perror("accept");
				exit(1);
			}

			write(srcfd, reply, 8);

			FD_SET(newfd, &a_rfds);
			FD_SET(srcfd, &a_rfds);

			while (true) {
				memcpy(&rfds, &a_rfds, sizeof(rfds));

				if ( select(nfds, &rfds, NULL, NULL, NULL) < 0) {
					perror("select");
					exit(1);
				}

				if (FD_ISSET(newfd, &rfds)) {
					memset(buf, 0, sizeof(buf));
					int len = read(newfd, buf, sizeof(buf));
					if (len == 0)
						break;
					write(srcfd, buf, len);
				}

				if (FD_ISSET(srcfd, &rfds)) {
					memset(buf, 0, sizeof(buf));
					int len = read(srcfd, buf, sizeof(buf));
					if (len == 0)
						break;
					write(newfd, buf, len);
				}
			}
		}
	}
	else {
		printf("SOCKS connection rejected\n");
		reply[0] = 0;
		reply[1] = (unsigned char) 91; // 90 or 91
		reply[2] = DST_PORT / 256;
		reply[3] = DST_PORT % 256;
		reply[4] = DST_IP >> 24 ;
		// ip = ip in SOCKS4_REQUEST for connect mode
		// ip = 0 for bind mode
		reply[5] = (DST_IP >> 16) & 0xFF;
		reply[6] = (DST_IP >> 8)  & 0xFF;
		reply[7] = DST_IP & 0xFF;
		write(srcfd, reply, 8);
	}

}

int main(int argc, char** argv, char** envp)
{

	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s PORT\n", argv[0]);
		exit(1);
	}
	PORT = atoi(argv[1]);

	int sockfd, newfd, childpid;
	socklen_t clilen;
	struct sockaddr_in cli_addr, serv_addr;

	if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("socket");
		exit(1);
	}

	int optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(PORT);

	if ( bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
	{
		perror("bind");
	}

	listen(sockfd, MAX_CLIENT);



	struct sigaction sa;
	sa.sa_handler = &signal_handler;
	sa.sa_flags = SA_RESTART; // Provide behavior compatible with BSD signal semantics by making certain system calls restartable across signals.
	sigfillset(&sa.sa_mask); // block every signal during signal handling

	// def_sa.sa_handler = SIG_DFL;
	// def_sa.sa_flags = SA_RESTART; // Provide behavior compatible with BSD signal semantics by making certain system calls restartable across signals.
	// sigfillset(&def_sa.sa_mask); // block every signal during signal handling

	if (sigaction (SIGCHLD, &sa, NULL) < 0 )
	{
		perror("sigaction SIGCHLD");
		exit(1);
	}


	while (true)
	{
		clilen = sizeof(cli_addr);
		newfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);

		if (newfd < 0) {
			perror("accept");
			exit(1);
		}
		// cout << "accept connection\n";

		if ( (childpid = fork()) < 0)
			perror("fork");
		else if (childpid == 0) // child process
		{
			close(sockfd);
			// close(0);
			// close(1);
			// close(2);
			// dup(newfd);
			// dup(newfd);
			// dup(newfd);
			socks_proxy(newfd);
			// shutdown(newfd, SHUT_RDWR);
			exit(0);
		}
		else // parent process
		{
			close(newfd);
			waitpid(-1, NULL, WNOHANG);
		}
	} // end while


}