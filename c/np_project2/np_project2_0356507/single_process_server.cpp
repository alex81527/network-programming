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
// #include "my_server.h"

using namespace std;
const char* SHELL_PROMPT = "% ";
const char* WELCOME = "****************************************\n"
                      "** Welcome to the information server. **\n"
                      "****************************************\n";
int PORT = 65005;
const int MAX_CLIENT = 30;
const int MSG_LEN = 200;
const int MAX_PUBLIC_PIPE = 100;

class Numbered_pipe {
public:
    Numbered_pipe(int fd_r_ = -1, int fd_w_ = -1, int count_ = -1) {
        fd_r = fd_r_;
        fd_w = fd_w_;
        count = count_;
    }
    int getfdr() { return fd_r;}
    int getfdw() { return fd_w;}
    int getcnt() { return count;}
    void setfdr(int fd_) { fd_r = fd_;}
    void setfdw(int fd_) { fd_w = fd_;}
    void setcnt(int cnt) { count = cnt;}
    Numbered_pipe& operator= (Numbered_pipe x) {
        setcnt(x.getcnt());
        setfdr(x.getfdr());
        setfdw(x.getfdw());
        return *this;
    }
private:
    int fd_r;
    int fd_w;
    int count;
};

class ID {
public:
    ID(string a = "", string b = "", string c = "", int skt = -1, int d = -1) {
        name = a;
        ip = b;
        port = c ;
        socket = skt;
        id = d;
    }
    string& getname() { return name;}
    string& getip() { return ip;}
    string& getport() { return port;}
    int getsocket() {return socket;}
    int getid() {return id;}
    void setid(int a) { id = a;}
private:
    string name;
    string ip;
    string port;
    int socket;
    int id;
};



char msg[MSG_LEN];
int childpid, pipefd[2][2], cur_pipe[MAX_CLIENT], last_pipe[MAX_CLIENT];
char cmd[15000], cache_data[MAX_CLIENT][65536];
int status, redir_stdout[MAX_CLIENT], redir_stderr[MAX_CLIENT], arg_cnt, angle_bracket, has_to_transfer[MAX_CLIENT], transfer[MAX_CLIENT][2], read_bytes;
vector<string> tmp, tmp2;
vector<int> free_array[MAX_CLIENT]; //tracking how many memories needed to be freed in arg_list
vector<char**> arg_list[MAX_CLIENT];
vector<Numbered_pipe> num_pipe[MAX_CLIENT];
Numbered_pipe redir_stdout_class[MAX_CLIENT], redir_stderr_class[MAX_CLIENT], public_pipe[MAX_PUBLIC_PIPE];

int unknown[MAX_CLIENT] = {0}; // 0 is ok, 1 means something wrong happened in last command


void signal_handler(int signo);
vector<string> split(char* str, char* delim);
int is_contained(vector<Numbered_pipe>& n, int count);
bool contained_slash(char* cmd);
bool is_zero(Numbered_pipe n);
int find_angle_bracket(char ** p, int len);
int find_public_redir_output(char** p, int len);
int find_first_vacant_pos(Numbered_pipe pub[], int num);
int find_pos(Numbered_pipe pub[], int num);
int find_public_redir_intput(char** p, int len);
void free_memory(vector<char**> &arg_list, vector<int> &free_array);
void execute(char** cmd, vector<char**> &arg_list, vector<int> &free_array);
int shell(int fd, vector<ID>& who_list, vector<string>& env_list, int index, string& username, string& ipaddress, string& portnum);
void test();
int get_first_vacant_id(vector<ID>& who_list);
void who(int fd, vector<ID>& who_list);
void tell(int fd, vector<ID>& who_list, int id, const char* msg2, string& username);
void yell(int fd, vector<ID>& who_list, const char* msg2, string& username);
void name(int fd, vector<ID>& who_list, const char* msg2, string& username, string& ipaddress, string& portnum);

void signal_handler(int signo)
{
    switch (signo)
    {
    case SIGCHLD:
        waitpid(-1, NULL, WNOHANG);
        break;
    default:
        break;
    }
}

vector<string> split(char* str, char* delim) {
    vector<string> res;
    char* p = strtok(str, delim);

    while ( p != NULL )
    {
        string s(p);
        res.push_back(s);
        p = strtok(NULL, delim);
    }

    return res;
}

int is_contained(vector<Numbered_pipe>& n, int count) {
    int size = n.size();
    for (int i = 0; i < size ; i++)
    {
        if (n[i].getcnt() == count)
        {
            return i;
        }
    }

    return -1;
}

bool contained_slash(char* cmd) {
    string s(cmd);
    if (s.find("/") != string::npos)
        return true;
    else
        return false;
}

bool is_zero(Numbered_pipe n)
{
    if (n.getcnt() == 0)
        return true;
    else
        return false;
}

int find_angle_bracket(char ** p, int len) {
    for (int i = 0; i < len; i++) {
        if (!strcmp(p[i], ">")) {
            return i;
        }
    }
    return -1;
}

int find_public_redir_output(char** p, int len) {

    for (int i = 0; i < len; ++i)
    {
        string s(p[i]);
        int pos = s.find(">");
        if ( pos != string::npos && strcmp(p[i], ">") != 0) {
            int pos2 = s.find(" \r\n", pos + 1);
            return atoi(s.substr(pos + 1, pos2 - pos).c_str());
        }
    }

    return -1;
}

int find_first_vacant_pos(Numbered_pipe pub[], int num) {

    int first_vacant, on = 1;
    for (int i = 0; i < MAX_PUBLIC_PIPE; ++i)
    {
        if (on == 1 && pub[i].getcnt() == -1) {
            first_vacant = i;
            on = 0;
        }
        if (pub[i].getcnt() == num) {
            return -1;
        }
    }

    return first_vacant;
}

int find_pos(Numbered_pipe pub[], int num) {

    for (int i = 0; i < MAX_PUBLIC_PIPE; ++i)
    {
        if (pub[i].getcnt() == num) {
            return i;
        }
    }

    return -1;
}

int find_public_redir_intput(char** p, int len) {

    for (int i = 0; i < len; ++i)
    {
        string s(p[i]);
        int pos = s.find("<");
        if ( pos != string::npos && strcmp(p[i], "<") != 0) {
            int pos2 = s.find(" \r\n", pos + 1);
            return atoi(s.substr(pos + 1, pos2 - pos).c_str());
        }
    }

    return -1;
}

void free_memory(vector<char**> &arg_list, vector<int> &free_array) {
    for (int k = 0; k < arg_list.size(); k++)
    {
        for (int j = 0; j < free_array[k]; j++)
        {
            delete [] arg_list[k][j];
        }
        delete [] arg_list[k];
    }
}

void execute(char** cmd, vector<char**> &arg_list, vector<int> &free_array) {
    if ( execvp(cmd[0], cmd) < 0)
    {
        if (errno == ENOENT) // No such file or directory
        {
            free_memory(arg_list, free_array);
            exit(2);
        }
        else
            perror("execvp");
    }
    else
    {
        fflush(stdout);
        fflush(stderr);
    }
}

int shell(int fd, vector<ID>& who_list, vector<string>& env_list, int index, string& username, string& ipaddress, string& portnum) {

    // for (int i = 0; i < who_list.size(); ++i)
    // {
    //      code
    //     cout << "shell: " << who_list[i].getid() << endl;
    // }

    string backup_cmd;

    memset(cmd, 0, 15000);

    redir_stdout[index] = -1;
    redir_stderr[index] = -1;
    cur_pipe[index] = 1;

    free_array[index].clear();
    tmp.clear();
    tmp2.clear();
    arg_list[index].clear();

    string s = "";
    while (true) {
        int len = read(fd, cmd, 15000);
        // cout << "cmd : " << cmd << endl;
        // cout << "len : " << len << endl;
        // cout << "cmd[len-1]" << (int)cmd[len - 1] << endl;
        // fflush(stdout);

        if (cmd[len - 1] == '\n' || (int)cmd[len - 1] == 10 || (int)cmd[len - 1] == 13) {
            s += string(cmd);
            strcpy(cmd, s.c_str());

            int found3 = s.find_first_of("\r\n");
            backup_cmd = s.substr(0, found3);
            // backup_cmd[strlen(backup_cmd)-1]='\0';
            break;
        }
        else {
            s += string(cmd);
        }
    }

    int l = strlen(cmd);
    for (int i = 0; i < l; i++)
    {
        if ( (int)cmd[i] == 10 || (int)cmd[i] == 13)
            cmd[i] = '\0';
        // else
        //     printf("%c %d\n", cmd[i], cmd[i]);
    }

    cout << "*** " << cmd << "@" << endl;
    fflush(stdout);

    if (!strcmp(cmd, "") || !strcmp(cmd, "\n") || !strcmp(cmd, "\r\n"))
        return 0;
    else if (!strcmp(cmd, "exit") || !strcmp(cmd, "exit\n") || !strcmp(cmd, "exit\r\n")) {
        return 1;
    }
    else if (contained_slash(cmd))
    {
        const char* warning = "Slash (\"/\") in command is not allowed.\n";
        write(fd, warning, strlen(warning));
        return 0;
    }

    char delim[] = " \r\n";
    tmp = split(cmd, delim);

    for (string z : tmp) {
        size_t exclamation_found = z.find("!");
        size_t pipe_found = z.find("|");

        if (z == "|")
        {
            arg_cnt = tmp2.size();
            char** p = new char*[arg_cnt + 1]; // additional space for terminating NULL
            free_array[index].push_back(arg_cnt);
            for (int j = 0; j < arg_cnt; j++)
            {
                p[j] = new char[tmp2[j].length() + 1];
                strcpy(p[j], tmp2[j].c_str());
                //cout<< j <<" "<<tmp2[j]<<"!"<<endl;
            }
            p[arg_cnt] = NULL;
            arg_list[index].push_back(p);
            tmp2.clear();
        }
        else if ( exclamation_found != string::npos )
        {
            redir_stderr[index] = atoi(z.substr(exclamation_found + 1).c_str());
            //cout << "exclamation found: " << redir_stderr << endl;
        }
        else if ( pipe_found != string::npos )
        {
            redir_stdout[index] = atoi(z.substr(pipe_found + 1).c_str());
            //cout << "pipe found: " << redir_stdout << endl;
        }
        else
        {
            tmp2.push_back(z);
        }
    }

    arg_cnt = tmp2.size();
    char** p = new char*[arg_cnt + 1]; // additional space for terminating NULL
    free_array[index].push_back(arg_cnt);
    for (int j = 0; j < arg_cnt; j++)
    {
        p[j] = new char[tmp2[j].length() + 1];
        strcpy(p[j], tmp2[j].c_str());
        //cout<< j <<" "<<tmp2[j]<<"!"<<endl;
    }
    p[arg_cnt] = NULL;
    arg_list[index].push_back(p);

    //cout << arg_list.size() << endl;
    /*
    for (int j = 0; j < free_array[0] + 1; j++)
    {
        if (arg_list[0][j] == NULL)
            cout << "*** NULL@\n";
        else
            cout << "*** " << arg_list[0][j] << "@" << endl;
        //execute(arg_list[0], pipe_list, arg_list, free_array);
    }*/

    // if something wrong happened, leave the variable as in previous state
    if (unknown[index] == 0)
        has_to_transfer[index] = 0;

    // Step1.
    // decrease every count in num_pipe by 1
    // remove entries with count 0 and open a new pipe to cache/collect those data
    if (num_pipe[index].size() > 0 )
    {
        // if ( pipe(transfer[index]) < 0)
        // {
        //     perror("pipe");
        //     exit(1);
        // }

        if (unknown[index] == 0) {
            int size = num_pipe[index].size();
            for (int i = 0; i < size; i++)
            {
                num_pipe[index][i].setcnt(num_pipe[index][i].getcnt() - 1);
                // cout << " num_pipe: count=" << num_pipe[i].getcnt() << " fd_r=" << num_pipe[i].getfdr() << " fd_w=" << num_pipe[i].getfdw() << endl;
                if (num_pipe[index][i].getcnt() == 0)
                {
                    has_to_transfer[index] = 1;
                    // read_bytes = read(num_pipe[index][i].getfdr(), cache_data[index], sizeof(cache_data[index]));
                    // write(transfer[index][1], cache_data[index], read_bytes);
                    // cout << endl << "read " << read_bytes << " bytes: " << cache_data << endl;
                    transfer[index][0] = num_pipe[index][i].getfdr();
                    transfer[index][1] = num_pipe[index][i].getfdw();
                    close(transfer[index][1]);
                    break;
                }
            }

            // in case of unknown command, e.g. ctt, cache_data serves as a backup
            // if (has_to_transfer[index] == 1)
            // {
            //     read_bytes = read(transfer[index][0], cache_data[index], sizeof(cache_data[index]));
            //     write(transfer[index][1], cache_data[index], read_bytes);
            // }

        }
        // else {
        //     // restore cache_data into pipe
        //     write(transfer[index][1], cache_data[index], read_bytes);
        // }

        // if (has_to_transfer[index] != 1)
        // {
        //     close(transfer[index][0]);
        //     close(transfer[index][1]);
        // }
    }

    // Step2. check if there is fd opened for the count "redir_stdout" or "redir_stderr"
    // if not, create a new one, and push it into num_pipe
    if ( redir_stdout[index] > 0)
    {
        int indexx;
        if ( (indexx = is_contained(num_pipe[index], redir_stdout[index])) < 0) {
            int tmp_fd[2];
            if (pipe(tmp_fd) < 0) {
                perror("pipe");
                exit(1);
            }
            Numbered_pipe c(tmp_fd[0], tmp_fd[1], redir_stdout[index]);
            num_pipe[index].push_back(c);
            redir_stdout_class[index] = c;
        }
        else {
            redir_stdout_class[index] = num_pipe[index][indexx];
        }
    }

    if ( redir_stderr[index] > 0)
    {
        int indexx;
        if ( (indexx = is_contained(num_pipe[index], redir_stderr[index])) < 0) {
            int tmp_fd[2];
            if (pipe(tmp_fd) < 0) {
                perror("pipe");
                exit(1);
            }
            Numbered_pipe c(tmp_fd[0], tmp_fd[1], redir_stderr[index]);
            num_pipe[index].push_back(c);
            redir_stderr_class[index] = c;
        }
        else
            redir_stderr_class[index] = num_pipe[index][indexx];
    }

    /*
            for (Numbered_pipe n : num_pipe)
            {
                cout << " num_pipe: count=" << n.getcnt() << " fd_r=" << n.getfdr() << " fd_w=" << n.getfdw() << endl;
            }

            cout << "transfer: " << has_to_transfer << endl;
            cout << "redir_stdout: " << redir_stdout << endl;
            cout << "redir_stderr: " << redir_stderr << endl;
    */

    // Step3. arrange input pipe, output pipe, and stderr, then execute command
    if (arg_list[index].size() == 1)
    {
        if ( !strcmp(arg_list[index][0][0], "setenv") ) {
            if (!strcmp(arg_list[index][0][1], "PATH"))
                env_list[index] = string(arg_list[index][0][2]);
            free_memory(arg_list[index], free_array[index]);
            return 0;
        }
        else if ( !strcmp(arg_list[index][0][0], "printenv") ) {
            if (!strcmp(arg_list[index][0][1], "PATH")) {
                snprintf(msg, MSG_LEN, "PATH=%s\n", env_list[index].c_str());
                write(fd, msg, strlen(msg));
            }
            free_memory(arg_list[index], free_array[index]);
            return 0;
        }
        else if ( !strcmp(arg_list[index][0][0], "who") ) {
            who(fd, who_list);
            return 0;
        }
        else if ( !strcmp(arg_list[index][0][0], "tell") ) { //msg2 needs modification
            string s = backup_cmd;
            int found = s.find_first_of(" ");
            found = s.find_first_of(" ", found + 1);
            int found2 = s.find_first_not_of(" ", found + 1);

            tell(fd, who_list, atoi(arg_list[index][0][1]), s.substr(found2).c_str(), username);
            return 0;
        }
        else if ( !strcmp(arg_list[index][0][0], "yell") ) { //msg2 needs modification
            string s = backup_cmd;
            int found = s.find_first_of(" ");
            int found2 = s.find_first_not_of(" ", found + 1);

            yell(fd, who_list, s.substr(found2).c_str(), username);
            return 0;
        }
        else if ( !strcmp(arg_list[index][0][0], "name") ) { //msg2 needs modification
            string s = backup_cmd;
            int found = s.find_first_of(" ");
            int found2 = s.find_first_not_of(" ", found + 1);

            name(fd, who_list, s.substr(found2).c_str(), username, ipaddress, portnum);
            return 0;
        }

        angle_bracket = find_angle_bracket(arg_list[index][0], free_array[index][0]);
        int public_redir_out = find_public_redir_output(arg_list[index][0], free_array[index][0]);
        int public_redir_int = find_public_redir_intput(arg_list[index][0], free_array[index][0]);
        int public_redir_out_fdw, public_redir_int_fdr;
        int pos_out, pos_int;

        bool error_public_pipe = false;
        // handle public pipe intput redirection, e.g., "<3"
        if (public_redir_int > 0) {
            pos_int = find_pos(public_pipe, public_redir_int); // return -1 if public_redir_int doesn't exist, otherwise return its index in the array
            if (pos_int < 0) {
                snprintf(msg, MSG_LEN, "*** Error: public pipe #%d does not exist yet. ***\n", public_redir_int);
                write(fd, msg, strlen(msg));
                unknown[index] = 1;
                error_public_pipe = true;
            }
            else {
                public_redir_int_fdr = public_pipe[pos_int].getfdr();

                snprintf(msg, MSG_LEN, "*** %s (#%d) just received via '%s' ***\n", username.c_str(), who_list[index].getid(), backup_cmd.c_str());
                for (int i = 0; i < who_list.size(); ++i)
                {
                    if (who_list[i].getid() != -1)
                        write(who_list[i].getsocket(), msg, strlen(msg));
                }
            }
        }

        // handle public pipe output redirection, e.g., ">5"
        if (public_redir_out > 0) {
            pos_out = find_first_vacant_pos(public_pipe, public_redir_out); // return -1 if public_redir_out already exists, otherwise return first vacant position in the array
            if (pos_out < 0) {
                snprintf(msg, MSG_LEN, "*** Error: public pipe #%d already exists. ***\n", public_redir_out);
                write(fd, msg, strlen(msg));
                unknown[index] = 1;
                error_public_pipe = true;
            }
            else if (!error_public_pipe) {
                int tmp_fd[2];
                if (pipe(tmp_fd) < 0) {
                    perror("pipe");
                    exit(1);
                }
                Numbered_pipe c(tmp_fd[0], tmp_fd[1], public_redir_out);
                public_pipe[pos_out] = c;
                public_redir_out_fdw = tmp_fd[1];

                snprintf(msg, MSG_LEN, "*** %s (#%d) just piped '%s' ***\n", username.c_str(), who_list[index].getid(), backup_cmd.c_str());
                for (int i = 0; i < who_list.size(); ++i)
                {
                    if (who_list[i].getid() != -1)
                        write(who_list[i].getsocket(), msg, strlen(msg));
                }
            }
        }

        if (error_public_pipe)
            return 0;

        if ( (childpid = fork()) < 0)
            perror("fork");
        else if (childpid == 0) // child process
        {
            close(0);
            close(1);
            close(2);
            dup(fd);
            dup(fd);
            dup(fd);

            clearenv();
            if (setenv("PATH", env_list[index].c_str(), 1) < 0)
            {
                perror("setenv");
                exit(1);
            }

            if (has_to_transfer[index] == 1) // read from transfer pipe
            {
                close(0);
                dup(transfer[index][0]);
                // close(transfer[index][1]);
            }

            if (redir_stdout[index] > 0)
            {
                close(1);
                dup(redir_stdout_class[index].getfdw());
            }

            if (redir_stderr[index] > 0)
            {
                close(2);
                dup(redir_stderr_class[index].getfdw());
            }

            // handle public pipe intput redirection, e.g., "<3"
            if (public_redir_int > 0) {
                close(0);
                dup(public_redir_int_fdr);
            }

            // handle public pipe output redirection, e.g., ">5"
            if (public_redir_out > 0) {
                close(1);
                close(2);
                dup(public_redir_out_fdw);
                dup(public_redir_out_fdw);
            }

            if (angle_bracket > 0) {
                char* filename = arg_list[index][0][angle_bracket + 1];

                int fdd;
                if (  (fdd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0)
                    perror("open");

                close(1);
                dup(fdd);
            }

            for (int i = 0; i < free_array[index][0]; ++i)
            {
                if (arg_list[index][0][i] == NULL) // prevent string constructor error in next line, string(NULL)
                    continue;

                string s(arg_list[index][0][i]);
                if (!strcmp(arg_list[index][0][i], ">")) {
                    arg_list[index][0][i] = NULL;
                    arg_list[index][0][i + 1] = NULL;
                }
                else if (s.find_first_of("><") != string::npos)
                    arg_list[index][0][i] = NULL;
            }

            //execvp cmd
            execute(arg_list[index][0], arg_list[index], free_array[index]);
            free_memory(arg_list[index], free_array[index]);
            exit(1);

        }
        else // parent process
        {
            if (has_to_transfer[index] == 1) // has to close the write end, so that child won't hang
            {
                // close(transfer[index][0]);
                // close(transfer[index][1]);
            }

            if (public_redir_out > 0) {
                close(public_redir_out_fdw);
            }

            if (public_redir_int > 0) {
                close(public_redir_int_fdr);
                public_pipe[pos_int].setcnt(-1);
            }

            wait(&status);

            // if (public_redir_out > 0) {
            //     char buf[65536];
            //     int len = read(public_redir_out_fdr, buf, 65536);
            //     int tmp_fd[2];
            //     if (pipe(tmp_fd) < 0) {
            //         perror("pipe");
            //         exit(1);
            //     }
            //     Numbered_pipe c(tmp_fd[0], tmp_fd[1], public_redir_out);
            //     write(tmp_fd[1], buf, len);
            //     public_pipe[pos_out] = c;

            //     close(public_redir_out_fdr);
            // }

            if (WEXITSTATUS(status) == 2)
            {
                unknown[index] = 1;

                snprintf(msg, MSG_LEN, "Unknown command: [%s].\n", arg_list[index][0][0]);
                write(fd, msg, strlen(msg));
                return 0;
            }

            // if it gets here, the cmd has successfully executed
            unknown[index] = 0;
            if (has_to_transfer[index] == 1) // close unneeded pipes, remove entries with count 0
            {
                for (Numbered_pipe n : num_pipe[index])
                {
                    if (n.getcnt() == 0)
                    {
                        close(n.getfdr());
                        // close(n.getfdw());
                    }
                }
                // c++ erase-remove paradigm
                num_pipe[index].erase(remove_if(num_pipe[index].begin(), num_pipe[index].end(), is_zero), num_pipe[index].end());
            }
        }
    }
    else
    {
        int last = arg_list[index].size() - 1;
        angle_bracket = find_angle_bracket(arg_list[index][last], free_array[index][last]);

        for (int i = 0; i < arg_list[index].size(); i++)
        {

            int public_redir_out = find_public_redir_output(arg_list[index][i], free_array[index][i]);
            int public_redir_int = find_public_redir_intput(arg_list[index][i], free_array[index][i]);
            int public_redir_out_fdw, public_redir_int_fdr;
            int pos_out, pos_int;

            bool error_public_pipe = false;
            // handle public pipe intput redirection, e.g., "<3"
            if (public_redir_int > 0) {
                pos_int = find_pos(public_pipe, public_redir_int); // return -1 if public_redir_int doesn't exist, otherwise return its index in the array
                if (pos_int < 0) {
                    snprintf(msg, MSG_LEN, "*** Error: public pipe #%d does not exist yet. ***\n", public_redir_int);
                    write(fd, msg, strlen(msg));
                    if (i == 0)
                        unknown[index] = 1;

                    error_public_pipe = true;
                }
                else {
                    public_redir_int_fdr = public_pipe[pos_int].getfdr();

                    snprintf(msg, MSG_LEN, "*** %s (#%d) just received via '%s' ***\n", username.c_str(), who_list[index].getid(), backup_cmd.c_str());
                    for (int i = 0; i < who_list.size(); ++i)
                    {
                        if (who_list[i].getid() != -1)
                            write(who_list[i].getsocket(), msg, strlen(msg));
                    }
                }
            }

            // handle public pipe output redirection, e.g., ">5"
            if (public_redir_out > 0) {
                pos_out = find_first_vacant_pos(public_pipe, public_redir_out); // return -1 if public_redir_out already exists, otherwise return first vacant position in the array
                if (pos_out < 0) {
                    snprintf(msg, MSG_LEN, "*** Error: public pipe #%d already exists. ***\n", public_redir_out);
                    write(fd, msg, strlen(msg));
                    if (i == 0)
                        unknown[index] = 1;

                    error_public_pipe = true;
                }
                else if (!error_public_pipe) {
                    int tmp_fd[2];
                    if (pipe(tmp_fd) < 0) {
                        perror("pipe");
                        exit(1);
                    }
                    Numbered_pipe c(tmp_fd[0], tmp_fd[1], public_redir_out);
                    public_pipe[pos_out] = c;
                    public_redir_out_fdw = tmp_fd[1];

                    snprintf(msg, MSG_LEN, "*** %s (#%d) just piped '%s' ***\n", username.c_str(), who_list[index].getid(), backup_cmd.c_str());
                    for (int i = 0; i < who_list.size(); ++i)
                    {
                        if (who_list[i].getid() != -1)
                            write(who_list[i].getsocket(), msg, strlen(msg));
                    }
                }
            }

            if (error_public_pipe)
                return 0;

            last_pipe[index] = cur_pipe[index];
            cur_pipe[index] = cur_pipe[index] ? 0 : 1;

            if (pipe(pipefd[cur_pipe[index]]) < 0)
            {
                perror("pipe");
                exit(1);
            }

            if ( (childpid = fork()) < 0)
                perror("fork");
            else if (childpid == 0) // child process
            {
                close(0);
                close(1);
                close(2);
                dup(fd);
                dup(fd);
                dup(fd);

                clearenv();
                if (setenv("PATH", env_list[index].c_str(), 1) < 0)
                {
                    perror("setenv");
                    exit(1);
                }

                if (i == 0)
                {
                    if (has_to_transfer[index] == 1) // read from transfer pipe
                    {
                        close(0);
                        dup(transfer[index][0]);
                        // close(transfer[index][1]);
                    }

                    // handle public pipe intput redirection, e.g., "<3"
                    if (public_redir_int > 0) {
                        close(0);
                        dup(public_redir_int_fdr);
                    }

                    close(1);
                    dup(pipefd[cur_pipe[index]][1]);
                    close(pipefd[cur_pipe[index]][0]);
                } else if (i == arg_list[index].size() - 1) {
                    close(0);
                    dup(pipefd[last_pipe[index]][0]);
                    close(pipefd[cur_pipe[index]][0]);
                    close(pipefd[cur_pipe[index]][1]);

                    if (redir_stdout[index] > 0)
                    {
                        close(1);
                        dup(redir_stdout_class[index].getfdw());
                    }

                    if (redir_stderr[index] > 0)
                    {
                        close(2);
                        dup(redir_stderr_class[index].getfdw());
                    }

                    // handle public pipe output redirection, e.g., ">5"
                    if (public_redir_out > 0) {
                        close(1);
                        close(2);
                        dup(public_redir_out_fdw);
                        dup(public_redir_out_fdw);
                    }

                    // handle redirection ">"
                    if (angle_bracket > 0) {
                        char* filename = arg_list[index][i][angle_bracket + 1];

                        int fdd;
                        if (  (fdd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0)
                            perror("open");

                        close(1);
                        dup(fdd);
                    }
                }
                else {
                    close(0);
                    close(1);
                    dup(pipefd[last_pipe[index]][0]);
                    dup(pipefd[cur_pipe[index]][1]);
                    close(pipefd[cur_pipe[index]][0]);
                }

                for (int j = 0; j < free_array[index][i]; ++j)
                {
                    if (arg_list[index][i][j] == NULL) // prevent string constructor error in next line, string(NULL)
                        continue;

                    string s(arg_list[index][i][j]);
                    if (!strcmp(arg_list[index][i][j], ">")) {
                        arg_list[index][i][j] = NULL;
                        arg_list[index][i][j + 1] = NULL;
                    }
                    else if (s.find_first_of("><") != string::npos)
                        arg_list[index][i][j] = NULL;
                }

                //execvp cmd
                execute(arg_list[index][i], arg_list[index], free_array[index]);
                free_memory(arg_list[index], free_array[index]);
                exit(1);
            }
            else // parent process
            {
                if (has_to_transfer[index] == 1)
                {
                    // close(transfer[index][0]);
                    // close(transfer[index][1]);
                }

                if (redir_stdout[index] > 0)
                {

                }

                if (redir_stderr[index] > 0)
                {

                }

                // parent "must" close the writing end of the pipe, so that the child won't hang there waiting
                close(pipefd[cur_pipe[index]][1]);

                if (public_redir_out > 0) {
                    close(public_redir_out_fdw);
                }

                if (public_redir_int > 0) {
                    close(public_redir_int_fdr);
                    public_pipe[pos_int].setcnt(-1);
                }

                wait(&status);

                if (i != 0)
                    close(pipefd[last_pipe[index]][0]);

                if (WEXITSTATUS(status) == 2)
                {
                    unknown[index] = 1;
                    snprintf(msg, MSG_LEN, "Unknown command: [%s].\n", arg_list[index][i][0]);
                    write(fd, msg, strlen(msg));
                    break;
                }
            }
        } // end for loop

        unknown[index] = 0;
        if (has_to_transfer[index] == 1)
        {
            for (Numbered_pipe n : num_pipe[index])
            {
                if (n.getcnt() == 0)
                {
                    close(n.getfdr());
                    // close(n.getfdw());
                }
            }
            // c++ erase-remove paradigm
            num_pipe[index].erase(remove_if(num_pipe[index].begin(), num_pipe[index].end(), is_zero), num_pipe[index].end());
        }
    }

    free_memory(arg_list[index], free_array[index]);
    return 0;
}

void test() {

    for (int i = 0; i < 510 ; i++)
    {
        int p[510][2];
        if (pipe(p[i]) < 0)
        {
            break;
        }
        cout << i << endl;
    }

    exit(0);

    vector<char**> x;
    char** v = new char*[3];
    v[0] = new char[10];
    v[1] = new char[10];
    v[2] = new char[10];
    strcpy(v[0], "ls");
    strcpy(v[1], "-l");
    v[2] = NULL;
    x.push_back(v);
    execvp(x[0][0], x[0]);
    exit(0);
}

int get_first_vacant_id(vector<ID>& who_list) {
    for (int i = 0; i < who_list.size(); ++i)
    {
        if (who_list[i].getid() == -1) {
            who_list[i].setid(i + 1);
            return i + 1;
        }
        if (i == who_list.size() - 1)
            return -1;
    }
}

void who(int fd, vector<ID>& who_list) {

    snprintf(msg, MSG_LEN, "<ID>\t<nickname>\t<IP/port>\t<indicate me>\n");
    write(fd, msg, strlen(msg));

    for (int i = 0; i < who_list.size(); ++i)  {
        if (who_list[i].getid() != -1) {
            if (who_list[i].getsocket() == fd) {
                snprintf(msg, 200, "%d\t%s\t%s/%s\t<-me\n", who_list[i].getid(), who_list[i].getname().c_str(), who_list[i].getip().c_str(), who_list[i].getport().c_str());
            }
            else {
                snprintf(msg, 200, "%d\t%s\t%s/%s\n", who_list[i].getid(), who_list[i].getname().c_str(), who_list[i].getip().c_str(), who_list[i].getport().c_str());
            }
            write(fd, msg, strlen(msg));
        }
    }
}

void tell(int fd, vector<ID>& who_list, int id, const char* msg2, string& username) {
    for (int i = 0; i < who_list.size(); ++i)  {
        if (who_list[i].getid() == id && id > 0) {
            snprintf(msg, MSG_LEN, "*** %s told you ***: %s\n", username.c_str(), msg2);
            write(who_list[i].getsocket(), msg, strlen(msg));
            return;
        }

        if (i == who_list.size() - 1 ) {
            snprintf(msg, MSG_LEN, "*** Error: user #%d does not exist yet. ***\n", id);
            write(fd, msg, strlen(msg));
        }
    }
}

void yell(int fd, vector<ID>& who_list, const char* msg2, string& username) {
    snprintf(msg, MSG_LEN, "*** %s yelled ***: %s\n", username.c_str(), msg2);
    for (int i = 0; i < who_list.size(); ++i)  {
        if (who_list[i].getid() != -1)
            write(who_list[i].getsocket(), msg, strlen(msg));
    }
}

void name(int fd, vector<ID>& who_list, const char* msg2, string& username, string& ipaddress, string& portnum) {
    string s(msg2);
    bool found = false;
    for (int i = 0; i < who_list.size(); ++i)  {
        if (who_list[i].getid() != -1 && who_list[i].getname() == s) {
            found = true;
            break;
        }
    }

    if (found) {
        snprintf(msg, MSG_LEN, "*** User '%s' already exists. ***\n", msg2);
        write(fd, msg, strlen(msg));
    }
    else {
        username = s;
        snprintf(msg, MSG_LEN, "*** User from %s/%s is named '%s'. ***\n", ipaddress.c_str(), portnum.c_str(), msg2);
        for (int i = 0; i < who_list.size(); ++i)  {
            if (who_list[i].getid() != -1)
                write(who_list[i].getsocket(), msg, strlen(msg));
        }
    }
}

int main(int argc, char** argv, char** envp)
{
    string dir(getenv("HOME"));
    dir += "/ras";

    if (chdir(dir.c_str()) < 0)
    {
        perror("chdir");
        exit(1);
    }

    // if (setenv("PATH", "bin:.", 1) < 0)
    // {
    //     perror("setenv");
    //     exit(1);
    // }

    struct sigaction sa;

    sa.sa_handler = &signal_handler;
    sa.sa_flags = SA_RESTART; // Provide behavior compatible with BSD signal semantics by making certain system calls restartable across signals.
    sigfillset(&sa.sa_mask); // block every signal during signal handling

    // def_sa.sa_handler = SIG_DFL;
    // def_sa.sa_flags = SA_RESTART; // Provide behavior compatible with BSD signal semantics by making certain system calls restartable across signals.
    // sigfillset(&def_sa.sa_mask); // block every signal during signal handling

    if (sigaction (SIGCHLD, &sa, NULL) < 0)
    {
        perror("sigaction SIGCHLD");
    }

    // if (sigaction (SIGINT, &def_sa, NULL) < 0)
    // {
    //     perror("sigaction SIGINT");
    // }

    if (argc < 2)
    {
        fprintf(stderr, "Usage: ./server PORT\n");
        exit(1);
    }
    PORT = atoi(argv[1]);
    //test();
    //struct addrinfo hints, *serverinfo, *p;
    //struct sockaddr_storage their_addr;
    int sockfd, newfd, childpid;
    socklen_t clilen;
    struct sockaddr_in cli_addr, serv_addr;

    if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
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


    int nfds; // max number of fds
    int available_id ; // managing clients' ids
    fd_set rfds, afds; // read fds, active fds
    char ipstr[INET_ADDRSTRLEN];
    vector<ID> who_list;
    vector<string> env_list; // assume it only uses the environment varaible "PATH"

    nfds = getdtablesize();
    FD_ZERO(&afds);
    FD_SET(sockfd, &afds);

    for (int i = 0; i < MAX_CLIENT; ++i)
    {
        ID myself;
        who_list.push_back(myself);
        env_list.push_back(string("bin:."));
        // cout << "main: " << who_list[i].getid() << endl;
    }

    Numbered_pipe b[MAX_PUBLIC_PIPE];
    for (int i = 0; i < MAX_PUBLIC_PIPE; ++i)
    {
        public_pipe[i] = b[i];
    }

    while (true)
    {
        memcpy(&rfds, &afds, sizeof(rfds));

        if ( select(nfds, &rfds, NULL, NULL, NULL) < 0) {
            perror("select");
            exit(1);
        }

        cout << "after select \n"; fflush(stdout);

        if (FD_ISSET(sockfd, &rfds)) {
            clilen = sizeof(cli_addr);
            newfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
            if (newfd < 0) {
                perror("accept");
                exit(1);
            }

            cout << "after accept \n"; fflush(stdout);
            FD_SET(newfd, &afds);
            // int port = ntohs(cli_addr.sin_port);
            // inet_ntop(AF_INET, &cli_addr.sin_addr, ipstr, sizeof(ipstr));
            // for demo
            int port = 511;
            strcpy(ipstr, "CGILAB");
            available_id = get_first_vacant_id(who_list);
            if ( available_id < 0 ) {
                write(newfd, "error", 5);
                close(newfd);
                continue;
            }

            ID myself(string("(no name)"), string(ipstr), to_string(port), newfd, available_id);
            who_list[available_id - 1] = myself;

            // welcome message
            write(newfd, WELCOME, strlen(WELCOME));

            // cout << "after welcome\n"; fflush(stdout);

            //broadcast to everyone, new incoming guest.
            snprintf(msg, MSG_LEN, "*** User '%s' entered from %s/%s. ***\n", myself.getname().c_str(), myself.getip().c_str(), myself.getport().c_str());
            for (int i = 0; i < who_list.size(); i++) {
                if (who_list[i].getid() != -1)
                    write(who_list[i].getsocket(), msg, strlen(msg));
            }

            // cout << "after broadcast \n"; fflush(stdout);
            write(newfd, SHELL_PROMPT, strlen(SHELL_PROMPT));
            // cout << "after shell prompt \n"; fflush(stdout);
        }

        for (int fd = 0; fd < nfds; fd++) {
            if (fd != sockfd && FD_ISSET(fd, &rfds)) {
                //do something
                for (int i = 0; i < who_list.size(); i++) {
                    if (who_list[i].getsocket() == fd) {
                        int y = shell(fd, who_list, env_list, i, who_list[i].getname(), who_list[i].getip(), who_list[i].getport());
                        if ( y == 1) {
                            snprintf(msg, MSG_LEN, "*** User '%s' left. ***\n", who_list[i].getname().c_str());
                            for (int j = 0; j < who_list.size(); j++) {
                                if (who_list[j].getid() != -1)
                                    write(who_list[j].getsocket(), msg, strlen(msg));
                            }

                            who_list[i].setid(-1);
                            env_list[i] = "bin:.";

                            FD_CLR(fd, &afds);
                            close(fd);
                        }
                        else {
                            write(fd, SHELL_PROMPT, strlen(SHELL_PROMPT));
                            // cout << "for loop shell prompt \n"; fflush(stdout);
                        }
                        break;
                    }
                }

            }
        }
    } // end while
}


