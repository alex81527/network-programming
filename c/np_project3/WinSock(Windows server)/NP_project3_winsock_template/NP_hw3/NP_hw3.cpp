#include <windows.h>
#include <list>
#include <iostream>
#include <stdio.h>
#include <string>

using namespace std;

#include "resource.h"

#define SERVER_PORT 56545

#define WM_SOCKET_HTTP (WM_USER + 1)
#define WM_SOCKET_CGI (WM_USER + 2)

BOOL CALLBACK MainDlgProc(HWND, UINT, WPARAM, LPARAM);
int EditPrintf(HWND, TCHAR *, ...);
string read_input(int newfd);
void send_webpage(string filename, int newfd);
void cgi(string str, int newfd, HWND hwndEdit) ;
//=================================================================
//	Global Variables
//=================================================================
list<SOCKET> Socks;
const int MAX_CLIENT = 5;
const int F_CONNECTING = 1;
const int F_READING = 2;
const int F_WRITING = 3;
const int F_FINISHED = 4;


char tmp[65536];
int start_sending[MAX_CLIENT] = {0};
string hosts[MAX_CLIENT];
int ports[MAX_CLIENT] = {0};
string data_str[MAX_CLIENT];
SOCKET sock_fd[MAX_CLIENT];
FILE *file_fd[MAX_CLIENT];
int status[MAX_CLIENT];
int conn = 0, exit_flag[MAX_CLIENT];
int read_bytes, write_bytes, needwrite_bytes[MAX_CLIENT];
char buf[65536];
char s[MAX_CLIENT][65536];


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{

	return DialogBox(hInstance, MAKEINTRESOURCE(ID_MAIN), NULL, MainDlgProc);
}

BOOL CALLBACK MainDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	WSADATA wsaData;

	static HWND hwndEdit;
	static SOCKET msock, ssock;
	static struct sockaddr_in sa;

	int err;


	switch (Message)
	{
	case WM_INITDIALOG:
		hwndEdit = GetDlgItem(hwnd, IDC_RESULT);
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case ID_LISTEN:

			WSAStartup(MAKEWORD(2, 0), &wsaData);

			//create master socket
			msock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

			if (msock == INVALID_SOCKET ) {
				EditPrintf(hwndEdit, TEXT("=== Error: create socket error ===\r\n"));
				WSACleanup();
				return TRUE;
			}

			err = WSAAsyncSelect(msock, hwnd, WM_SOCKET_HTTP, FD_ACCEPT | FD_CLOSE | FD_READ | FD_WRITE);

			if (err == SOCKET_ERROR ) {
				EditPrintf(hwndEdit, TEXT("=== Error: select error ===\r\n"));
				closesocket(msock);
				WSACleanup();
				return TRUE;
			}

			//fill the address info about server
			sa.sin_family = AF_INET;
			sa.sin_port = htons(SERVER_PORT);
			sa.sin_addr.s_addr = INADDR_ANY;

			//bind socket
			err = bind(msock, (LPSOCKADDR)&sa, sizeof(struct sockaddr));

			if (err == SOCKET_ERROR) {
				EditPrintf(hwndEdit, TEXT("=== Error: binding error ===\r\n"));
				WSACleanup();
				return FALSE;
			}

			err = listen(msock, 2);

			if (err == SOCKET_ERROR) {
				EditPrintf(hwndEdit, TEXT("=== Error: listen error ===\r\n"));
				WSACleanup();
				return FALSE;
			}
			else {
				EditPrintf(hwndEdit, TEXT("=== Server START ===\r\n"));
			}

			break;
		case ID_EXIT:
			EndDialog(hwnd, 0);
			break;
		};
		break;

	case WM_CLOSE:
		EndDialog(hwnd, 0);
		break;

	case WM_SOCKET_HTTP:
		switch (WSAGETSELECTEVENT(lParam))
		{
		case FD_ACCEPT:
			ssock = accept(msock, NULL, NULL);
			Socks.push_back(ssock);
			EditPrintf(hwndEdit, TEXT("=== Accept one new client(%d), List size:%d ===\r\n"), ssock, Socks.size());
			break;
		case FD_READ: {
			//Write your code for read event here.

			// read HTTP request
			string str = read_input(wParam);
			//EditPrintf(hwndEdit, TEXT("*** %s@"), str.c_str());
			// cout << "pid === "<< getpid() << endl;
			//cout << "*** " << str.c_str() << "@" << endl;
			int found1 = str.find_first_of(" ");
			int found2 = str.find_first_of(" ", found1 + 1);
			string cmd = str.substr(found1 + 2, found2 - found1 - 2);
			//EditPrintf(hwndEdit, TEXT("*** %s@"), cmd.c_str());

			if (cmd.find(".html") != string::npos) {
				send_webpage(cmd, wParam);
				closesocket(wParam);
				//continue;
			}
			else if (cmd.find(".cgi") != string::npos) {
				string cgi_scipt_name;
				string query_string;

				// get QUERT_STRING
				int found1 = cmd.find("?");
				if (found1 == string::npos) {
					cgi_scipt_name = cmd;
					query_string = "";
				}
				else {
					cgi_scipt_name = cmd.substr(0, found1);
					query_string = cmd.substr(found1 + 1);
				}

				/************************parsing query_string*************************/
				string qstr = query_string;
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

				/************************create socket*************************/
				struct hostent *he;
				struct sockaddr_in  client_sin;

				for (int i = 0; i < MAX_CLIENT; ++i)
				{
					// initialization
					exit_flag[i] = 0;
					needwrite_bytes[i] = 0;

					if (!hosts[i].empty()) {
						if ((he = gethostbyname(hosts[i].c_str())) == NULL) {
							EditPrintf(hwndEdit, TEXT("=== Error: gethostbyname: ===\r\n"));
							//perror("gethostbyname");
							exit(1);
						}

						WSADATA wsaData2;
						WSAStartup(MAKEWORD(2, 2), &wsaData2);
						//WSAStartup(MAKEWORD(2, 0), &wsaData);

						sock_fd[i] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

						if (sock_fd[i] == INVALID_SOCKET ) {
							EditPrintf(hwndEdit, TEXT("=== Error: create socket error ===\r\n"));
							WSACleanup();
							return true;
						}

						err = WSAAsyncSelect(sock_fd[i], hwnd, WM_SOCKET_CGI, FD_CONNECT | FD_CLOSE | FD_READ | FD_WRITE);

						if (err == SOCKET_ERROR ) {
							EditPrintf(hwndEdit, TEXT("=== Error: select error ===\r\n"));
							closesocket(sock_fd[i]);
							WSACleanup();
							return true;
						}

						memset(&client_sin, 0, sizeof(client_sin));
						client_sin.sin_family = AF_INET;
						client_sin.sin_addr = *((struct in_addr *)he->h_addr);
						client_sin.sin_port = htons(ports[i]);

						// status[i] = F_WRITING;
						err = connect(sock_fd[i], (struct sockaddr *)&client_sin, sizeof(client_sin));

						/*
						if (err == SOCKET_ERROR ) {
							EditPrintf(hwndEdit, TEXT("=== Error: connect error ===\r\n"));
							closesocket(sock_fd[i]);
							WSACleanup();
							return true;
						}*/


						if ( (file_fd[i] = fopen(data_str[i].c_str(), "r")) == NULL ) {
							EditPrintf(hwndEdit, TEXT("=== Error: fopen ===\r\n"));
							exit(1);
						}

						conn++;
					}
				}

				// return status code
				strcpy(tmp, "HTTP/1.1 200 OK\n");
				send(wParam, tmp, strlen(tmp), 0);
				strcpy(tmp, "Content-Type: text/html\n\n");
				send(wParam, tmp, strlen(tmp), 0);
				snprintf(tmp, sizeof(tmp), " \n\
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
  \n\
.... \n\
</font> \n\
</body> \n\
</html> \n\
", hosts[0].c_str(), hosts[1].c_str(), hosts[2].c_str(), hosts[3].c_str(), hosts[4].c_str());
				send(wParam, tmp, strlen(tmp), 0);
				/**************************  CGI PROGRAM  **************************/
			}

			break;
		}
		case FD_WRITE:
			//Write your code for write event here

			break;
		case FD_CLOSE:
			break;
		};
		break;

	case WM_SOCKET_CGI:
		switch (WSAGETSELECTEVENT(lParam))
		{
		case FD_CONNECT: {
			EditPrintf(hwndEdit, TEXT("=== SOCKET CONNECTED ===\r\n"));
			break;
		}
		case FD_READ: {
			//Write your code for read event here.
			EditPrintf(hwndEdit, TEXT("=== FD_READ ===\r\n"));
			for (int i = 0; i < MAX_CLIENT; ++i)
			{
				if (!hosts[i].empty() && wParam == sock_fd[i]) {
					// if (status[i] != F_READING)
					// 	return false;

					memset(buf, 0, 65536);
					int r = recv(sock_fd[i], buf, 65536, 0);
					// while (r != SOCKET_ERROR && r > 0) {
					// 	// strcat(buf, "\r\n");
					//EditPrintf(hwndEdit, TEXT(buf));
					// 	memset(buf, 0, 65536);
					// 	r = recv(sock_fd[i], buf, 65536, 0);
					// }
					// buf[r] = '\0';
					// for (int k = 0; k < strlen(buf); ++k)
					// {
					// 	EditPrintf(hwndEdit, TEXT("%c\r\n", buf[k]));
					// }

					// if (exit_flag[i] == 1) {
					// 	snprintf(tmp, sizeof(tmp), "<script>document.all['m%d'].innerHTML += \"%s<br>\";</script>\n", i, buf);
					// 	send(ssock, tmp, strlen(tmp), 0);
					// 	return false;
					// }

					char *p = NULL;
					p = strtok(buf, "\r\n");
					while (p != NULL) {
						int len = strlen(p), offset = 0;
						char modified_str[65536];
						memset(modified_str, 0, 65536);
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

						if (string(modified_str).find("% ") != string::npos) {
							if (strlen(modified_str) == 2) {
								snprintf(tmp, sizeof(tmp), "<script>document.all['m%d'].innerHTML += \"%s\";</script>\n", i, modified_str);
								send(ssock, tmp, strlen(tmp), 0);
							}
							else {
								snprintf(tmp, sizeof(tmp), "<script>document.all['m%d'].innerHTML += \"%s<br>\";</script>\n", i, modified_str);
								send(ssock, tmp, strlen(tmp), 0);
							}

							// printf("<script>document.all['m%d'].innerHTML += \"SHELL PROMPT FOUND<br>\";</script>\n", i);
							// fflush(stdout);
							status[i] = F_WRITING;
							// FD_CLR(sock_fd[i], &a_rfds);
							// FD_SET(sock_fd[i], &a_wfds);

							if ( fgets(s[i], 65536, file_fd[i]) != NULL) {
								//EditPrintf(hwndEdit, TEXT(s[i]));

								char send_str[65536];
								int offset = 0;
								for (unsigned int j = 0; j < strlen(s[i]); ++j)
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
								snprintf(tmp, sizeof(tmp), "<script>document.all['m%d'].innerHTML += \"<b>%s</b>\";</script>\n", i, send_str);
								send(ssock, tmp, strlen(tmp), 0);
								// printf("<script>document.all['m%d'].innerHTML += \"<b>s=%s</b>\";</script>\n", i, s);
								// fflush(stdout);

								needwrite_bytes[i] =  strlen(s[i]);
								// send cmd to backend servers to retrieve data
								int w = send(sock_fd[i], s[i], strlen(s[i]), 0);

								if (w >= 0)
									needwrite_bytes[i] -= w;
							}

						}
						else {
							snprintf(tmp, sizeof(tmp), "<script>document.all['m%d'].innerHTML += \"%s<br>\";</script>\n", i, modified_str);
							send(ssock, tmp, strlen(tmp), 0);
						}
						p = strtok(NULL, "\r\n");
					}

					break;
				}
			}


			break;
		}
		case FD_WRITE:
			//Write your code for write event here
			EditPrintf(hwndEdit, TEXT("=== FD_WRITE ===\r\n"));

			// for (int i = 0; i < MAX_CLIENT; ++i)
			// {
			// 	if (!hosts[i].empty() && wParam == sock_fd[i]) {
			// 		if (start_sending[i] == 0)
			// 			return false;

			// 		if (needwrite_bytes[i] == 0 ) { // needwrite == 0, means to read a new line from the file

			// 			// handle some characters to correctly show in html, e.g., '\n' --> <br>, '>' --> &gt
			// 			if ( fgets(s[i], 3000, file_fd[i]) != NULL) {
			// 				EditPrintf(hwndEdit, TEXT(s[i]));

			// 				char send_str[3000];
			// 				int offset = 0;
			// 				for (unsigned int j = 0; j < strlen(s[i]); ++j)
			// 				{
			// 					if (s[i][j] == '>') {
			// 						send_str[j + offset] = '&';
			// 						send_str[j + offset + 1] = 'g';
			// 						send_str[j + offset + 2] = 't';
			// 						offset += 2;
			// 					}
			// 					else if (s[i][j] == '<') {
			// 						send_str[j + offset] = '&';
			// 						send_str[j + offset + 1] = 'l';
			// 						send_str[j + offset + 2] = 't';
			// 						offset += 2;
			// 					}
			// 					else if (s[i][j] == '\n' || s[i][j] == '\r') {
			// 						send_str[j + offset] = '<';
			// 						send_str[j + offset + 1] = 'b';
			// 						send_str[j + offset + 2] = 'r';
			// 						send_str[j + offset + 3] = '>';
			// 						send_str[j + offset + 4] = '\0';
			// 						break;
			// 					}
			// 					else {
			// 						send_str[j + offset] = s[i][j];
			// 					}
			// 				}

			// 				if (!strcmp(send_str, "exit<br>")) {
			// 					exit_flag[i] = 1;
			// 				}

			// 				// send to http server
			// 				// printf("<script>document.all['m%d'].innerHTML += \"Writing<br>\";</script>\n", i);
			// 				snprintf(tmp, sizeof(tmp), "<script>document.all['m%d'].innerHTML += \"<b>%s</b>\";</script>\n", i, send_str);
			// 				send(ssock, tmp, strlen(tmp), 0);
			// 				// printf("<script>document.all['m%d'].innerHTML += \"<b>s=%s</b>\";</script>\n", i, s);
			// 				// fflush(stdout);

			// 				needwrite_bytes[i] =  strlen(s[i]);
			// 				// send cmd to backend servers to retrieve data
			// 				int w = send(sock_fd[i], s[i], strlen(s[i]), 0);

			// 				if (w >= 0)
			// 					needwrite_bytes[i] -= w;
			// 			}
			// 		}
			// 		else {
			// 			// send the rest of the cmd to backend servers to retrieve data
			// 			int w = send(sock_fd[i], s[i] + strlen(s[i]) - needwrite_bytes[i] , needwrite_bytes[i], 0);

			// 			if (w >= 0)
			// 				needwrite_bytes[i] -= w;
			// 		}

			// 		if (needwrite_bytes[i] <= 0) {
			// 			status[i] = F_READING;
			// 			// FD_CLR(sock_fd[i], &a_wfds);
			// 			// FD_SET(sock_fd[i], &a_rfds);
			// 		}
			// 	}
			// }

			break;
		case FD_CLOSE:
			EditPrintf(hwndEdit, TEXT("=== FD_CLOSE ===\r\n"));
			for (int i = 0; i < MAX_CLIENT; ++i)
			{
				if (!hosts[i].empty() && wParam == sock_fd[i]) {
					hosts[i]="";
					closesocket(sock_fd[i]);
					WSACleanup();
					break;
				}
			}

			int all_closed = 1;
			for (int i = 0; i < MAX_CLIENT; ++i)
			{
				if (!hosts[i].empty()) {
					all_closed = 0;
					break;
				}
			}

			if (all_closed==1) {
				EditPrintf(hwndEdit, TEXT("=== SOCKET CLOSE ===\r\n"));
				closesocket(ssock);
				// WSACleanup();
			}

			break;
		};
		break;
	default:
		return FALSE;


	};

	return TRUE;
}

void cgi(string qstr, int newfd, HWND hwndEdit) {




}

void send_webpage(string filename, int newfd) {
	char tmp[65536];
// return status code
	strcpy(tmp, "HTTP/1.1 200 OK\n");
	send(newfd, tmp, strlen(tmp), 0);
// print header and blank line
	strcpy(tmp, "Content-Type: text/html\n\n");
	send(newfd, tmp, strlen(tmp), 0);
// print the html file
	FILE* f = fopen(filename.c_str(), "r");
	if (f == NULL) {
		perror("fopen");
		//exit(1);
	}
	while (fgets(tmp, sizeof(tmp), f)) {
		send(newfd, tmp, strlen(tmp), 0);
	}
}

string read_input(int newfd) {
	char buf[65536];
	memset(buf, 0, sizeof(buf));
	string s = "";
	while (true) {
		int len = recv(newfd, buf, sizeof(buf), 0);

		if (buf[len - 1] == '\n' || (int)buf[len - 1] == 10 || (int)buf[len - 1] == 13) {
			s += string(buf);
			return s;
		}
		else {
			s += string(buf);
		}
	}
}

int EditPrintf(HWND hwndEdit, TCHAR * szFormat, ...)
{
	TCHAR   szBuffer[1024];
	va_list pArgList;

	va_start(pArgList, szFormat);
	wvsprintf(szBuffer, szFormat, pArgList);
	va_end(pArgList);

	SendMessage(hwndEdit, EM_SETSEL, (WPARAM) - 1, (LPARAM) - 1);
	SendMessage(hwndEdit, EM_REPLACESEL, FALSE, (LPARAM)szBuffer);
	SendMessage(hwndEdit, EM_SCROLLCARET, 0, 0);
	return SendMessage(hwndEdit, EM_GETLINECOUNT, 0, 0);
}