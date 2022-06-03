#include "../includes/CgiHandler.hpp"

#define CGI_RESOURCE_BUFFER_SIZE 100
#define CGI_READ_BUFFER_SIZE 64000

//# meta-variables
// gateway_interface : CGI/1.1
// path_translated : /Users/kimjeongjun/webserv/YoupiBanane/youpi.bla
// query_string : 
// req_method : GET
// content_len : 0
// req_uri : /directory/youpi.bla
// req_ident : 
// redirect_status : 200
// remote_addr : 127.0.0.1
// script_name : /directory//youpi.bla
// path_info : /directory/youpi.bla
// script_filename : /Users/kimjeongjun/webserv/YoupiBanane/youpi.bla
// server_name : localhost
// server_port : 80
// server_protocol : HTTP/1.1
// server_software : webserv/1.0

static void set_signal_kill_child_process(int sig)
{
	(void) sig;
    kill(-1,SIGKILL);
}

CgiHandler::CgiHandler(Request &request, Location& loc)
{
	this->env["AUTH_TYPE"] = "";
	this->env["CONTENT_TYPE"] = request.headers["Content-Type"];
	this->env["GATEWAY_INTERFACE"] = "CGI/1.1";
	this->env["PATH_INFO"] = request.get_path();
	this->env["PATH_TRANSLATED"] = this->get_target_file_fullpath(request, loc);
	this->env["QUERY_STRING"] = request.get_query();
	this->env["REMOTE_HOST"] = request.headers["Host"];
	this->env["REMOTE_ADDR"] = get_ip(request.get_client_fd());
	this->env["REMOTE_USER"] = "";
	this->env["REMOTE_IDENT"] = "";
	this->env["REQUEST_METHOD"] = request.method;
	this->env["REQUEST_URI"] = request.get_path();
	this->env["SCRIPT_NAME"] = request.get_path();
	this->env["SCRIPT_FILENAME"] = this->get_target_file_fullpath(request, loc);
	this->env["SERVER_NAME"] = request.headers["Host"];
	this->env["SERVER_PROTOCOL"] = "HTTP/1.1";
	this->env["SERVER_PORT"] = request.get_port();
	this->env["SERVER_SOFTWARE"] = "webserv/1.0";
	this->env["CONTENT_LENGTH"] = "-1";
	if (request.method == "GET")
	{
		load_file_resource();
	}
	loc.print_location_info();
}

void CgiHandler::load_file_resource() {
	this->resource_p = fopen(this->env["PATH_TRANSLATED"].c_str(), "rb");
	char buffer[CGI_RESOURCE_BUFFER_SIZE + 1];
	memset(buffer, 0, CGI_RESOURCE_BUFFER_SIZE + 1);
	int r = 1;
	while ((r = fread(buffer, 1, CGI_RESOURCE_BUFFER_SIZE, this->resource_p)) > 0)
	{
		this->file_resource += buffer;
		memset(buffer, 0, CGI_RESOURCE_BUFFER_SIZE + 1);
	}
	this->env["CONTENT_LENGTH"] = NumberToString(this->file_resource.size());
}

std::string CgiHandler::get_target_file_fullpath(Request& req, Location& loc)
{
	std::string ret;
	char *pwd = getcwd(NULL, 0);
	std::string loc_root = loc.get_root();
	std::string req_path = req.get_path();

	ret += pwd;
	if (loc_root[0] == '.') {
		loc_root = loc_root.substr(1);
	}
	ret += loc_root;
	ret += req_path.substr(loc.get_path().size());
	free(pwd);
	return ret;
};

char** CgiHandler::set_env()
{
	char **envp = new char*[sizeof(char*) * (env.size() + 1)];
	if (!envp)
		return NULL;
	int i = 0;
	for(std::map<std::string, std::string>::iterator it = env.begin(); it != env.end(); ++it)
	{
		envp[i] = strdup((it->first + "=" + it->second).c_str());
		++i;
	}
	envp[i] = NULL;
	return envp;
}

int CgiHandler::excute_CGI(Request &Request, Location &loc)
{
	int read_fd[2];
	int write_fd[2];
	int pid;
	int ret1 = pipe(read_fd);

	if (ret1 < 0 || pipe(write_fd) < 0 || !resource_p) return -1;
	signal(SIGALRM,(void (*)(int))set_signal_kill_child_process);
	pid = fork();
	if (pid < 0) return -1;
	else if (pid == 0)
	{
		dup2(write_fd[0], STDIN_FILENO);
		dup2(read_fd[1], STDOUT_FILENO);
		close(write_fd[1]);
		close(read_fd[0]);
		char **env = set_env();
		std::string extension = Request.get_path().substr(Request.get_path().find(".") + 1);
		char *av[3];
		av[0] = const_cast<char*>(loc.getCgiBinary(extension).c_str());
		av[1] = const_cast<char*>(loc.root.c_str());
		av[2] = NULL;
		if (env)
		{
			ret1 = execve(av[0], av, env);
		}
		exit(1);
	}
	else
	{
		close(write_fd[0]);
		close(read_fd[1]);
		set_pipe_write_fd(write_fd[1]);
		set_pipe_read_fd(read_fd[0]);
		return 0;
	}
}

std::string& CgiHandler::get_file_resource(void) {
	return this->file_resource;
}

int CgiHandler::get_pipe_write_fd(void) {
	return this->pipe_wfd;
};

int CgiHandler::get_pipe_read_fd(void) {
	return this->pipe_rfd;
};

void CgiHandler::set_pipe_write_fd(int fd) {
	this->pipe_wfd = fd;
};

void CgiHandler::set_pipe_read_fd(int fd) {
	this->pipe_rfd = fd;
};

std::string CgiHandler::read_from_CGI_process(int timeout_ms) {
	int rbytes = 1;
	// struct timeval timeout_tv;
	char buf[CGI_READ_BUFFER_SIZE + 1];
	memset(buf, 0, CGI_READ_BUFFER_SIZE + 1);
	std::string ret;

	(void)timeout_ms;
	// timeout_tv.tv_sec = 0;
	// timeout_tv.tv_usec = 1000 * timeout_ms;
	while (rbytes > 0) {
		rbytes = read(this->get_pipe_read_fd(), buf, CGI_READ_BUFFER_SIZE);
		if (rbytes < 0)
			return NULL;
		ret += buf;
		memset(buf, 0, CGI_READ_BUFFER_SIZE + 1);
	}
	return ret;
};

int CgiHandler::write_to_CGI_process() {
	int wbyte = write(
					this->get_pipe_write_fd(), 
					this->file_resource.c_str(), 
					this->file_resource.size()
				);
	if (wbyte == -1)
	{
		fprintf(stderr, "[ERROR] cgihandler : write failed. (%d)%s\n", errno, strerror(errno));
		alarm(30);
		waitpid(-1, NULL, 0);
		return -1;
	}
	else
	{
		signal(SIGALRM, SIG_DFL);
		return wbyte;
	}
};

