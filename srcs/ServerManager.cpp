#include "../includes/ServerManager.hpp"

ServerManager::ServerManager(std::vector<Server> servers)
{
	_servers = servers;

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
	for (unsigned long i = 0; i < _servers.size(); i++)
	{
		std::map<int, std::string>::iterator it;
		for (it = _servers[i].error_pages.begin(); it != _servers[i].error_pages.end(); it++)
		{
			int status_code = it->first;
			if (status_code < 400 || (status_code > 431 && status_code < 500) || status_code > 511)
			{
				fprintf(stderr, "[ERROR] invalid status code on error_page\n");
				exit(1);
			}
		}
		// to find default server
		if (!default_servers[_servers[i].port])
			default_servers[_servers[i].port] = &_servers[i];

		// to identity server name(host)
		if (!this->servers[_servers[i].server_name + ":" + _servers[i].port])
			this->servers[_servers[i].server_name + ":" + _servers[i].port] = &_servers[i];
		else
			std::cout << "! ignore already exist server block !\n";
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
	std::map<std::string, Server*>::iterator it;
	for (it = default_servers.begin(); it != default_servers.end(); it++)
	{
		std::cout << "> Create Server : " << (*it).first << "\n";
		(*it).second->create_socket();
	}
}

void ServerManager::close_servers()
{
	std::map<std::string, Server*>::iterator it;
	for (it = default_servers.begin(); it != default_servers.end(); it++)
		close((*it).second->listen_socket);
}

/*
** Poll Management Methods
*/

void ServerManager::add_fd_selectPoll(int fd, fd_set *fds)
{
	FD_SET(fd, fds);
	if (this->max_fd < fd)
		this->max_fd = fd;
}

void ServerManager::run_selectPoll(fd_set *reads, fd_set *writes)
{
	int ret = 0;

	if ((ret = select(this->max_fd + 1, reads, writes, 0, 0)) < 0)
	{
		fprintf(stderr, "[ERROR] select() failed. (%d)\n", errno);
		if (errno == EINVAL)
		{
			for (unsigned long i = 0; i < clients.size(); i++)
				send_error_page(429, clients[i], NULL);
		}
		else 
		{
			for (unsigned long i = 0; i < clients.size(); i++)
				send_error_page(500, clients[i], NULL);
		}
		exit(1);
	}
	else if (ret == 0)
		fprintf(stderr, "[ERROR] select() timeout. (%d)\n", errno);
	this->reads = *reads;
	this->writes = *writes;
}

/*
** Client methods
*/

void ServerManager::wait_to_client()
{
	// int recv;
	fd_set reads;
	fd_set writes;

	FD_ZERO(&reads);
	FD_ZERO(&writes);
	std::map<std::string, Server*>::iterator it;
	for (it = default_servers.begin(); it != default_servers.end(); it++)
		add_fd_selectPoll((*it).second->listen_socket, &reads);
	for (unsigned long i = 0; i < clients.size(); i++)
		add_fd_selectPoll(clients[i].get_socket(), &reads);
	run_selectPoll(&reads, &writes);
}

void ServerManager::accept_sockets()
{
	int server;
	std::map<std::string, Server*>::iterator it;
	for (it = default_servers.begin(); it != default_servers.end(); it++)
	{
		server = (*it).second->listen_socket;
		if (FD_ISSET(server, &reads))
		{
			clients.push_back(Client((*it).second));
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

void ServerManager::drop_client(Client client)
{
	close(client.get_socket());

	std::cout << "> Drop Client\n";
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
** Request Treatment methods
*/

bool static is_request_done(char *request)
{
	std::cout << "is_request_done: request: " << request << "\n";
	char *body = strstr(request, "\r\n\r\n");
	if (!body)
		return false;
	if (strnstr(request, "chunked", strlen(request) - strlen(body)))
	{
		body += 4;
		if (strstr(body, "\r\n\r\n"))
			return true;
		return false;
	}
	else if (strnstr(request, "Content-Length", strlen(request) - strlen(body)))
	{
		body += 4;
		if (strstr(body, "\r\n\r\n"))
			return true;
		char *start = strnstr(request, "Content-Length: ", strlen(request) - strlen(body)) + 16;
		char *end = strstr(start, "\r\n");
		char *len = strndup(start, end - start);
		int len_i = atoi(len);
		if ((size_t)len_i <= strlen(body))
			return true;
		return false;
	}
	else if (strnstr(request, "boundary=", strlen(request) - strlen(body)))
	{
		body += 4;
		if (strstr(body, "\r\n\r\n"))
			return true;
		return false;
	}
	return true;
}

void ServerManager::treat_request()
{
	for (unsigned long i = 0  ; i < clients.size() ; i++)
	{
		if (FD_ISSET(clients[i].get_socket(), &reads))
		{
			if (MAX_REQUEST_SIZE == clients[i].get_received_size())
			{
				send_error_page(400, clients[i]);
				drop_client(clients[i]);
				i--;
				continue;
			}
			int r = recv(clients[i].get_socket(), 
					clients[i].request + clients[i].get_received_size(), 
					MAX_REQUEST_SIZE - clients[i].get_received_size(), 0);
			clients[i].set_received_size(clients[i].get_received_size() + r);
			if (clients[i].get_received_size() > MAX_REQUEST_SIZE)
			{
				send_error_page(413, clients[i]);
				drop_client(clients[i]);
				i--;
				continue;
			}
			// int recv_size = clients[i].get_received_size();
			// char *reqt = clients[i].request;
			if (r < 0)
			{
				std::cout << "> Unexpected disconnect from (" << r << ")[" << clients[i].get_client_address() << "].\n";
				fprintf(stderr, "[ERROR] recv() failed.\n");
				send_error_page(500, clients[i]);
				drop_client(clients[i]);
				i--;
			}
			else if (r == 0)
			{
				std::cout << "> The connection has been closed. (" << r << ")[" << clients[i].get_client_address() << "].\n";
				fprintf(stderr, "[ERROR] recv() failed.\n");
				send_error_page(400, clients[i]);
				drop_client(clients[i]);
				i--;
			}
			else if (is_request_done(clients[i].request))
			{
				Request req = Request(clients[i].get_socket());
				int error_code;
				if ((error_code = req.parsing(clients[i].request)))
				{
					send_error_page(error_code, clients[i]);
					drop_client(clients[i]);
					i--;
					continue;
				}

				std::string port = req.headers["Host"].substr(req.headers["Host"].find(':') + 1);
				std::cout << "> " << req.headers["Host"];
				if (servers[req.headers["Host"]])
				{
					std::cout << " -> found in server name\n";
					clients[i].server = servers[req.headers["Host"]];
				}
				else if (default_servers[port])
				{
					std::cout << " -> default server\n";
					clients[i].server = default_servers[port];
				}
				else
				{
					std::cout << " -> not found\n";
					send_error_page(400, clients[i]);
					drop_client(clients[i]);
					i--;
					continue;
				}
				if (req.headers.find("Content-Length") != req.headers.end() && 
				stoi(req.headers["Content-Length"]) > clients[i].server->client_body_limit)
				{
					send_error_page(413, clients[i]);
					drop_client(clients[i]);
					i--;
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
					i--;
					continue;
				}
				
				if (loc && is_cgi(&req, loc))
				{
					std::cout << "cgi\n";
					CgiHandler cgi(req, *loc);
					int cgi_ret;
					int read_fd = cgi.excute_CGI(req, *loc);
					if (read_fd == -1)
						send_error_page(404, clients[i]);
					else if (is_response_timeout(clients[i]) == true)
						send_error_page(408, clients[i]);
					else if ((cgi_ret = send_cgi_response(clients[i], cgi)))
						send_error_page(cgi_ret, clients[i]);
				}
				else
				{
					if (is_response_timeout(clients[i]) == true)
						send_error_page(408, clients[i]);
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
				i--;
				std::cout << "> Request completed\n";
			}
		}
	}
}

/*
** HTTP Methods
*/

void ServerManager::get_method(Client &client, std::string path)
{
	std::cout << "GET method\n";
	if (path.length() >= MAX_URI_SIZE)
	{
		send_error_page(414, client);
		return;
	}

	// char *dir_list;
	struct stat buf;
	std::string full_path = find_path_in_root(path, client);
	lstat(full_path.c_str(), &buf);
	FILE *fp = fopen(full_path.c_str(), "rb");
	std::cout << "> " + full_path + ", " + (fp == NULL ? "not found\n" : "found\n");
	if (!fp)
		send_error_page(404, client);
	else
	{
		if (S_ISDIR(buf.st_mode))
		{
			std::cout << "> Current path is directory\n";
			if (client.server->autoindex)
			{
				send_autoindex_page(client, path);
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
				for (unsigned long i = 0; i < indexes.size(); i++)
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
					send_error_page(404, client);
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
		int send_ret;
		while (r)
		{
			send_ret = send(client.get_socket(), buffer, r, 0);
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
		send_error_page(411, client);
		return;
	}

	std::string full_path = find_path_in_root(request.path, client);
	size_t index = full_path.find_last_of("/");
	if (index == std::string::npos)
	{
		send_error_page(500, client);
		return;
	}
	std::string file_name = full_path.substr(index + 1);
	std::string folder_path = full_path.substr(0, index);

	std::string command = "mkdir -p " + folder_path;
	system(command.c_str());
	FILE *fp = fopen(full_path.c_str(), "w");
	if (!fp)
	{
		send_error_page(500, client);
		return;
	}

	fwrite(request.body.c_str(), request.body.size(), 1, fp);
	fclose(fp);
	
	Response response(status_info[201]);
	response.append_header("Connection", "close");
	std::string header = response.make_header();
	int send_ret = send(client.get_socket(), header.c_str(), header.size(), 0);
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
		send_error_page(204, client);
		return;
	}
	fclose(fp);

	std::remove(full_path.c_str());
	Response response(status_info[200]);
	response.append_header("Connection", "close");

	std::string header = response.make_header();
	int send_ret = send(client.get_socket(), header.c_str(), header.size(), 0);
	if (send_ret < 0)
		send_error_page(500, client, NULL);
	else if (send_ret == 0)
		send_error_page(400, client, NULL);
	std::cout << "> " << full_path << " deleted\n";
}

/*
** Response Methods
*/

void ServerManager::send_autoindex_page(Client &client, std::string path)
{
	std::cout << "> Send autoindex page\n";
	std::string addr = find_path_in_root(path, client);
	std::string result = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\" />"
		"<title>webserv</title></head><body><h1>webserv</h1><h2>Index of ";
	result += path;
	result += "<hr><div>";

	DIR *dir = NULL;
	if (path[path.size() - 1] != '/')
		path += "/";
	if ((dir = opendir(addr.c_str())) == NULL)
		return;

	struct dirent *file = NULL;
	while ((file = readdir(dir)) != NULL)
	{
		if (strcmp(file->d_name, ".") || strcmp(file->d_name, ".."))
			result += "<a href=\"" + path + file->d_name;
		else if (path[path.length() - 1] == '/')
			result += "<a href=\"" + path + file->d_name;
		else
			result += "<a href=\"" + path + file->d_name;
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
	
	int send_ret = send(client.get_socket(), header.c_str(), header.size(), 0);
	if (send_ret < 0)
	{
		send_error_page(500, client, NULL);
		return;
	}
	else if (send_ret == 0)
	{
		send_error_page(400, client, NULL);
		return;
	}
	send_ret = send(client.get_socket(), result.c_str(), result.length(), 0);
	if (send_ret < 0)
		send_error_page(500, client, NULL);
	else if (send_ret == 0)
		send_error_page(400, client, NULL);
}

void ServerManager::send_redirection(Client &client, std::string request_method)
{
	(void) request_method;
	std::cout << ">> send redirection response\n";
	Response response(status_info[client.server->redirect_status]);
	if (client.server->redirect_status == 300)
		response.make_status_body(client.server->redirect_url);
	else
		response.make_status_body();
	response.append_header("Server", client.server->server_name);
	response.append_header("Date", get_current_date_GMT());
	response.append_header("Content-Type", "text/html");
	response.append_header("Content-Length", std::to_string(response.get_body_size()));
	// response.append_header("Connection", "keep-alive");
	response.append_header("Location", client.server->redirect_url);

	std::string result = response.serialize();
	int send_ret = send(client.get_socket(), result.c_str(), result.size(), 0);
	if (send_ret < 0)
		send_error_page(500, client, NULL);
	else if (send_ret == 0)
		send_error_page(400, client, NULL);
}

void ServerManager::send_error_page(int code, Client &client, std::vector<MethodType> *allow_methods)
{
	std::cout << "> Send error page(" << code << ")\n";
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
		for (unsigned long i = 0; i < (*allow_methods).size(); i++)
		{
			allowed_method_list += methodtype_to_s((*allow_methods)[i]);
			if (i < (*allow_methods).size() - 1)
				allowed_method_list += ", ";
		}
		response.append_header("Allow", allowed_method_list);
	}

	std::string result = response.serialize();
	int send_ret = send(client.get_socket(), result.c_str(), result.size(), 0);
	if (send_ret < 0)
		std::cerr << "> Unexpected disconnect!\n";
	else if (send_ret == 0)
		std::cerr << "> The connection has been closed or 0 bytes were passed to send()!\n";
}

/*
** CGI Methods
*/

static void set_signal_kill_child_process(int sig)
{
	(void) sig;
    kill(-1,SIGKILL);
}

void ServerManager::create_cgi_msg(Response& res, std::string& cgi_ret, Client &client)
{
	std::stringstream ss(cgi_ret);
	size_t tmpi;
	std::string tmp;
	std::string body;

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
	while (getline(ss, tmp, '\n')) {
		body += tmp;
		body += "\n";
	}
	res.set_body(body);
	res.append_header("Content-Length", std::to_string(res.get_body_size()));
}

int ServerManager::send_cgi_response(Client& client, CgiHandler& ch)
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
		int send_ret = send(client.get_socket(), result.c_str(), result.size(), 0);
		if (send_ret < 0)
			return 500;
		else if (send_ret == 0)
			return 400;
		else
			std::cout << ">> cgi responsed\n";
	}
	return 0;
}
