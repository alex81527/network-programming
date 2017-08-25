#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<ctype.h>
#include<netdb.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<signal.h>
#include<fcntl.h>       // open, O_WRONLY, O_TRUNC, O_CREAT, etc
#include<unistd.h>      // fork, dup, exec, read/write, etc
#include<string.h>      // memset
#include<string>        // c++ string
#include<vector>
#include<sstream>       // convert string to stream
#include<iostream>
#include<algorithm>     // remove, find 
using namespace std;
const char* SHELL_PROMPT = "% ";
int PORT = 65005;

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

vector<string> split(char* str, char* delim);
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

void shell() {

    int childpid, pipefd[2][2], cur_pipe, last_pipe;
    char cmd[15000], cache_data[65536];
    int status, redir_stdout, redir_stderr, arg_cnt, angle_bracket, has_to_transfer, unknown, transfer[2], read_bytes;
    vector<string> tmp, tmp2;
    vector<int> free_array; //tracking how many memories needed to be freed in arg_list
    vector<char**> arg_list;
    vector<Numbered_pipe> num_pipe;
    Numbered_pipe redir_stdout_class, redir_stderr_class;

    unknown = 0; // 0 is ok, 1 means something wrong happened in last command

    cout <<
         "****************************************\n"
         "** Welcome to the information server. **\n"
         "****************************************\n";
    fflush(stdout);

    while (true) {
        memset(cmd, 0, 15000);

        redir_stdout = -1;
        redir_stderr = -1;
        cur_pipe = 1;

        free_array.clear();
        tmp.clear();
        tmp2.clear();
        arg_list.clear();

        write(1, SHELL_PROMPT, strlen(SHELL_PROMPT));
        string s="";
        while(true){
            int len = read(0, cmd, 15000);
            // cout<< "cmd : " << cmd << endl;
            // cout<< "len : " << len << endl;
            // cout<< "cmd[len-1]" << (int)cmd[len-1] << endl;
            // fflush(stdout);

            if(cmd[len-1]=='\n' || (int)cmd[len-1] == 10 || (int)cmd[len-1] == 13){
                s += string(cmd);
                strcpy(cmd, s.c_str());
                break;
            }
            else{
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

        // cout << "*** " << cmd << "@" << endl;

        if (!strcmp(cmd, "") || !strcmp(cmd, "\n") || !strcmp(cmd, "\r\n"))
            continue;
        else if (!strcmp(cmd, "exit") || !strcmp(cmd, "exit\n") || !strcmp(cmd, "exit\r\n"))
            break;
        else if (contained_slash(cmd))
        {
            cerr << "Slash (\"/\") in command is not allowed." << endl;
            continue;
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
                free_array.push_back(arg_cnt);
                for (int j = 0; j < arg_cnt; j++)
                {
                    p[j] = new char[tmp2[j].length() + 1];
                    strcpy(p[j], tmp2[j].c_str());
                    //cout<< j <<" "<<tmp2[j]<<"!"<<endl;
                }
                p[arg_cnt] = NULL;
                arg_list.push_back(p);
                tmp2.clear();
            }
            else if ( exclamation_found != string::npos )
            {
                redir_stderr = atoi(z.substr(exclamation_found + 1).c_str());
                //cout << "exclamation found: " << redir_stderr << endl;
            }
            else if ( pipe_found != string::npos )
            {
                redir_stdout = atoi(z.substr(pipe_found + 1).c_str());
                //cout << "pipe found: " << redir_stdout << endl;
            }
            else
            {
                tmp2.push_back(z);
            }
        }

        arg_cnt = tmp2.size();
        char** p = new char*[arg_cnt + 1]; // additional space for terminating NULL
        free_array.push_back(arg_cnt);
        for (int j = 0; j < arg_cnt; j++)
        {
            p[j] = new char[tmp2[j].length() + 1];
            strcpy(p[j], tmp2[j].c_str());
            //cout<< j <<" "<<tmp2[j]<<"!"<<endl;
        }
        p[arg_cnt] = NULL;
        arg_list.push_back(p);

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
        if (unknown == 0)
            has_to_transfer = 0;

        // Step1.
        // decrease every count in num_pipe by 1
        // remove entries with count 0 and open a new pipe to cache/collect those data
        if (num_pipe.size() > 0 )
        {
            if ( pipe(transfer) < 0)
            {
                perror("pipe");
                exit(1);
            }

            if (unknown == 0) {
                int size = num_pipe.size();
                for (int i = 0; i < size; i++)
                {
                    num_pipe[i].setcnt(num_pipe[i].getcnt() - 1);
                    // cout << " num_pipe: count=" << num_pipe[i].getcnt() << " fd_r=" << num_pipe[i].getfdr() << " fd_w=" << num_pipe[i].getfdw() << endl;
                    if (num_pipe[i].getcnt() == 0)
                    {
                        has_to_transfer = 1;
                        read_bytes = read(num_pipe[i].getfdr(), cache_data, sizeof(cache_data));
                        write(transfer[1], cache_data, read_bytes);
                        // cout << endl << "read " << read_bytes << " bytes: " << cache_data << endl;
                    }
                }

                // in case of unknown command, e.g. ctt, cache_data serves as a backup
                if (has_to_transfer == 1)
                {
                    read_bytes = read(transfer[0], cache_data, sizeof(cache_data));
                    write(transfer[1], cache_data, read_bytes);
                }

            } else {
                // restore cache_data into pipe
                write(transfer[1], cache_data, read_bytes);
            }

            if (has_to_transfer != 1)
            {
                close(transfer[0]);
                close(transfer[1]);
            }
        }

        // Step2. check if there is fd opened for the count "redir_stdout" or "redir_stderr"
        // if not, create a new one, and push it into num_pipe
        if ( redir_stdout > 0)
        {
            int index;
            if ( (index = is_contained(num_pipe, redir_stdout)) < 0) {
                int tmp_fd[2];
                if (pipe(tmp_fd) < 0) {
                    perror("pipe");
                    exit(1);
                }
                Numbered_pipe c(tmp_fd[0], tmp_fd[1], redir_stdout);
                num_pipe.push_back(c);
                redir_stdout_class = c;
            }
            else {
                redir_stdout_class = num_pipe[index];
            }
        }

        if ( redir_stderr > 0)
        {
            int index;
            if ( (index = is_contained(num_pipe, redir_stderr)) < 0) {
                int tmp_fd[2];
                if (pipe(tmp_fd) < 0) {
                    perror("pipe");
                    exit(1);
                }
                Numbered_pipe c(tmp_fd[0], tmp_fd[1], redir_stderr);
                num_pipe.push_back(c);
                redir_stderr_class = c;
            }
            else
                redir_stderr_class = num_pipe[index];
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
        if (arg_list.size() == 1)
        {
            if ( !strcmp(arg_list[0][0], "setenv") ) {
                if (setenv(arg_list[0][1], arg_list[0][2], 1) < 0)
                    perror("setenv");
                free_memory(arg_list, free_array);
                continue;
            }
            else if ( !strcmp(arg_list[0][0], "printenv") ) {
                cout << "PATH=" << getenv(arg_list[0][1]) << endl;
                fflush(stdout);
                free_memory(arg_list, free_array);
                continue;
            }

            angle_bracket = find_angle_bracket(arg_list[0], free_array[0]);

            if ( (childpid = fork()) < 0)
                perror("fork");
            else if (childpid == 0) // child process
            {
                if (has_to_transfer == 1) // read from transfer pipe
                {
                    close(0);
                    dup(transfer[0]);
                    close(transfer[1]);
                }

                if (redir_stdout > 0)
                {
                    close(1);
                    dup(redir_stdout_class.getfdw());
                }

                if (redir_stderr > 0)
                {
                    close(2);
                    dup(redir_stderr_class.getfdw());
                }

                // handle redirection ">"
                if (angle_bracket > 0)
                {
                    char** new_cmd = new char*[free_array[0] + 1];
                    char* filename;
                    for (int j = 0; j < free_array[0]; j++)
                    {
                        if ( j == angle_bracket )
                        {
                            filename = arg_list[0][j + 1];
                            new_cmd[j] = NULL;
                            break;
                        }
                        new_cmd[j] = arg_list[0][j];
                        //cout<<"xxx "<<arg_list[0][j]<<endl;
                    }

                    int fd;
                    if (  (fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0)
                        perror("open");

                    close(1);
                    dup(fd);

                    //execvp cmd
                    execute(new_cmd, arg_list, free_array);
                    delete [] new_cmd;
                    free_memory(arg_list, free_array);
                    exit(1);
                }
                else // no redirection
                {
                    //execvp cmd
                    execute(arg_list[0], arg_list, free_array);
                    free_memory(arg_list, free_array);
                    exit(1);
                }
            }
            else // parent process
            {
                if (has_to_transfer == 1) // has to close the write end, so that child won't hang
                {
                    close(transfer[0]);
                    close(transfer[1]);
                }

                wait(&status);
                if (WEXITSTATUS(status) == 2)
                {
                    unknown = 1;

                    cout << "Unknown command: [" << arg_list[0][0] << "]." << endl;
                    fflush(stdout);
                    continue;
                }

                // if it gets here, the cmd has successfully executed
                unknown = 0;
                if (has_to_transfer == 1) // close unneeded pipes, remove entries with count 0
                {
                    for (Numbered_pipe n : num_pipe)
                    {
                        if (n.getcnt() == 0)
                        {
                            close(n.getfdr());
                            close(n.getfdw());
                        }
                    }
                    // c++ erase-remove paradigm
                    num_pipe.erase(remove_if(num_pipe.begin(), num_pipe.end(), is_zero), num_pipe.end());
                }
            }
        }
        else
        {
            int last = arg_list.size() - 1;
            angle_bracket = find_angle_bracket(arg_list[last], free_array[last]);

            for (int i = 0; i < arg_list.size(); i++)
            {
                last_pipe = cur_pipe;
                cur_pipe = cur_pipe ? 0 : 1;

                if (pipe(pipefd[cur_pipe]) < 0)
                {
                    perror("pipe");
                    exit(1);
                }

                if ( (childpid = fork()) < 0)
                    perror("fork");
                else if (childpid == 0) // child process
                {
                    if (i == 0)
                    {
                        if (has_to_transfer == 1) // read from transfer pipe
                        {
                            close(0);
                            dup(transfer[0]);
                            close(transfer[1]);
                        }

                        close(1);
                        dup(pipefd[cur_pipe][1]);
                        close(pipefd[cur_pipe][0]);
                    } else if (i == arg_list.size() - 1) {
                        close(0);
                        dup(pipefd[last_pipe][0]);
                        close(pipefd[cur_pipe][0]);
                        close(pipefd[cur_pipe][1]);

                        if (redir_stdout > 0)
                        {
                            close(1);
                            dup(redir_stdout_class.getfdw());
                        }

                        if (redir_stderr > 0)
                        {
                            close(2);
                            dup(redir_stderr_class.getfdw());
                        }

                        // handle redirection ">"
                        if (angle_bracket > 0)
                        {
                            char** new_cmd = new char*[free_array[i] + 1];
                            char* filename;
                            for (int j = 0; j < free_array[i]; j++)
                            {
                                if ( j == angle_bracket )
                                {
                                    filename = arg_list[i][j + 1];
                                    new_cmd[j] = NULL;
                                    break;
                                }
                                new_cmd[j] = arg_list[i][j];
                                //cout<<"xxx "<<arg_list[0][j]<<endl;
                            }

                            int fd;
                            if (  (fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0)
                                perror("open");

                            close(1);
                            dup(fd);

                            //execvp cmd
                            execute(new_cmd, arg_list, free_array);
                            delete [] new_cmd;
                            free_memory(arg_list, free_array);
                            exit(1);
                        }
                    } else {
                        close(0);
                        close(1);
                        dup(pipefd[last_pipe][0]);
                        dup(pipefd[cur_pipe][1]);
                        close(pipefd[cur_pipe][0]);
                    }

                    //execvp cmd
                    execute(arg_list[i], arg_list, free_array);
                    free_memory(arg_list, free_array);
                    exit(1);
                }
                else // parent process
                {
                    if (has_to_transfer == 1)
                    {
                        close(transfer[0]);
                        close(transfer[1]);
                    }

                    if (redir_stdout > 0)
                    {

                    }

                    if (redir_stderr > 0)
                    {

                    }

                    // parent "must" close the writing end of the pipe, so that the child won't hang there waiting
                    close(pipefd[cur_pipe][1]);
                    wait(&status);

                    if (i != 0)
                        close(pipefd[last_pipe][0]);

                    if (WEXITSTATUS(status) == 2)
                    {
                        unknown = 1;
                        cout << "Unknown command: [" << arg_list[i][0] << "]." << endl;
                        fflush(stdout);
                        break;
                    }
                }
            } // end for loop

            unknown = 0;
            if (has_to_transfer == 1)
            {
                for (Numbered_pipe n : num_pipe)
                {
                    if (n.getcnt() == 0)
                    {
                        close(n.getfdr());
                        close(n.getfdw());
                    }
                }
                // c++ erase-remove paradigm
                num_pipe.erase(remove_if(num_pipe.begin(), num_pipe.end(), is_zero), num_pipe.end());
            }
        }

        free_memory(arg_list, free_array);
    } // end while
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

int main(int argc, char** argv, char** envp)
{
    string dir(getenv("HOME"));
    dir += "/ras";

    if (chdir(dir.c_str()) < 0)
    {
        perror("chdir");
        exit(1);
    }

    if (setenv("PATH", "bin:.", 1) < 0)
    {
        perror("setenv");
        exit(1);
    }

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

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(PORT);

    if ( bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("bind");
    }

    listen(sockfd, 5);

    while (true)
    {
        clilen = sizeof(cli_addr);
        newfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);

        if (newfd < 0)
            perror("accept");

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
            shell();
            exit(0);
        }
        else // parent process
        {
            close(newfd);
            waitpid(-1, NULL, WNOHANG);
        }
    }

}
