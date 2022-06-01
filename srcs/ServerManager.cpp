#include "../includes/ServerManager.hpp"

ServerManager::ServerManager(std::vector<Server> servers)
{
	this->servers = servers;

	status_info.insert(std::make_pair(200, "200 OK"));
	status_info.insert(std::make_pair(201, "201 Created"));
	status_info.insert(std::make_pair(204, "204 No Content"));
	status_info.insert(std::make_pair(300, "300 Multiple Choices"));
	status_info.insert(std::make_pair(301, "301 Moved Permanently"));
	status_info.insert(std::make_pair(302, "302 Found"));
	status_info.insert(std::make_pair(303, "303 See Other"));
	status_info.insert(std::make_pair(307, "307 Temporary Redirect"));
	status_info.insert(std::make_pair(400, "400 Bad Request"));
	status_info.insert(std::make_pair(401, "401 Unauthorized"));
	status_info.insert(std::make_pair(403, "403 Forbidden"));
	status_info.insert(std::make_pair(404, "404 Not found"));
	status_info.insert(std::make_pair(405, "405 Method Not Allowed"));
	status_info.insert(std::make_pair(408, "408 Request Timeout"));
	status_info.insert(std::make_pair(411, "411 Length Required"));
	status_info.insert(std::make_pair(413, "413 Request Entity Too Large"));
	status_info.insert(std::make_pair(414, "414 URI Too Long"));
	status_info.insert(std::make_pair(417, "417 Expectation Failed"));
	status_info.insert(std::make_pair(429, "429 Too Many Request"));
	status_info.insert(std::make_pair(500, "500 Internal Server Error"));
	status_info.insert(std::make_pair(502, "502 Bad Gateway"));
	status_info.insert(std::make_pair(504, "504 Gateway Timeout"));
	status_info.insert(std::make_pair(505, "505 HTTP Version Not Supported"));
	max_fd = -1;
	for (int i = 0; i < servers.size(); i++)
	{
		std::map<int, std::string>::iterator it;
		for (it = servers[i].error_pages.begin(); it != servers[i].error_pages.end(); it++)
		{
			int status_code = it->first;
			if (status_code < 400 || (status_code > 431 && status_code < 500) || status_code > 511)
			{
				fprintf(stderr, "[ERROR] invalid status code on error_page\n");
				exit(1);
			}
		}
	}
}

ServerManager::~ServerManager()
{
}

/*
** Server methods
*/

void ServerManager::create_servers()
{
	for (int i = 0; i < servers.size(); i++)
	{
		servers[i].create_socket();
	}
}

void ServerManager::accept_sockets()
{
	int server;
	for (int i = 0; i < servers.size(); i++)
	{
		for (int j = 0; j < servers[i].listen_socket.size(); j++)
		{
			server = servers[i].listen_socket[j];
			if (FD_ISSET(server, &reads))
			{
				clients.push_back(Client(&servers[i]));
				Client &client = clients.back();
				client.set_socket(accept(server, (struct sockaddr*)&(client.address), &(client.address_length)));
				if (client.get_socket() < 0)
				{
					fprintf(stderr, "[ERROR] accept() failed. (%d)\n", errno);
					exit(1);
				}
				std::cout << "> New Connection from [" << client.get_client_address() << "].\n";
			}
		}
	}
}

void ServerManager::close_servers()
{
	for (int i = 0; i < servers.size(); i++)
	{
		for (int j = 0; j < servers[i].listen_socket.size(); j++)
			close(servers[i].listen_socket[j]);
	}
}

void ServerManager::print_servers_info()
{
	std::cout << "\n=================================================\n";
	std::cout << "            Total Server Informations            \n";
	std::cout << "=================================================\n";
	for (int i = 0; i < servers.size(); i++)
	{
		servers[i].print_server_info();
	}
	std::cout << "=================================================\n\n";
}

/*
** Client methods
*/

void ServerManager::add_fd_selectPoll(int fd, fd_set *fds)
{
	FD_SET(fd, fds);
	if (this->max_fd < fd) this->max_fd = fd;
}

void ServerManager::run_selectPoll(fd_set *reads, fd_set *writes)
{
	int ret = 0;

	if ((ret = select(this->max_fd + 1, reads, writes, 0, 0)) < 0)
	{
		fprintf(stderr, "[ERROR] select() failed. (%d)\n", errno);
		if (errno == EINVAL)
		{
			for (int i = 0; i < clients.size(); i++)
				send_error_page(429, clients[i], NULL);
		}
		else 
		{
			for (int i = 0; i < clients.size(); i++)
				send_error_page(500, clients[i], NULL);
		}
		exit(1);
	} else if (ret == 0) {
		fprintf(stderr, "[ERROR] select() timeout. (%d)\n", errno);
	}
	this->reads = *reads;
	this->writes = *writes;
}


void ServerManager::wait_to_client()
{
	int recv;
	fd_set reads;
	fd_set writes;

	FD_ZERO(&reads);
	FD_ZERO(&writes);
	for (int i = 0; i < servers.size(); i++)
	{
		for (int j = 0; j < servers[i].listen_socket.size(); j++)
		{
			add_fd_selectPoll(servers[i].listen_socket[j], &reads);
		}
	}
	for (int i = 0; i < clients.size(); i++)
	{
		add_fd_selectPoll(clients[i].get_socket(), &reads);
	}
	run_selectPoll(&reads, &writes);
}

void ServerManager::drop_client(Client client)
{
	close(client.get_socket());

	std::cout << "drop client!\n";
	std::vector<Client>::iterator iter;
	for (iter = clients.begin(); iter != clients.end(); iter++)
	{
		if ((*iter).get_socket() == client.get_socket())
		{
			clients.erase(iter);
			return;
		}
	}
	fprintf(stderr, "[ERROR] drop_client not found.\n");
	exit(1);
}

/*
** Response methods
*/
bool ServerManager::handle_CGI(Request *request, Location *loc)
{
	std::cout << "handle_cgi\n";
	for (std::map<std::string, std::string>::iterator it = loc->cgi_info.begin();
	it != loc->cgi_info.end(); it++)
	{
		std::cout << "get_path: " << request->get_path() << "\n";
		std::cout << "it->first: " << it->first << "\n"; 
		if (request->get_path().find(it->first) != std::string::npos)
			return true;
	}
	return false;
}

void ServerManager::treat_request()
{
	for (int i = 0  ; i < clients.size() ; i++)
	{
		if (FD_ISSET(clients[i].get_socket(), &reads))
		{
			if (MAX_REQUEST_SIZE == clients[i].get_received_size())
			{
				send_error_page(400, clients[i], NULL);
				drop_client(clients[i]);
				continue;
			}
			int r = recv(clients[i].get_socket(), 
					clients[i].request + clients[i].get_received_size(), 
					MAX_REQUEST_SIZE - clients[i].get_received_size(), 0);
			clients[i].set_received_size(clients[i].get_received_size() + r);
			int recv_size = clients[i].get_received_size();
			char *reqt = clients[i].request;
			if (r < 0)
			{
				std::cout << "> Unexpected disconnect from (" << r << ")[" << clients[i].get_client_address() << "].\n";
				fprintf(stderr, "[ERROR] recv() failed.\n");
				send_error_page(500, clients[i], NULL);
				drop_client(clients[i]);
				i--;
			}
			else if (r == 0)
			{
				std::cout << "> The connection has been closed or 0 bytes were passed to send() (" << r << ")[" << clients[i].get_client_address() << "].\n";
				fprintf(stderr, "[ERROR] recv() failed.\n");
				send_error_page(400, clients[i], NULL);
				drop_client(clients[i]);
				i--;
			}
			else if (strstr(reqt, "\r\n\r\n"))
			{
				Request req = Request(clients[i].get_socket());
				int error_code;
				if ((error_code = req.parsing(clients[i].request)))
				{
					send_error_page(error_code, clients[i], NULL);
					drop_client(clients[i]);
					continue;
				}
				
				if (req.headers.find("Content-Length") != req.headers.end() && 
				stoi(req.headers["Content-Length"]) > clients[i].server->client_body_limit)
				{
					send_error_page(413, clients[i], NULL);
					drop_client(clients[i]);
					continue;
				}

				clients[i].request[clients[i].get_received_size()] = 0;

				Location* loc = clients[i].server->get_cur_location(req.get_path());
				std::vector<MethodType> method_list;
				method_list = loc ? loc->allow_methods : clients[i].server->allow_methods;
				if (!is_allowed_method(method_list, req.method))
				{
					send_error_page(405, clients[i], &method_list);
					drop_client(clients[i]);
					continue;
				}
				
				if (loc && handle_CGI(&req, loc))
				{
					std::cout << "cgi\n";
					size_t cgi_res;
					CgiHandler cgi(req, *loc);
					int read_fd = cgi.excute_CGI(req, *loc);
					if (read_fd == -1)
						send_error_page(404, clients[i], NULL);
					else if (is_response_timeout(clients[i]) == true)
						send_error_page(408, clients[i], NULL);
					else if ((cgi_res = send_cgi_response(clients[i], cgi)))
						send_error_page(cgi_res, clients[i], NULL);
				}
				else
				{
					// if (is_response_timeout(clients[i]) == true)
					// 	send_error_page(408, clients[i], NULL);
					if (clients[i].server->redirect_status != -1)
						send_redirection(clients[i], req.method);
					else if (req.method == "GET")
						get_method(clients[i], req.path);
					else if (req.method == "POST")
						post_method(clients[i], req);
					else if (req.method == "DELETE")
						delete_method(clients[i], req.path);
				}
				drop_client(clients[i]);
			}
		}
	}
}

static void set_signal_kill_child_process(int sig)
{
    kill(-1,SIGKILL);
}

size_t ServerManager::send_cgi_response(Client& client, CgiHandler& ch)
{
	this->add_fd_selectPoll(ch.get_pipe_write_fd(), &(this->writes));
	this->run_selectPoll(&(this->reads), &(this->writes));
	if (FD_ISSET(ch.get_pipe_write_fd(), &(this->writes)) == 0) 
	{
		fprintf(stderr, "[ERROR] writing input to cgi failed. (%d)%s\n", errno, strerror(errno));
		close(ch.get_pipe_read_fd());
		signal(SIGALRM, set_signal_kill_child_process);
		alarm(30);
		signal(SIGALRM, SIG_DFL);
		close(ch.get_pipe_write_fd());
		return 500;
	}
	ch.write_to_CGI_process();
	FD_ZERO(&this->writes);
	this->add_fd_selectPoll(ch.get_pipe_read_fd(), &(this->reads));
	this->run_selectPoll(&(this->reads), &(this->writes));
	if (FD_ISSET(ch.get_pipe_read_fd(), &(this->reads)) == 0)
	{
		fprintf(stderr, "[ERROR] reading from cgi failed. (%d)%s\n", errno, strerror(errno));
		close(ch.get_pipe_read_fd());
		close(ch.get_pipe_write_fd());
		return 500;
	}
	std::string cgi_ret = ch.read_from_CGI_process(10);
	if (cgi_ret.empty())
		return 500;
	close(ch.get_pipe_read_fd());
	close(ch.get_pipe_write_fd());
	std::cout << "successfully read\n";
	if (cgi_ret.compare("cgi: failed") == 0) return 400;
	else
	{
		Response res(status_info[atoi(get_status_cgi(cgi_ret).c_str())]);
		create_cgi_msg(res, cgi_ret, client);
		std::string result = res.serialize();
		ssize_t send_ret = send(client.get_socket(), result.c_str(), result.size(), 0);
		if (send_ret < 0)
			return 500;
		else if (send_ret == 0)
			return 400;
		else
			std::cout << ">> cgi responsed\n";
	}
	return 0;
}

std::string ServerManager::get_status_cgi(std::string& cgi_ret)
{
	std::string status_line;
	std::stringstream ss(cgi_ret);

	getline(ss, status_line, '\n');
	cgi_ret.erase(0, status_line.length() + 1);
	status_line.erase(0, 8);
	status_line.erase(status_line.length() - 1, 1);
	return status_line;
}

void ServerManager::create_cgi_msg(Response& res, std::string& cgi_ret, Client &client)
{
	std::stringstream ss(cgi_ret);
	size_t tmpi;
	std::string tmp;

	res.append_header("Server", client.server->server_name);
	res.append_header("Connection", "close");
	while (getline(ss, tmp, '\n')) {
		if (tmp.length() == 1 && tmp[0] == '\r') break ;
		size_t mid_deli = tmp.find(":");
		size_t end_deli = tmp.find("\n");
		if (tmp[end_deli] == '\r') {
			tmp.erase(tmp.length() - 1, 1);
			end_deli -= 1;
		}
		if ((tmpi = tmp.find(";")) != std::string::npos) {
			tmp = tmp.substr(0, tmpi);
		}
		std::string key = tmp.substr(0, mid_deli);
		std::string value = tmp.substr(mid_deli + 1, end_deli);
		res.append_header(key, value);
	}
	getline(ss, tmp, '\n');
	tmp.append("\n");
	res.set_body(tmp);
	res.append_header("Content-Length", std::to_string(res.get_body_size()));
}

bool ServerManager::is_response_timeout(Client& client)
{
	static timeval tv;
	
	gettimeofday(&tv, NULL);
	if (tv.tv_sec - client.get_last_time().tv_sec > client.server->recv_timeout.tv_sec) return true;
	client.set_last_time_sec(tv);
	return false;
}

void ServerManager::send_redirection(Client &client, std::string request_method)
{
	std::cout << ">> send redirection response\n";
	size_t send_ret;
	Response response(status_info[client.server->redirect_status]);
	if (client.server->redirect_status == 300)
		response.make_status_body(client.server->redirect_url);
	else
		response.make_status_body();
	response.append_header("Server", client.server->server_name);
	response.append_header("Date", get_current_date_GMT());
	response.append_header("Content-Type", "text/html");
	response.append_header("Content-Length", std::to_string(response.get_body_size()));
	response.append_header("Connection", "keep-alive");
	response.append_header("Location", client.server->redirect_url);

	std::string result = response.serialize();
	send(client.get_socket(), result.c_str(), result.size(), 0);
	if (send_ret < 0)
		send_error_page(500, client, NULL);
	else if (send_ret == 0)
		send_error_page(400, client, NULL);
}

void ServerManager::send_error_page(int code, Client &client, std::vector<MethodType> *allow_methods)
{
	std::cout << "> send error page\n";
	std::ifstream page;

	if (client.server->error_pages.find(code) != client.server->error_pages.end())
	{
		page.open(client.server->error_pages[code]);
		if (!page.is_open())
			code = 404;
	}
	Response response(status_info[code]);
	if (page.is_open())
	{
		std::string body;
		std::string line;
		while (!page.eof())
		{
			getline(page, line);
			body += line;
			body += '\n';
		}
		response.body = body;
		page.close();
	}
	else
		response.make_status_body();
	response.append_header("Connection", "close");
	response.append_header("Content-Length", std::to_string(response.get_body_size()));
	response.append_header("Content-Type", "text/html");
	if (code == 405)
	{
		std::string allowed_method_list;
		for (int i = 0; i < (*allow_methods).size(); i++)
		{
			allowed_method_list += methodtype_to_s((*allow_methods)[i]);
			if (i < (*allow_methods).size() - 1)
				allowed_method_list += ", ";
		}
		response.append_header("Allow", allowed_method_list);
	}

	std::string result = response.serialize();
	size_t send_ret = send(client.get_socket(), result.c_str(), result.size(), 0);
	if (send_ret < 0)
		std::cerr << "> Unexpected disconnect!\n";
	else if (send_ret == 0)
		std::cerr << "> The connection has been closed or 0 bytes were passed to send()!\n";
}

int	ServerManager::is_allowed_method(std::vector<MethodType> allow_methods, std::string method) 
{
	if (method == "GET")
		return true;
	for (std::vector<MethodType>::iterator it = allow_methods.begin(); 
	it != allow_methods.end(); it++)
	{
		if (method == methodtype_to_s(*it))
			return true;
	}
	return false;
}

std::string ServerManager::methodtype_to_s(MethodType method) {
	if (method == GET)
		return "GET";
	else if (method == POST)
		return "POST";
	else if (method == DELETE)
		return "DELETE";
	return "";
}

/*
** http methods
*/

bool is_loc_check(std::string path, Client &client)
{
	Location *cur_loc = client.server->get_cur_location(path);
	if (!cur_loc)
		return false;
	std::string root = cur_loc->path;
	if (path == root)
		return true;
	return false;
}

void ServerManager::get_method(Client &client, std::string path)
{
	std::cout << "GET method\n";
	if (path.length() >= MAX_URI_SIZE)
	{
		send_error_page(414, client, NULL);
		return;
	}

	char *dir_list;
	struct stat buf;
	std::string full_path = find_path_in_root(path, client);
	lstat(full_path.c_str(), &buf);
	FILE *fp = fopen(full_path.c_str(), "rb");
	std::cout << "> " + full_path + ", " + (fp == NULL ? "not found\n" : "found\n");
	if (!fp)
		send_error_page(404, client, NULL);
	else
	{
		if (S_ISDIR(buf.st_mode))
		{
			if (client.server->autoindex)
			{
				std::cout << "autoindex true\n";
				get_autoindex_page(client, path);
				fclose(fp);
				return;
			}
			else
			{
				bool flag = false;
				Location *loc = client.server->get_cur_location(path);
				std::vector<std::string> indexes;
				if (loc)
					indexes = loc->index;
				else
					indexes = client.server->index;
				if (full_path.back() != '/')
					full_path.append("/");
				for (int i = 0; i < indexes.size(); i++)
				{
					FILE *fp = fopen((full_path + indexes[i]).c_str(), "rb");
					if (fp)
					{
						fclose(fp);
						full_path.append(indexes[i]);
						flag = true;
						break;
					}
				}
				if (!flag)
				{
					send_error_page(404, client, NULL);
					fclose(fp);
					return;
				}
			}
		}
		fclose(fp);
		fp = fopen(full_path.c_str(), "rb");
		fseek(fp, 0L, SEEK_END);
		size_t length = ftell(fp);
		rewind(fp);
		const char *type = find_content_type(full_path.c_str());

		Response response(status_info[200]);
		response.append_header("Connection", "close");
		response.append_header("Content-Length", std::to_string(length));
		response.append_header("Content-Type", type);

		std::string header = response.make_header();
		send(client.get_socket(), header.c_str(), header.size(), 0);

		char buffer[BSIZE];
		int r = fread(buffer, 1, BSIZE, fp);
		size_t send_ret;
		while (r)
		{
			send(client.get_socket(), buffer, r, 0);
			if (send_ret < 0)
			{
				send_error_page(500, client, NULL);
				break;
			}
			else if (send_ret == 0)
			{
				send_error_page(400, client, NULL);
				break;
			}
			r = fread(buffer, 1, BSIZE, fp);
		}
	}
	fclose(fp);
}

void ServerManager::post_method(Client &client, Request &request)
{
	std::cout << "POST method\n";

	if (request.headers.find("Content-Length") == request.headers.end())
	{
		send_error_page(411, client, NULL);
		return;
	}

	std::string full_path = find_path_in_root(request.path, client);
	size_t index = full_path.find_last_of("/");
	if (index == std::string::npos)
	{
		send_error_page(500, client, NULL);
		return;
	}
	std::string file_name = full_path.substr(index + 1);
	std::string folder_path = full_path.substr(0, index);

	std::string command = "mkdir -p " + folder_path;
	system(command.c_str());
	FILE *fp = fopen(full_path.c_str(), "w");
	if (!fp)
	{
		send_error_page(500, client, NULL);
		return;
	}

	fwrite(request.body.c_str(), request.body.size(), 1, fp);
	fclose(fp);
	
	Response response(status_info[201]);
	response.append_header("Connection", "close");
	std::string header = response.make_header();
	size_t send_ret = send(client.get_socket(), header.c_str(), header.size(), 0);
	if (send_ret < 0)
		send_error_page(500, client, NULL);
	else if (send_ret == 0)
		send_error_page(400, client, NULL);
	std::cout << "> " << full_path << " posted\n";
}

void ServerManager::delete_method(Client &client, std::string path)
{
	std::cout << "DELETE method\n";
	std::string full_path = find_path_in_root(path, client);
	std::cout << full_path << "\n";

	FILE *fp = fopen(full_path.c_str(), "r");
	if (!fp)
	{
		send_error_page(204, client, NULL);
		return;
	}
	fclose(fp);

	std::remove(full_path.c_str());
	Response response(status_info[200]);
	response.append_header("Connection", "close");

	std::string header = response.make_header();
	size_t send_ret = send(client.get_socket(), header.c_str(), header.size(), 0);
	if (send_ret < 0)
		send_error_page(500, client, NULL);
	else if (send_ret == 0)
		send_error_page(400, client, NULL);
	std::cout << "> " << full_path << " deleted\n";
}

/*
** helper methods
*/

const char *ServerManager::find_content_type(const char *path)
{
	const char *last_dot = strrchr(path, '.');
	if (last_dot) {
		if (strcmp(last_dot, ".css") == 0) return "text/css";
		if (strcmp(last_dot, ".csv") == 0) return "text/csv";
		if (strcmp(last_dot, ".html") == 0) return "text/html";
		if (strcmp(last_dot, ".js") == 0) return "application/javascript";
		if (strcmp(last_dot, ".json") == 0) return "application/json";
		if (strcmp(last_dot, ".pdf") == 0) return "application/pdf";
		if (strcmp(last_dot, ".gif") == 0) return "image/gif";
		if (strcmp(last_dot, ".jpeg") == 0) return "image/jpeg";
		if (strcmp(last_dot, ".jpg") == 0) return "image/jpeg";
		if (strcmp(last_dot, ".png") == 0) return "image/png";
		if (strcmp(last_dot, ".svg") == 0) return "image/svg+xml";
	}
	return "text/plain";
}

std::string ServerManager::find_path_in_root(std::string path, Client &client)
{
	std::string full_path = "";
	std::string location;
	full_path.append(client.get_root_path(path));
	Location *loc = client.server->get_cur_location(path);
	if (loc)
		location = loc->path;
	else
		location = "";
	std::string str = path.substr(location.length());
	full_path.append(str);
	return full_path;
}

void ServerManager::get_autoindex_page(Client &client, std::string path)
{
	std::cout << "path: " << path << "\n";
	std::cout << "root path: " << client.get_autoindex_root_path(path) << "\n";
	std::string addr = client.get_autoindex_root_path(path);
	std::string result = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\" />"
		"<title>webserv</title></head><body><h1>webserv</h1><h2>Index of ";
	result += path;
	result += "<hr><div>";

	DIR *dir = NULL;
	if ((dir = opendir(addr.c_str())) == NULL)
		return;

	struct dirent *file = NULL;
	while ((file = readdir(dir)) != NULL)
	{
		std::cout << "file name: " << file->d_name << "\n";
		if (file->d_name == "." || file->d_name == "..")
			result += "<a href=\"" + path + "/" + file->d_name;
		else if (path[path.length() - 1] == '/')
			result += "<a href=\"" + path + file->d_name;
		else
			result += "<a href=\"" + path + "/" + file->d_name;
		result += (file->d_type == DT_DIR ? "/" : "") + (std::string)"\">";
		result += (std::string)(file->d_name) + (file->d_type == DT_DIR ? "/" : "") + "</a><br>";
	}
	closedir(dir);

	result += "</div></body></html>";

	Response response(status_info[200]);
	response.append_header("Connection", "close");
	response.append_header("Content-Length", std::to_string(result.length()));
	response.append_header("Content-Type", "text/html");
	std::string header = response.make_header();
	
	send(client.get_socket(), header.c_str(), header.size(), 0);
	send(client.get_socket(), result.c_str(), result.length(), 0);
}