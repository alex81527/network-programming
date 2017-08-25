#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>   // inet_ntop
#include <signal.h>
#include <fcntl.h>       // open, O_WRONLY, O_TRUNC, O_CREAT, etc
#include <unistd.h>      // fork, dup, exec, read/write, getdtablesize, etc
#include <string.h>      // memset
#include <string>        // c++ string
#include <vector>
#include <sstream>       // convert string to stream
#include <iostream>
using namespace std;

const int MAX_CLIENT = 5;
// const int F_CONNECT_READY = 0;
const int F_CONNECTING = 1;
const int F_READING = 2;
const int F_WRITING = 3;
const int F_FINISHED = 4;

int main(void) {
	string hosts[MAX_CLIENT];
	int ports[MAX_CLIENT] = {0};
	string data_str[MAX_CLIENT];
	int sock_fd[MAX_CLIENT];
	FILE *file_fd[MAX_CLIENT];
	int status[MAX_CLIENT];

	// parsing CGI environment variables
	string qstr(getenv("QUERY_STRING"));
	int count = 0, eq = 0, ampersand = 0;
	while (count < 3 * MAX_CLIENT) {
		eq = qstr.find_first_of("=", ampersand + 1);
		ampersand = qstr.find_first_of("&", eq + 1);
		if (count % 3 == 0) {
			hosts[count / 3] = qstr.substr(eq + 1, ampersand - eq - 1);
		}
		else if (count % 3 == 1 ) {
			ports[count / 3] = atoi(qstr.substr(eq + 1, ampersand - eq - 1).c_str());
		}
		else if (count % 3 == 2 ) {
			if (count == MAX_CLIENT - 1)
				data_str[count / 3] = qstr.substr(eq + 1);
			else
				data_str[count / 3] = qstr.substr(eq + 1, ampersand - eq - 1);
		}
		count++;
	}

	printf("Content-Type: text/html\n\n");
	printf(" \n\
	       <html> \n\
	       <head> \n\
	       <meta http-equiv=\"Content-Type\" content=\"text/html; charset=big5\" /> \n\
	       <title>Network Programming Homework 3</title> \n\
	       </head> \n\
	       <body bgcolor=#336699> \n\
	       <font face=\"Courier New\" size=2 color=#FFFF99> \n\
	       <table width=\"800\" border=\"1\"> \n\
	       <tr> \n\
	       <td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr> \n\
	       <tr> \n\
	       <td valign=\"top\" id=\"m0\"></td><td valign=\"top\" id=\"m1\"></td><td valign=\"top\" id=\"m2\"></td><td valign=\"top\" id=\"m3\"></td><td valign=\"top\" id=\"m4\"></td></tr> \n\
	       </table> \n\
	       ", hosts[0].c_str(), hosts[1].c_str(), hosts[2].c_str(), hosts[3].c_str(), hosts[4].c_str());

	// printf("<script>document.all['m0'].innerHTML += \"****************************************<br>\";</script>");
	printf(" \n\
		.... \n\
	    </font> \n\
	    </body> \n\
	    </html> \n\
	    ");
	fflush(stdout);
	// char buf[65536];
	// int fd = open("test.html", O_RDONLY, 0666);
	// int bytes = read(fd, buf, 65536);
	// write(1, buf, bytes);

	struct hostent *he;
	struct sockaddr_in  client_sin;
	int nfds = getdtablesize();
	fd_set rfds, a_rfds; // read fds, active read fds
	fd_set wfds, a_wfds; // write fds, active write fds
	int err, n, conn = 0, exit_flag[MAX_CLIENT];
	int read_bytes, write_bytes, needwrite_bytes[MAX_CLIENT];
	char buf[65536];
	char *s[MAX_CLIENT];
	size_t len[MAX_CLIENT];

	FD_ZERO(&a_rfds);
	FD_ZERO(&a_wfds);

	for (int i = 0; i < MAX_CLIENT; ++i)
	{
		// initialization
		exit_flag[i] = 0;
		needwrite_bytes[i] = 0;
		s[i] = NULL;
		len[i] = 0;

		if (!hosts[i].empty()) {
			if ((he = gethostbyname(hosts[i].c_str())) == NULL) {
				perror("gethostbyname");
				exit(1);
			}

			if ((sock_fd[i] = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
				perror("socket");
				exit(1);
			}

			memset(&client_sin, 0, sizeof(client_sin));
			client_sin.sin_family = AF_INET;
			client_sin.sin_addr = *((struct in_addr *)he->h_addr);
			client_sin.sin_port = htons(ports[i]);

			// set non-blocking socket
			int flag = fcntl(sock_fd[i], F_GETFL, 0);
			fcntl(sock_fd[i], F_SETFL, flag | O_NONBLOCK);

			status[i] = F_CONNECTING;
			if ( connect(sock_fd[i], (struct sockaddr *)&client_sin, sizeof(client_sin)) < 0) {
				printf("<script>document.all['m%d'].innerHTML += \"sock%d connect fail: %s<br>\";</script>\n", i, i, strerror(errno));
				fflush(stdout);
				if (errno != EINPROGRESS) {
					perror("connect");
					exit(1);
				}
			}


			// if ( getsockopt(sock_fd[i], SOL_SOCKET, SO_ERROR, &err, (socklen_t *)&n) != -1 && err == 0 ) {
			// 	status[i] = F_READING;
			// 	FD_CLR(sock_fd[i], &a_wfds);
			// }

			if ( (file_fd[i] = fopen(data_str[i].c_str(), "r")) == NULL ) {
				perror("fopen");
				exit(1);
			}

			FD_SET(sock_fd[i], &a_rfds);
			FD_SET(sock_fd[i], &a_wfds);
			conn++;
			// printf("host%d: %s port: %d data: %s<br>", i, hosts[i].c_str(), ports[i], data_str[i].c_str());
			// printf("<script>document.all['m1'].innerHTML += \"****<br>\";</script>");
			// fflush(stdout);
		}
	}

	while (conn > 0) {

		memcpy(&rfds, &a_rfds, sizeof(rfds));
		memcpy(&wfds, &a_wfds, sizeof(wfds));

		if ( select(nfds, &rfds, &wfds, NULL, NULL) < 0) {
			perror("select");
			exit(1);
		}

		// printf("<script>document.all['m0'].innerHTML += \"after select<br>\";</script>");
		// fflush(stdout);

		for (int i = 0; i < MAX_CLIENT; ++i)
		{
			if (!hosts[i].empty()) {

				if (status[i] == F_CONNECTING && (FD_ISSET(sock_fd[i], &rfds) || FD_ISSET(sock_fd[i], &wfds)) ) {
					if ( getsockopt(sock_fd[i], SOL_SOCKET, SO_ERROR, &err, (socklen_t *)&n) < 0 || err != 0 ) {
						perror("getsockopt");
						printf("<script>document.all['m%d'].innerHTML += \"sock%d connect fail: %s<br>\";</script>\n", i, i, strerror(errno));
						fflush(stdout);
						exit(1);
					}
					status[i] = F_READING;
					// FD_CLR(sock_fd[i], &a_wfds);
				}
				else if (status[i] == F_READING && FD_ISSET(sock_fd[i], &rfds)) { // scan for readable fds

					if (exit_flag[i] == 1) {
						status[i] = F_FINISHED;
					}

					memset(buf, 0, 65536);
					// while (read(sock_fd[i], buf, 65536) > 0) { // keep reading until the cmd prompt shows, i.e., "% " shows up
					read(sock_fd[i], buf, 65536);
					// send to http server
					// printf("<script>document.all['m%d'].innerHTML += \"Reading<br>\";</script>\n", i);
					// printf("<script>document.all['m0'].innerHTML += \"%s<br>\";</script>", buf);
					char *p = NULL;
					p = strtok(buf, "\r\n");
					while (p != NULL) {
						int len = strlen(p), offset = 0;
						char modified_str[len + 100];
						memset(modified_str, 0, len + 100);
						for (int j = 0; j < len; ++j)
						{
							if (p[j] == '"') {
								modified_str[j + offset] = '\\';
								modified_str[j + offset + 1] = '"';
								offset += 1;
							}
							else if (p[j] == '>') {
								modified_str[j + offset] = '&';
								modified_str[j + offset+1] = 'g';
								modified_str[j + offset+2] = 't';
								offset += 2;
							}
							else if (p[j] == '<') {
								modified_str[j + offset] = '&';
								modified_str[j + offset+1] = 'l';
								modified_str[j + offset+2] = 't';
								offset += 2;
							}
							else {
								modified_str[j + offset] = p[j];
							}
						}

						if (modified_str[0] == '%' && modified_str[1] == ' ') {
							if (strlen(modified_str) == 2)
								printf("<script>document.all['m%d'].innerHTML += \"%s\";</script>\n", i, modified_str);
							else
								printf("<script>document.all['m%d'].innerHTML += \"%s<br>\";</script>\n", i, modified_str);

							// printf("<script>document.all['m%d'].innerHTML += \"SHELL PROMPT FOUND<br>\";</script>\n", i);
							// fflush(stdout);
							status[i] = F_WRITING;
							// FD_CLR(sock_fd[i], &a_rfds);
							// FD_SET(sock_fd[i], &a_wfds);
						}
						else {
							printf("<script>document.all['m%d'].innerHTML += \"%s<br>\";</script>\n", i, modified_str);
						}
						fflush(stdout);
						p = strtok(NULL, "\r\n");
					}
					// printf("<script>document.all['m%d'].innerHTML += \"Reading over<br>\";</script>\n", i);
					// memset(buf, 0, 65536);
					// }
				}
				else if (status[i] == F_WRITING && FD_ISSET(sock_fd[i], &wfds)) { // scan for writable fds

					if (needwrite_bytes[i] == 0 ) { // needwrite == 0, means to read a new line from the file

						// handle some characters to correctly show in html, e.g., '\n' --> <br>, '>' --> &gt
						if ( getline(&s[i], &len[i], file_fd[i]) != -1) {

							char send_str[strlen(s[i]) + 100];
							int offset = 0;
							for (int j = 0; j < strlen(s[i]); ++j)
							{
								if (s[i][j] == '>') {
									send_str[j + offset] = '&';
									send_str[j + offset + 1] = 'g';
									send_str[j + offset + 2] = 't';
									offset += 2;
								}
								else if (s[i][j] == '<') {
									send_str[j + offset] = '&';
									send_str[j + offset + 1] = 'l';
									send_str[j + offset + 2] = 't';
									offset += 2;
								}
								else if (s[i][j] == '\n' || s[i][j] == '\r') {
									send_str[j + offset] = '<';
									send_str[j + offset + 1] = 'b';
									send_str[j + offset + 2] = 'r';
									send_str[j + offset + 3] = '>';
									send_str[j + offset + 4] = '\0';
									break;
								}
								else {
									send_str[j + offset] = s[i][j];
								}
							}

							if (!strcmp(send_str, "exit<br>")) {
								exit_flag[i] = 1;
							}

							// send to http server
							// printf("<script>document.all['m%d'].innerHTML += \"Writing<br>\";</script>\n", i);
							printf("<script>document.all['m%d'].innerHTML += \"<b>%s</b>\";</script>\n", i, send_str);
							// printf("<script>document.all['m%d'].innerHTML += \"<b>s=%s</b>\";</script>\n", i, s);
							fflush(stdout);

							needwrite_bytes[i] =  strlen(s[i]);
							// send cmd to backend servers to retrieve data
							int w = write(sock_fd[i], s[i], strlen(s[i]));

							if (w >= 0)
								needwrite_bytes[i] -= w;
						}
					}
					else {
						// send the rest of the cmd to backend servers to retrieve data
						int w = write(sock_fd[i], s + strlen(s[i]) - needwrite_bytes[i] , needwrite_bytes[i]);

						if (w >= 0)
							needwrite_bytes[i] -= w;
					}

					if (needwrite_bytes[i] <= 0) {
						status[i] = F_READING;
						// FD_CLR(sock_fd[i], &a_wfds);
						// FD_SET(sock_fd[i], &a_rfds);
					}
				}
				else if (status[i] == F_FINISHED) {
					FD_CLR(sock_fd[i], &a_rfds);
					FD_CLR(sock_fd[i], &a_wfds);
					close(sock_fd[i]);
					fclose(file_fd[i]);
					conn--;
					hosts[i] = "";

					// printf("<script>document.all['m%d'].innerHTML += \"sock%d DONE, conn = %d<br>\";</script>\n", i, i, conn);
					// fflush(stdout);
				}
			}
		}
	}

	// printf(" \
	// 	.... \
	//     </font> \
	//     </body> \
	//     </html> \
	//     ");
	// fflush(stdout);

	return 0;
}
