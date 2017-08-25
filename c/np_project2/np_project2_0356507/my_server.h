
void signal_handler(int signo);
vector<string> split(char* str, char* delim);
int is_contained(vector<Numbered_pipe>& n, int count);
bool contained_slash(char* cmd);
bool is_zero(Numbered_pipe n);
int find_angle_bracket(char ** p, int len);
void free_memory(vector<char**> &arg_list, vector<int> &free_array);
void execute(char** cmd, vector<char**> &arg_list, vector<int> &free_array);
int shell(int fd, vector<ID>& who_list, vector<vector<pair<string, string>>>& env_list, int index);
void test();
int get_first_vacant_id(vector<ID>& who_list);
void who(int fd, vector<ID>& who_list);