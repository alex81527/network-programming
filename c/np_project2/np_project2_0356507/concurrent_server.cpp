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
#include<unistd.h>      // fork, dup, exec, read/write, etc
#include<string.h>      // memset
#include<string>        // c++ string
#include<vector>
#include<sstream>       // convert string to stream
#include<iostream>
#include<algorithm>     // remove, find 
#include<sys/ipc.h>
#include<sys/sem.h>
#include<sys/shm.h>
using namespace std;


const char* SHELL_PROMPT = "% ";
const char* WELCOME = "****************************************\n"
                      "** Welcome to the information server. **\n"
                      "****************************************\n";
const int MAX_CLIENT = 40;
const int MSG_LEN = 200;
const int MAX_PUBLIC_PIPE = 100;
const char *HASHV = "ecb0cffbd492ac706ccc957466ea09"; 

key_t SEMKEY1 = 1564851;
key_t SEMKEY2 = 1564852;
int semid_who_list, semid_public_pipe;

key_t SHMKEY1 = 1564853;
key_t SHMKEY2 = 1564854;
key_t SHMKEY3 = 1564855;
int PORT = 65005;
char msg[MSG_LEN];
char my_name[100];
char my_ip[20];
int my_fd;
int my_port;
int my_index;

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
    ID(char* a = (char*)"", char* b = (char*)"", int c = -1, int skt = -1, int d = -1, int e = -1, char* f = (char*)"", char* g = (char*)"") {
        setname(a);
        setip(b);
        setport(c);
        socket = skt;
        id = d;
        pid = e;
        setmsg1(f);
        setmsg2(g);
    }
    char* getname() { return name;}
    char* getip() { return ip;}
    int getport() { return port;}
    int getsocket() {return socket;}
    int getid() {return id;}
    int getpid() {return pid;}
    char* getmsg1() {return msg1;}
    char* getmsg2() {return msg2;}
    void setname(char* s) {strncpy(name , s, 100);}
    void setip(char* s) {strncpy(ip , s, 20);}
    void setport(int s) {port = s;}
    void setsocket(int s) {socket = s;}
    void setid(int a) { id = a;}
    void setpid(int a) { pid = a;}
    void setmsg1(char* a) {strncpy(msg1, a, 1025);};
    void setmsg2(char* a) {strncpy(msg2, a, 1025);};
private:
    char name[100];
    char ip[20];
    int port;
    int socket;
    int id;
    int pid;
    char msg1[1025];
    char msg2[1025];
};


const int SHM_SIZE_WHO_LIST = MAX_CLIENT * sizeof(ID);
const int SHM_SIZE_PUBLIC_PIPE = MAX_PUBLIC_PIPE * sizeof(Numbered_pipe);
const int SHM_SIZE_MSG = 3 * 1024;
ID* shm_who_list;                // pointer to shared memory
Numbered_pipe* shm_public_pipe;  // pointer to shared memory
char* shm_msg;  // pointer to shared memory
int shmid_who_list, shmid_public_pipe, shmid_msg;

void signal_handler(int signo);
vector<string> split(char* str, char* delim);
int is_contained(vector<Numbered_pipe>& n, int count);
bool contained_slash(char* cmd);
bool is_zero(Numbered_pipe n);
int find_angle_bracket(char ** p, int len);
int find_public_redir_output(char** p, int len);
int find_first_vacant_pos(Numbered_pipe* pub, int num);
int find_pos(Numbered_pipe* pub, int num);
int find_public_redir_intput(char** p, int len);
void free_memory(vector<char**> &arg_list, vector<int> &free_array);
void execute(char** cmd, vector<char**> &arg_list, vector<int> &free_array);
void shell();
void test();
int get_first_vacant_id(vector<ID>& who_list);
void who();
void tell(int id, const char* msg2);
void yell(const char* msg2);
void name(const char* msg2);

void    sem_op(int, int);
int     sem_create(key_t, int);
int     sem_open(key_t);
void    sem_rm(int);
void    sem_close(int);
void    sem_wait(int);
void    sem_signal(int);


void signal_handler(int signo)
{
    switch (signo)
    {
    case SIGCHLD:
        waitpid(-1, NULL, WNOHANG);
        break;
    case SIGUSR1:
        write(my_fd, shm_who_list[my_index].getmsg1(), strlen(shm_who_list[my_index].getmsg1()));
        break;
    case SIGUSR2:
        write(my_fd, shm_who_list[my_index].getmsg2(), strlen(shm_who_list[my_index].getmsg2()));
        break;
    case SIGINT:
        /* detach from the segment: */
        if (shmdt(shm_msg) == -1 || shmdt(shm_who_list) == -1 || shmdt(shm_public_pipe) == -1) {
            perror("shmdt");
            exit(1);
        }
        shmctl(shmid_msg, IPC_RMID, NULL);
        shmctl(shmid_who_list, IPC_RMID, NULL);
        shmctl(shmid_public_pipe, IPC_RMID, NULL);

        sem_rm(semid_who_list);
        sem_rm(semid_public_pipe);

        exit(0);
        break;
    default:
        return;
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

int find_first_vacant_pos(Numbered_pipe* pub, int num) {
    sem_wait(semid_public_pipe);

    int first_vacant, on = 1;
    for (int i = 0; i < MAX_PUBLIC_PIPE; ++i)
    {
        if (on == 1 && pub[i].getcnt() == -1) {
            first_vacant = i;
            on = 0;
        }
        if (pub[i].getcnt() == num) {
            sem_signal(semid_public_pipe);
            return -1;
        }
    }
    sem_signal(semid_public_pipe);
    return first_vacant;
}

int find_pos(Numbered_pipe* pub, int num) {
    sem_wait(semid_public_pipe);

    for (int i = 0; i < MAX_PUBLIC_PIPE; ++i)
    {
        if (pub[i].getcnt() == num) {
            sem_signal(semid_public_pipe);
            return i;
        }
    }
    sem_signal(semid_public_pipe);
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

void shell() {

    int childpid, pipefd[2][2], cur_pipe, last_pipe;
    char cmd[15000], cache_data[65536];
    int status, redir_stdout, redir_stderr, arg_cnt, angle_bracket, has_to_transfer, unknown, transfer[2], read_bytes;
    vector<string> tmp, tmp2;
    vector<int> free_array; //tracking how many memories needed to be freed in arg_list
    vector<char**> arg_list;
    vector<Numbered_pipe> num_pipe;
    Numbered_pipe redir_stdout_class, redir_stderr_class;
    string backup_cmd;

    unknown = 0; // 0 is ok, 1 means something wrong happened in last command

    sem_wait(semid_who_list);
    {
        shm_who_list[my_index].setpid(getpid());
        //broadcast to everyone, new incoming guest.
        snprintf(msg, MSG_LEN, "*** User '%s' entered from %s/%d. ***\n", my_name, my_ip, my_port);
        for (int i = 0; i < MAX_CLIENT; i++) {
            if (shm_who_list[i].getid() == my_index + 1)
                write(1, msg, strlen(msg));
            else if (shm_who_list[i].getid() != -1) {
                shm_who_list[i].setmsg1(msg);
                kill(shm_who_list[i].getpid(), SIGUSR1);
            }
        }
    }
    sem_signal(semid_who_list);

    while (true) {

        write(1, SHELL_PROMPT, strlen(SHELL_PROMPT));

        memset(cmd, 0, 15000);

        redir_stdout = -1;
        redir_stderr = -1;
        cur_pipe = 1;

        free_array.clear();
        tmp.clear();
        tmp2.clear();
        arg_list.clear();

        string s = "";
        while (true) {
            int len = read(0, cmd, 15000);
            // cout<< "cmd : " << cmd << endl;
            // cout<< "len : " << len << endl;
            // cout<< "cmd[len-1]" << (int)cmd[len-1] << endl;
            // fflush(stdout);

            if (cmd[len - 1] == '\n' || (int)cmd[len - 1] == 10 || (int)cmd[len - 1] == 13) {
                s += string(cmd);
                strcpy(cmd, s.c_str());
                int found3 = s.find_first_of("\r\n");
                backup_cmd = s.substr(0, found3);
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

        // cout << "*** " << cmd << "@" << endl;

        if (!strcmp(cmd, "") || !strcmp(cmd, "\n") || !strcmp(cmd, "\r\n"))
            continue;
        else if (!strcmp(cmd, "exit") || !strcmp(cmd, "exit\n") || !strcmp(cmd, "exit\r\n")) {
            sem_wait(semid_who_list);
            {
                snprintf(msg, MSG_LEN, "*** User '%s' left. ***\n", my_name);
                for (int j = 0; j < MAX_CLIENT; j++) {
                    if (shm_who_list[j].getid() != -1) {
                        shm_who_list[j].setmsg1(msg);
                        kill(shm_who_list[j].getpid(), SIGUSR1);
                    }
                }

                shm_who_list[my_index].setid(-1);
            }
            sem_signal(semid_who_list);
            return;
        }
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
            if (unknown == 0) {
                int size = num_pipe.size();
                for (int i = 0; i < size; i++)
                {
                    num_pipe[i].setcnt(num_pipe[i].getcnt() - 1);
                    // cout << " num_pipe: count=" << num_pipe[i].getcnt() << " fd_r=" << num_pipe[i].getfdr() << " fd_w=" << num_pipe[i].getfdw() << endl;
                    if (num_pipe[i].getcnt() == 0)
                    {
                        has_to_transfer = 1;
                        // cout << "reading from ["<< i <<"] :\n"; fflush(stdout);
                        // read_bytes = read(num_pipe[i].getfdr(), cache_data, sizeof(cache_data));
                        // cout<< read_bytes << " read\n";
                        // fflush(stdout);
                        // write(transfer[1], cache_data, read_bytes);
                        // cout << endl << "read " << read_bytes << " bytes: " << cache_data << endl;
                        transfer[0] = num_pipe[i].getfdr();
                        transfer[1] = num_pipe[i].getfdw();
                        close(transfer[1]);
                        break;
                    }
                }

                // in case of unknown command, e.g. ctt, cache_data serves as a backup
                // if (has_to_transfer == 1)
                // {
                //     read_bytes = read(transfer[0], cache_data, sizeof(cache_data));
                //     write(transfer[1], cache_data, read_bytes);
                // }

            }
            // else {
            //     // restore cache_data into pipe
            //     write(transfer[1], cache_data, read_bytes);
            // }
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
                goto END;
            }
            else if ( !strcmp(arg_list[0][0], "printenv") ) {
                snprintf(msg, MSG_LEN, "%s=%s\n", arg_list[0][1], getenv(arg_list[0][1]) );
                write(2, msg, strlen(msg));
                goto END;
            }
            else if ( !strcmp(arg_list[0][0], "who") ) {
                who();
                goto END;
            }
            else if ( !strcmp(arg_list[0][0], "tell") ) { //msg2 needs modification
                string s = backup_cmd;
                int found = s.find_first_of(" ");
                found = s.find_first_of(" ", found + 1);
                int found2 = s.find_first_not_of(" ", found + 1);

                tell(atoi(arg_list[0][1]), s.substr(found2).c_str());
                goto END;
            }
            else if ( !strcmp(arg_list[0][0], "yell") ) { //msg2 needs modification
                string s = backup_cmd;
                int found = s.find_first_of(" ");
                int found2 = s.find_first_not_of(" ", found + 1);

                yell(s.substr(found2).c_str());
                goto END;
            }
            else if ( !strcmp(arg_list[0][0], "name") ) { //msg2 needs modification
                string s = backup_cmd;
                int found = s.find_first_of(" ");
                int found2 = s.find_first_not_of(" ", found + 1);

                name(s.substr(found2).c_str());
                goto END;
            }

            angle_bracket = find_angle_bracket(arg_list[0], free_array[0]);
            int public_redir_out = find_public_redir_output(arg_list[0], free_array[0]);
            int public_redir_int = find_public_redir_intput(arg_list[0], free_array[0]);
            int public_redir_out_fdw, public_redir_int_fdr;
            int pos_out, pos_int;

            bool error_public_pipe = false;
            // handle public pipe intput redirection, e.g., "<3"
            if (public_redir_int > 0) {
                pos_int = find_pos(shm_public_pipe, public_redir_int); // return -1 if public_redir_int doesn't exist, otherwise return its index in the array
                if (pos_int < 0) {
                    snprintf(msg, MSG_LEN, "*** Error: public pipe #%d does not exist yet. ***\n", public_redir_int);
                    write(my_fd, msg, strlen(msg));
                    unknown = 1;
                    error_public_pipe = true;
                }
                else {
                    char filename[100];
                    snprintf(filename, 100, "/tmp/%d%s", public_redir_int, HASHV);
                    int fdd;
                    if (  (fdd = open(filename, O_RDONLY, 0666)) < 0) {
                        perror("open");
                        goto END;
                    }

                    public_redir_int_fdr = fdd;
                    sem_wait(semid_who_list);
                    {
                        snprintf(msg, MSG_LEN, "*** %s (#%d) just received via '%s' ***\n", my_name, my_index + 1, backup_cmd.c_str());
                        for (int i = 0; i < MAX_CLIENT; ++i)
                        {
                            if (shm_who_list[i].getid() != -1) {
                                shm_who_list[i].setmsg1(msg);
                                kill(shm_who_list[i].getpid(), SIGUSR1);
                            }
                        }
                    }
                    sem_signal(semid_who_list);
                }
            }

            // handle public pipe output redirection, e.g., ">5"
            if (public_redir_out > 0) {
                pos_out = find_first_vacant_pos(shm_public_pipe, public_redir_out); // return -1 if public_redir_out already exists, otherwise return first vacant position in the array
                if (pos_out < 0) {
                    snprintf(msg, MSG_LEN, "*** Error: public pipe #%d already exists. ***\n", public_redir_out);
                    write(my_fd, msg, strlen(msg));
                    unknown = 1;
                    error_public_pipe = true;
                }
                else if (!error_public_pipe) {
                    char filename[100];
                    snprintf(filename, 100, "/tmp/%d%s", public_redir_out, HASHV);
                    int fdd;
                    if (  (fdd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0) {
                        perror("open");
                        goto END;
                    }
                    Numbered_pipe c(fdd, fdd, public_redir_out);

                    sem_wait(semid_public_pipe);
                    {
                        shm_public_pipe[pos_out] = c;
                    }
                    sem_signal(semid_public_pipe);

                    public_redir_out_fdw = fdd;

                    sem_wait(semid_who_list);
                    {
                        snprintf(msg, MSG_LEN, "*** %s (#%d) just piped '%s' ***\n", my_name, my_index + 1, backup_cmd.c_str());
                        for (int i = 0; i < MAX_CLIENT; ++i)
                        {
                            if (shm_who_list[i].getid() != -1) {
                                shm_who_list[i].setmsg2(msg);
                                kill(shm_who_list[i].getpid(), SIGUSR2);
                            }
                        }
                    }
                    sem_signal(semid_who_list);
                }
            }

            if (error_public_pipe)
                goto END;


            if ( (childpid = fork()) < 0)
                perror("fork");
            else if (childpid == 0) // child process
            {
                if (has_to_transfer == 1) // read from transfer pipe
                {
                    close(0);
                    dup(transfer[0]);
                    // close(transfer[1]);
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
                    char* filename = arg_list[0][angle_bracket + 1];

                    int fdd;
                    if (  (fdd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0)
                        perror("open");

                    close(1);
                    dup(fdd);
                }

                for (int i = 0; i < free_array[0]; ++i)
                {
                    if (arg_list[0][i] == NULL) // prevent string constructor error in next line, string(NULL)
                        continue;

                    string s(arg_list[0][i]);
                    if (!strcmp(arg_list[0][i], ">")) {
                        arg_list[0][i] = NULL;
                        arg_list[0][i + 1] = NULL;
                    }
                    else if (s.find_first_of("><") != string::npos)
                        arg_list[0][i] = NULL;
                }

                //execvp cmd
                execute(arg_list[0], arg_list, free_array);
                free_memory(arg_list, free_array);
                exit(1);
            }
            else // parent process
            {
                if (has_to_transfer == 1) // has to close the write end, so that child won't hang
                {
                    // close(transfer[0]);
                    // close(transfer[1]);
                }

                if (public_redir_out > 0) {
                    close(public_redir_out_fdw);
                }

                wait(&status);

                // has to wait until child successfully executed the cmd, then unlink the /tmp/%d%s
                if (public_redir_int > 0) {
                    close(public_redir_int_fdr);

                    sem_wait(semid_public_pipe);
                    {
                        shm_public_pipe[pos_int].setcnt(-1);
                    }
                    sem_signal(semid_public_pipe);

                    char filename[100];
                    snprintf(filename, 100, "/tmp/%d%s", public_redir_int, HASHV);
                    if (unlink(filename) < 0)
                    {
                        perror("unlink");
                        return;
                    }
                }

                if (WEXITSTATUS(status) == 2)
                {
                    unknown = 1;

                    snprintf(msg, MSG_LEN, "Unknown command: [%s].\n", arg_list[0][0]);
                    write(my_fd, msg, strlen(msg));
                    goto END;
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
                            // close(n.getfdw());
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

                int public_redir_out = find_public_redir_output(arg_list[i], free_array[i]);
                int public_redir_int = find_public_redir_intput(arg_list[i], free_array[i]);
                int public_redir_out_fdw, public_redir_int_fdr;
                int pos_out, pos_int;

                bool error_public_pipe = false;
                // handle public pipe intput redirection, e.g., "<3"
                if (public_redir_int > 0) {
                    pos_int = find_pos(shm_public_pipe, public_redir_int); // return -1 if public_redir_int doesn't exist, otherwise return its index in the array
                    if (pos_int < 0) {
                        snprintf(msg, MSG_LEN, "*** Error: public pipe #%d does not exist yet. ***\n", public_redir_int);
                        write(my_fd, msg, strlen(msg));
                        unknown = 1;
                        error_public_pipe = true;
                    }
                    else {
                        char filename[100];
                        snprintf(filename, 100, "/tmp/%d%s", public_redir_int, HASHV);
                        int fdd;
                        if (  (fdd = open(filename, O_RDONLY, 0666)) < 0) {
                            perror("open");
                            goto END;
                        }

                        public_redir_int_fdr = fdd;

                        sem_wait(semid_who_list);
                        {
                            snprintf(msg, MSG_LEN, "*** %s (#%d) just received via '%s' ***\n", my_name, my_index + 1, backup_cmd.c_str());
                            for (int i = 0; i < MAX_CLIENT; ++i)
                            {
                                if (shm_who_list[i].getid() != -1) {
                                    shm_who_list[i].setmsg1(msg);
                                    kill(shm_who_list[i].getpid(), SIGUSR1);
                                }
                            }
                        }
                        sem_signal(semid_who_list);
                    }
                }

                // handle public pipe output redirection, e.g., ">5"
                if (public_redir_out > 0) {
                    pos_out = find_first_vacant_pos(shm_public_pipe, public_redir_out); // return -1 if public_redir_out already exists, otherwise return first vacant position in the array
                    if (pos_out < 0) {
                        snprintf(msg, MSG_LEN, "*** Error: public pipe #%d already exists. ***\n", public_redir_out);
                        write(my_fd, msg, strlen(msg));
                        unknown = 1;
                        error_public_pipe = true;
                    }
                    else if (!error_public_pipe){
                        char filename[100];
                        snprintf(filename, 100, "/tmp/%d%s", public_redir_out, HASHV);
                        int fdd;
                        if (  (fdd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0) {
                            perror("open");
                            goto END;
                        }
                        Numbered_pipe c(fdd, fdd, public_redir_out);

                        sem_wait(semid_public_pipe);
                        {
                            shm_public_pipe[pos_out] = c;
                        }
                        sem_signal(semid_public_pipe);

                        public_redir_out_fdw = fdd;

                        sem_wait(semid_who_list);
                        {
                            snprintf(msg, MSG_LEN, "*** %s (#%d) just piped '%s' ***\n", my_name, my_index + 1, backup_cmd.c_str());
                            for (int i = 0; i < MAX_CLIENT; ++i)
                            {
                                if (shm_who_list[i].getid() != -1) {
                                    shm_who_list[i].setmsg2(msg);
                                    kill(shm_who_list[i].getpid(), SIGUSR2);
                                }
                            }
                        }
                        sem_signal(semid_who_list);
                    }
                }

                if (error_public_pipe)
                    goto END;

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
                            // close(transfer[1]);
                        }

                        // handle public pipe intput redirection, e.g., "<3"
                        if (public_redir_int > 0) {
                            close(0);
                            dup(public_redir_int_fdr);
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

                        // handle public pipe output redirection, e.g., ">5"
                        if (public_redir_out > 0) {
                            close(1);
                            close(2);
                            dup(public_redir_out_fdw);
                            dup(public_redir_out_fdw);
                        }

                        // handle redirection ">"
                        if (angle_bracket > 0) {
                            char* filename = arg_list[i][angle_bracket + 1];

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
                        dup(pipefd[last_pipe][0]);
                        dup(pipefd[cur_pipe][1]);
                        close(pipefd[cur_pipe][0]);
                    }

                    for (int j = 0; j < free_array[i]; ++j)
                    {
                        if (arg_list[i][j] == NULL) // prevent string constructor error in next line, string(NULL)
                            continue;

                        string s(arg_list[i][j]);
                        if (!strcmp(arg_list[i][j], ">")) {
                            arg_list[i][j] = NULL;
                            arg_list[i][j + 1] = NULL;
                        }
                        else if (s.find_first_of("><") != string::npos)
                            arg_list[i][j] = NULL;
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
                        // close(transfer[0]);
                        // close(transfer[1]);
                    }

                    if (redir_stdout > 0)
                    {

                    }

                    if (redir_stderr > 0)
                    {

                    }

                    // parent "must" close the writing end of the pipe, so that the child won't hang there waiting
                    close(pipefd[cur_pipe][1]);

                    if (public_redir_out > 0) {
                        close(public_redir_out_fdw);
                    }

                    wait(&status);

                    if (public_redir_int > 0) {
                        close(public_redir_int_fdr);

                        sem_wait(semid_public_pipe);
                        {
                            shm_public_pipe[pos_int].setcnt(-1);
                        }
                        sem_signal(semid_public_pipe);

                        char filename[100];
                        snprintf(filename, 100, "/tmp/%d%s", public_redir_int, HASHV);
                        if (unlink(filename) < 0)
                        {
                            perror("unlink");
                            return;
                        }
                    }

                    if (i != 0)
                        close(pipefd[last_pipe][0]);

                    if (WEXITSTATUS(status) == 2)
                    {
                        unknown = 1;
                        snprintf(msg, MSG_LEN, "Unknown command: [%s].\n", arg_list[i][0]);
                        write(my_fd, msg, strlen(msg));
                        goto END;
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
                        // close(n.getfdw());
                    }
                }
                // c++ erase-remove paradigm
                num_pipe.erase(remove_if(num_pipe.begin(), num_pipe.end(), is_zero), num_pipe.end());
            }
        }
END:
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

int get_first_vacant_id(ID* shm_who_list) {
    for (int i = 0; i < MAX_CLIENT; ++i)
    {
        if ( shm_who_list[i].getid() == -1) {
            shm_who_list[i].setid(i + 1);
            return i + 1;
        }
        if (i == MAX_CLIENT - 1)
            return -1;
    }
}

void who() {

    snprintf(msg, MSG_LEN, "<ID>\t<nickname>\t<IP/port>\t<indicate me>\n");
    write(2, msg, strlen(msg));

    sem_wait(semid_who_list);
    {
        for (int i = 0; i < MAX_CLIENT; ++i)  {
            // if (i < 5){
            //     cout << (shm_who_list + i) << " " << (shm_who_list[i]).getid() << " " << (shm_who_list[i]).getname() << " " << (shm_who_list[i]).getip() << " " << (shm_who_list[i]).getport() << endl;
            // }
            if (shm_who_list[i].getid() != -1) {
                if (shm_who_list[i].getid() == my_index + 1) {
                    snprintf(msg, 200, "%d\t%s\t%s/%d\t<-me\n", shm_who_list[i].getid(), shm_who_list[i].getname(), shm_who_list[i].getip(), shm_who_list[i].getport());
                }
                else {
                    snprintf(msg, 200, "%d\t%s\t%s/%d\n", shm_who_list[i].getid(), shm_who_list[i].getname(), shm_who_list[i].getip(), shm_who_list[i].getport());
                }
                write(2, msg, strlen(msg));
            }
        }
    }
    sem_signal(semid_who_list);
}

void tell(int id, const char* msg2) {
    sem_wait(semid_who_list);
    {
        for (int i = 0; i < MAX_CLIENT; ++i)  {
            if (shm_who_list[i].getid() == id && id > 0) {
                snprintf(msg, MSG_LEN, "*** %s told you ***: %s\n", my_name, msg2);
                shm_who_list[i].setmsg1(msg);
                kill(shm_who_list[i].getpid(), SIGUSR1);
                // write(shm_who_list[i].getsocket(), msg, strlen(msg));
                sem_signal(semid_who_list);
                return;
            }

            if (i == MAX_CLIENT - 1 ) {
                snprintf(msg, MSG_LEN, "*** Error: user #%d does not exist yet. ***\n", id);
                write(my_fd, msg, strlen(msg));
            }
        }
    }
    sem_signal(semid_who_list);
}

void yell(const char* msg2) {
    sem_wait(semid_who_list);
    {
        snprintf(msg, MSG_LEN, "*** %s yelled ***: %s\n", my_name, msg2);
        for (int i = 0; i < MAX_CLIENT; ++i)  {
            if (shm_who_list[i].getid() != -1) {
                shm_who_list[i].setmsg1(msg);
                kill(shm_who_list[i].getpid(), SIGUSR1);
            }
        }
    }
    sem_signal(semid_who_list);
}

void name(const char* msg2) {
    sem_wait(semid_who_list);
    {
        bool found = false;
        for (int i = 0; i < MAX_CLIENT; ++i)  {
            if (shm_who_list[i].getid() != -1 && !strcmp(shm_who_list[i].getname(), msg2) ) {
                found = true;
                break;
            }
        }

        if (found) {
            snprintf(msg, MSG_LEN, "*** User '%s' already exists. ***\n", msg2);
            write(my_fd, msg, strlen(msg));
        }
        else {
            shm_who_list[my_index].setname((char*)msg2);
            strncpy(my_name, (char*)msg2, 100);
            snprintf(msg, MSG_LEN, "*** User from %s/%d is named '%s'. ***\n", my_ip, my_port, msg2);
            for (int i = 0; i < MAX_CLIENT; ++i)  {
                if (shm_who_list[i].getid() != -1) {
                    shm_who_list[i].setmsg1(msg);
                    kill(shm_who_list[i].getpid(), SIGUSR1);
                }
            }
        }
    }
    sem_signal(semid_who_list);
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

    clearenv();
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

    if (sigaction (SIGCHLD, &sa, NULL) < 0 || sigaction (SIGINT, &sa, NULL) < 0)
    {
        perror("sigaction SIGCHLD");
        exit(1);
    }

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

    listen(sockfd, MAX_CLIENT);

    int available_id ; // managing clients' ids
    char ipstr[INET_ADDRSTRLEN];
    ID who_list[MAX_CLIENT];
    Numbered_pipe public_pipe[MAX_PUBLIC_PIPE];

    for (int i = 0; i < MAX_CLIENT; ++i)
    {
        ID myself;
        who_list[i] = myself;
    }

    Numbered_pipe b[MAX_PUBLIC_PIPE];
    for (int i = 0; i < MAX_PUBLIC_PIPE; ++i)
    {
        public_pipe[i] = b[i];
    }


    // put who_list and public_pipe into shared memory

    // if (    (SHMKEY1 = ftok(FILE_TO_KEY, 0)) == -1 ||
    //         (SHMKEY2 = ftok(FILE_TO_KEY, 1)) == -1 ||
    //         (SHMKEY3 = ftok(FILE_TO_KEY, 2)) == -1 )
    // {
    //     perror("ftok");
    //     exit(1);
    // }
    /* connect to (and possibly create) the segment: */
    if (    (shmid_who_list = shmget(SHMKEY1, SHM_SIZE_WHO_LIST, 0666 | IPC_CREAT)) == -1 ||
            (shmid_public_pipe = shmget(SHMKEY2, SHM_SIZE_PUBLIC_PIPE, 0666 | IPC_CREAT)) == -1 ||
            (shmid_msg = shmget(SHMKEY3, SHM_SIZE_MSG, 0666 | IPC_CREAT)) == -1 )
    {
        perror("shmget");
        exit(1);
    }

    /* attach to the segment to get a pointer to it: */
    shm_who_list = (ID*) shmat(shmid_who_list, (void *)0, 0);
    shm_public_pipe = (Numbered_pipe*) shmat(shmid_public_pipe, (void *)0, 0);
    shm_msg = (char*) shmat(shmid_msg, (void *)0, 0);
    if (shm_who_list == (ID*)(-1) || shm_public_pipe == (Numbered_pipe*)(-1) || shm_msg == (char*)(-1) ) {
        perror("shmat");
        exit(1);
    }
    memcpy(shm_who_list, &who_list, SHM_SIZE_WHO_LIST);
    memcpy(shm_public_pipe, &public_pipe, SHM_SIZE_PUBLIC_PIPE);
    memset(shm_msg, 0, SHM_SIZE_MSG);

    // create semaphores
    // if (    (SEMKEY1 = ftok(FILE_TO_KEY, 3)) == -1 ||
    //         (SEMKEY2 = ftok(FILE_TO_KEY, 4)) == -1 )
    // {
    //     perror("ftok");
    //     exit(1);
    // }

    if ( (semid_who_list = sem_create(SEMKEY1, 1) ) < 0 ||
            (semid_public_pipe = sem_create(SEMKEY2, 1) ) < 0 )
    {
        perror("sem_create");
        exit(1);
    }

    while (true)
    {
        clilen = sizeof(cli_addr);
        newfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);

        if (newfd < 0)
            perror("accept");



        // int port = ntohs(cli_addr.sin_port);
        // inet_ntop(AF_INET, &cli_addr.sin_addr, ipstr, sizeof(ipstr));
        // for demo
        int port = 511;
        strcpy(ipstr, "CGILAB");

        // welcome message
        write(newfd, WELCOME, strlen(WELCOME));

        sem_wait(semid_who_list);
        {
            available_id = get_first_vacant_id(shm_who_list);
            // cout << "avail_id, newfd : " << available_id << " " << newfd << endl;
            if ( available_id < 0 ) {
                write(newfd, "error", 5);
                close(newfd);
                continue;
            }

            ID myself((char*)"(no name)", ipstr, port, newfd, available_id);
            strncpy(my_name, "(no name)", 100);
            strncpy(my_ip, ipstr, 20);
            my_port = port;
            my_fd = newfd;
            my_index = available_id - 1;

            shm_who_list[available_id - 1] = myself;

            // cout << shm_who_list[available_id - 1].getname() << " " << shm_who_list[available_id - 1].getip() << " " << shm_who_list[available_id - 1].getport() << endl;
        }
        sem_signal(semid_who_list);


        if ( (childpid = fork()) < 0)
            perror("fork");
        else if (childpid == 0) // child process
        {
            if ( sigaction (SIGUSR1, &sa, NULL) < 0 || sigaction (SIGUSR2, &sa, NULL) < 0 )
            {
                perror("sigaction SIGUSR");
                exit(1);
            }

            sa.sa_handler = SIG_DFL;

            if ( sigaction (SIGINT, &sa, NULL) < 0 )
            {
                perror("sigaction SIGINT");
                exit(1);
            }

            close(sockfd);
            close(0);
            close(1);
            close(2);
            dup(newfd);
            dup(newfd);
            dup(newfd);
            shell();
            // shutdown(newfd, SHUT_RDWR);
            exit(0);
        }
        else // parent process
        {
            close(newfd);
            waitpid(-1, NULL, WNOHANG);
        }
    }

}
