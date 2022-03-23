#include "../includes/ConfigParser.hpp"

ConfigParser::ConfigParser(const char* filename)
{
	std::string read;
	std::ifstream fs;

	content.clear();
	fs.open(filename);
	if (fs.is_open())
	{
		while (!fs.eof())
		{
			getline(fs, read);
			content.append(read + '\n');
		}
		fs.close();
	}
}

ConfigParser::~ConfigParser()
{
}

std::vector<Server> *ConfigParser::parse()
{
	std::vector<Server> *result = new std::vector<Server>();

	std::cout << "============= Config Parsing Start ==============" << std::endl;
	size_t pre = 0;
	size_t cur = content.find_first_not_of(" \t\n", pre);
	while (cur != std::string::npos)
	{
		pre = content.find_first_not_of(" \t\n", cur);;
		cur = content.find_first_of(" \t\n", pre);
		std::string key = content.substr(pre, cur - pre);
		if (key != "server")
		{
			exit(print_parse_error());
		}
		Server server = parse_server(&cur);
		result->push_back(server);
	}
	std::cout << "============= Config Parsing Finish =============" << std::endl;
	return result;
}

Server ConfigParser::parse_server(size_t *i)
{
	Server result;

	std::cout << "parse_server.. starts with (" << *i << ")" << std::endl;
	size_t pre = content.find_first_not_of(" \t\n", *i);
	if (pre == std::string::npos || content[pre] != '{')
		exit(print_parse_error());
		
	pre++;
	size_t cur = content.find_first_not_of(" \t\n", pre);
	while (cur != std::string::npos)
	{
		if ((pre = content.find_first_not_of(" \t\n", cur)) == std::string::npos)
				exit(print_parse_error());
		if ((cur = content.find_first_of(" \t\n", pre)) == std::string::npos)
				exit(print_parse_error());
		std::string key = content.substr(pre, cur - pre);
		if (key == "}")
		{
			*i = content.find_first_not_of(" \n\t", cur + 1);
			break;
		}

		if (key == "location")
		{
			result.locations.push_back(parse_location(&cur));
		}
		else
		{
			if ((pre = content.find_first_not_of(" \t\n", cur)) == std::string::npos)
					exit(print_parse_error());
			if ((cur = content.find_first_of("\n", pre)) == std::string::npos)
					exit(print_parse_error());
			std::string value = content.substr(pre, cur - pre);
			if (set_server_values(&result, key, value) == FAILED)
			{
				exit(print_parse_error());
			}
		}
	}
	std::cout << "parse_server.. finish with (" << *i << ")" << std::endl;
	return result;
}

Location ConfigParser::parse_location(size_t *i)
{
	Location result;

	std::cout << "parse_location.. starts with (" << *i << ")" << std::endl;
	size_t pre = content.find_first_not_of(" \t\n", *i);
	size_t cur = content.find_first_of(" \t\n", pre);
	result.path = content.substr(pre, cur - pre);

	pre = content.find_first_not_of(" \t\n", cur);
	if (pre == std::string::npos || content[pre] != '{')
		exit(print_parse_error());
		
	pre++;
	cur = content.find_first_not_of(" \t\n", pre);
	while (cur != std::string::npos)
	{
		if ((pre = content.find_first_not_of(" \t\n", cur)) == std::string::npos)
				exit(print_parse_error());
		if ((cur = content.find_first_of(" \t\n", pre)) == std::string::npos)
				exit(print_parse_error());
		std::string key = content.substr(pre, cur - pre);
		if (key == "}")
		{
			*i = cur;
			break;
		}
		else
		{
			if ((pre = content.find_first_not_of(" \t\n", cur)) == std::string::npos)
					exit(print_parse_error());
			if ((cur = content.find_first_of("\n", pre)) == std::string::npos)
					exit(print_parse_error());
			std::string value = content.substr(pre, cur - pre);
			if (set_location_values(&result, key, value) == FAILED)
			{
				exit(print_parse_error());
			}
		}
	}
	std::cout << "parse_location.. finish with (" << *i << ")" << std::endl;
	return result;
}

int ConfigParser::set_server_values(Server *server, const std::string key, const std::string value)
{
	if (key == "server_name")
	{
		server->server_name = value;
	}
	else if (key == "listen")
	{
		std::vector<std::string> tmp = split(value, ':');
		if (server->host != "" && server->host != tmp[0])
			return FAILED;
		server->host = tmp[0];
		server->port.push_back(tmp[1]);
	}
	else if (key == "root")
	{
		server->root = value;
	}
	else if (key == "index")
	{
		std::vector<std::string> tmp = split(value, ' ');
		for (int i = 0; i != tmp.size(); i++)
			server->index.push_back(tmp[i]);
	}
	else if (key == "allow_methods")
	{
		std::vector<std::string> tmp = split(value, ' ');
		for (int i = 0; i != tmp.size(); i++)
			server->allow_methods.push_back(Server::s_to_methodtype(tmp[i]));
	}
	else if (key == "autoindex")
	{
		server->autoindex = value == "on" ? true : false;
	}
	else if (key == "client_body_limit")
	{
		server->client_body_limit = atoi(value.c_str());
	}
	else if (key == "return")
	{
		std::vector<std::string> tmp = split(value, ' ');
		server->redirect_status = atoi(tmp[0].c_str());
		server->redirect_url = tmp[1];
	}
	else
	{
		return FAILED;
	}
	return SUCCESS;
}

int ConfigParser::set_location_values(Location *location, const std::string key, const std::string value)
{
	if (key == "root")
	{
		location->root = value;
	}
	else if (key == "index")
	{
		std::vector<std::string> tmp = split(value, ' ');
		for (int i = 0; i != tmp.size(); i++)
			location->index.push_back(tmp[i]);
	}
	else if (key == "allow_methods")
	{
		std::vector<std::string> tmp = split(value, ' ');
		for (int i = 0; i != tmp.size(); i++)
			location->allow_methods.push_back(Location::s_to_methodtype(tmp[i]));
	}
	else if (key == "cgi_info")
	{
		int i = value.find_first_of(" ");
		if (i == std::string::npos)
			return FAILED;
		location->cgi_info[value.substr(0, i)] = value.substr(i + 1, value.length());
	}
	else
	{
		return FAILED;
	}
	return SUCCESS;
}

int ConfigParser::print_parse_error()
{
	fprintf(stderr, "[ERROR] config parsing failed.\n");
	return 1;
}