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

void cgi(string str, int newfd) {

	string cgi_scipt_name;
	string query_string;

	// get QUERT_STRING
	int found1 = str.find("?");
	if (found1 == string::npos) {
		cgi_scipt_name = str;
		query_string = "";
	}
	else {
		cgi_scipt_name = str.substr(0, found1);
		query_string = str.substr(found1 + 1);
	}
	// cout << "=== " << cgi_scipt_name << " " << query_string << "@" << endl;

	// return status code
	char tmp[3000];
	// return status code
	strcpy(tmp, "HTTP/1.1 200 OK\n");
	write(newfd, tmp, strlen(tmp));
	// print header and blank line, handled by CGI program



	// setup envirionment varaibles for the CGI program
	if (	setenv("QUERY_STRING", query_string.c_str(), 1) < 0 ||
	        setenv("CONTENT_LENGTH", "ANYTHING", 1) < 0 ||
	        setenv("REQUEST_METHOD", "ANYTHING", 1) < 0 ||
	        setenv("SCRIPT_NAME", "ANYTHING", 1) < 0 ||
	        setenv("REMOTE_HOST", "ANYTHING", 1) < 0 ||
	        setenv("REMOTE_ADDR", "ANYTHING", 1) < 0 ||
	        setenv("AUTH_TYPE", "ANYTHING", 1) < 0 ||
	        setenv("REMOTE_USER", "ANYTHING", 1) < 0 ||
	        setenv("REMOTE_IDENT", "ANYTHING", 1) < 0 ||
	        setenv("PATH", string(string(getenv("PATH")) + string(":.")).c_str(), 1) < 0
	   )
	{
		perror("setenv");
		exit(1);
	}

	// exec CGI program
	char **cmd;
	cmd = new char*[2];
	cmd[0] = new char[20];
	strcpy(cmd[0], cgi_scipt_name.c_str());
	cmd[1] = NULL;
	if (execvp(cmd[0], cmd) < 0) {
		perror("execvp");
		exit(1);
	}
}

void send_webpage(string filename, int newfd) {
	char tmp[3000];
	// return status code
	strcpy(tmp, "HTTP/1.1 200 OK\n");
	write(newfd, tmp, strlen(tmp));
	// print header and blank line
	strcpy(tmp, "Content-Type: text/html\n\n");
	write(newfd, tmp, strlen(tmp));
	// print the html file
	FILE* f = fopen(filename.c_str(), "r");
	if ( f == NULL) {
		perror("fopen");
		exit(1);
	}
	while (fgets(tmp, sizeof(tmp), f)) {
		write(newfd, tmp, strlen(tmp));
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

		// read HTTP request
		string str = read_input(newfd);

		// cout << "pid === "<< getpid() << endl;
		cout << "*** " << str << "@" << endl;
		int found1 = str.find_first_of(" ");
		int found2 = str.find_first_of(" ", found1 + 1);
		string cmd = str.substr(found1 + 2, found2 - found1 - 2);

		if (cmd.find(".html") != string:: npos) {
			send_webpage(cmd, newfd);
			close(newfd);
			continue;
		}
		else if (cmd.find(".cgi") != string::npos) { // execute cgi
			if ( (childpid = fork()) < 0)
				perror("fork");
			else if (childpid == 0) // child process
			{
				close(sockfd);
				close(0);
				close(1);
				close(2);
				dup(newfd);
				dup(newfd);
				dup(newfd);
				cgi(cmd, newfd);
				// shutdown(newfd, SHUT_RDWR);
				exit(0);
			}
			else // parent process
			{
				close(newfd);
				waitpid(-1, NULL, WNOHANG);
			}
		}
		else {
			close(newfd);
		}
	} // end while


}