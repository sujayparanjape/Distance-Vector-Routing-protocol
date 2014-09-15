/*
 * sujaypar_server.cpp
 *
 *  Created on : Apr 20, 2014
 *      Author : sujay
 *  Description: This is a main program that accepts the commands
 *  			 and parametes to start the routing node. This class initialzes the
 *  			 object of RoutingServer and calls the init method.
 */

#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <ctype.h>
#include "RoutingServer.h"
#include <sstream>

void PrintUsage();

using namespace std;
/**
 * Execution start point
 * @param argc : command line arguments array
 * @param argv : no of arguments on commandline
 * @return
 */
int main(int argc, char* argv[])
{
	stringstream ss;

	if(argc != 5)  // invalid no of parameters
	{
		PrintUsage();
		exit(0);
	}

	if( strcmp(argv[1],"-t") || strcmp(argv[3],"-i") ) //incorrect parameters
	{
		PrintUsage();
		exit(0);
	}


	int updateInterval = -1;
	updateInterval =(int)strtol(argv[4],NULL,10);

	if(updateInterval <= 0)
		{
			cout<<"Invalid value for routing-update-interval parameter.";
			exit(0);
		}

	FILE * fptr;

	fptr = fopen(argv[2],"r");

	if(fptr == NULL)
	{
		cout<<"Unable to read the specified file. Please check if file exists.";
		exit(0);
	}

	fclose(fptr);

	ifstream topologyFile(argv[2]);

	string line;
	int noOfServers= 0;
	int noOfNeighbours= 0;
	vector<ServerInfo> serverList;
	vector<LinkInfo> linkList;

	if(topologyFile.is_open())
	{
		if(getline(topologyFile,line))
		{
			noOfServers = (int)strtol(line.c_str(),NULL,10);
		}
		else
		{
			cout<<"Failed to read no of servers entry.";
			if(topologyFile.is_open())
				topologyFile.close();
			exit(0);
		}

		line = "";

		if(getline(topologyFile,line))
		{
			noOfNeighbours = (int)strtol(line.c_str(),NULL,10);
		}
		else
		{
			cout<<"Failed to read no of neighbour entry.";
			if(topologyFile.is_open())
				topologyFile.close();
			exit(0);
		}

		line = "";

		//parse server list

		for (int i = 0 ; i< noOfServers;i++)
		{
			if(getline(topologyFile,line))
			{
				// read input line into the char array
				char * inputLine_char = new char[line.length()*sizeof(char) + 1];
				strncpy(inputLine_char,line.c_str(),line.length());
				inputLine_char[line.length()] = '\0';

				struct ServerInfo e_serverinfo;
				char * tokenPtr = strtok(inputLine_char," ");
				int e_serverId = -1;

				e_serverId = strtol(tokenPtr,NULL,10);

				if(e_serverId == 0)
				{
					cout<<"Unable to read server id";
					if(topologyFile.is_open())
					topologyFile.close();
					delete[] inputLine_char;
					exit(0);
				}
				else
				{
					e_serverinfo.serverId = e_serverId;
				}

				// no ip validation
				tokenPtr = strtok(NULL," ");
				strcpy(e_serverinfo.serverIp,tokenPtr);
				e_serverinfo.serverIp[strlen(tokenPtr)] ='\0';

				int e_port = -1;
				tokenPtr = strtok(NULL," ");
				e_port = (int)strtol(tokenPtr,NULL,10);

				if(e_port < 1 || e_port > 65536)
				{
					cout<<"Invalid port entry";
					if(topologyFile.is_open())
					topologyFile.close();
					delete[] inputLine_char;
					exit(0);
				}
				else
				{
					ss.str("");
					ss<<e_port;
					strncpy(e_serverinfo.serverPort,ss.str().c_str(),ss.str().size());
					e_serverinfo.serverPort[ss.str().size()] = '\0';
					//e_serverinfo.serverPort = e_port;
				}

				serverList.push_back(e_serverinfo);

				//free the allocated memory
				delete[] inputLine_char;
			}
			else
			{
				cout<<"Unable to read server info entry";
				if(topologyFile.is_open())
					topologyFile.close();
				exit(0);
			}

		}

		// parse neighbor entries.

		for (int i = 0;i< noOfNeighbours ; i ++)
		{
			if(getline(topologyFile,line))
			{
				char * inputLine_char = new char[line.length()*sizeof(char) + 1];
				strncpy(inputLine_char,line.c_str(),line.length());
				inputLine_char[line.length()] = '\0';

				char * tokenPtr = strtok(inputLine_char," ");

				int sourceServerId = -1, destServerId = -1, cost = -1;

				sourceServerId  =(int)strtol(tokenPtr,NULL,10);
				tokenPtr = strtok(NULL," ");
				destServerId = (int)strtol(tokenPtr,NULL,10);
				tokenPtr = strtok(NULL," ");
				cost = (int)strtol(tokenPtr,NULL,10);

				if(sourceServerId < 1 || destServerId < 1 || cost<1)
				{
					cout<<"Invalid entries in neighbor link and costs";
					delete[] inputLine_char;
					if(topologyFile.is_open())
						topologyFile.close();
					exit(0);
				}
				else
				{
					struct LinkInfo e_linkInfo;
					e_linkInfo.sourceServerId = sourceServerId;
					e_linkInfo.destinationServerId = destServerId;
					e_linkInfo.cost = cost;
					e_linkInfo.isActiveLink = true;
					e_linkInfo.isDisabled = false;

					linkList.push_back(e_linkInfo);
				}

				delete[] inputLine_char;
			}
			else
			{
				cout<<"Invalid no of link entries."<<endl;
					if(topologyFile.is_open())
					topologyFile.close();
					exit(0);
			}

		}

	}
	else
	{
		cout<<"Unable to open the file.";
		exit(0);
	}

	if(getline(topologyFile,line))
	{
		cout<<"Some input was not parsed."<<endl;
		cout<<"Last unparsed line: "<<line<<endl;
		if(topologyFile.is_open())
		topologyFile.close();
		exit(0);
	}


	if(topologyFile.is_open())
	topologyFile.close();

	RoutingServer node(serverList,linkList,updateInterval);

	node.Init();

	return 0;
}

/**
 * Print the usage format.
 */
void PrintUsage()
{
	cout<<"Invalid input. Please type the command in following format."<<endl;
	cout<<"./server -t <topology-file-name> -i <routing-update-interval>"<<endl;
}
