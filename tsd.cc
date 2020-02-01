/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <regex>
#include <fstream>

#include <grpc++/grpc++.h>

#include "ts.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

// Logic and data behind the server's behavior.
class TSNServiceImpl final : public TSN::Service {
    Status AddUser(ServerContext* context, const AddUserRequest* request,
                   AddUserReply* reply) override;
    public:
    	std::ifstream infile;
    	std::ofstream outfile;
      	std::vector<std::string> users;
};

Status TSNServiceImpl::AddUser(ServerContext* context, const AddUserRequest* request,
								AddUserReply* reply) {
    // Make sure username contains only valid characters
    std::regex pattern("[A-Za-z1-9\\_\\.\\-]+");
    if (!regex_match(request->username(), pattern))
    {
        // Return invalid username
        reply->set_status(3);
        return Status::OK;
    }
    // Make sure username is not taken
    else if (std::find(users.begin(), users.end(), request->username()) != users.end())
    {
        // Return username already exists
	    reply->set_status(1);
        return Status::OK;
    }
    // Username is valid and available
    else
    {
    	// Add username to list of users
        users.push_back(request->username());
        
        // Append username to users file
        outfile.open("data/users.txt", std::ios_base::app);
        if (!outfile) {
        	std::cout << "ERROR: Could not write to users file!\n";
        	return Status::CANCELLED;
       	}
        outfile << request->username() << "\n";
        outfile.close();
        std::cout << "Registered new user " << request->username() << "\n";

        reply->set_status(0);
	    return Status::OK;
    }
}

void RunServer() {
  	std::string server_address("0.0.0.0:3010");
  	TSNServiceImpl service;
	service.infile.open("data/users.txt");
	if (service.infile) {
		std::cout << "Found existing users file, importing users..\n";
		std::string user;
		while(std::getline(service.infile, user)) {
			std::cout << "Registering new user " << user << "\n";
			service.users.push_back(user);
		}
		service.infile.close();
	}	
		
  	ServerBuilder builder;
  	// Listen on the given address without any authentication mechanism.
  	builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  	// Register "service" as the instance through which we'll communicate with
  	// clients. In this case it corresponds to an *synchronous* service.
  	builder.RegisterService(&service);
  	// Finally assemble the server.
  	std::unique_ptr<Server> server(builder.BuildAndStart());
  	std::cout << "Server listening on " << server_address << std::endl;

  	// Wait for the server to shutdown. Note that some other thread must be
  	// responsible for shutting down the server for this call to ever return.
  	server->Wait();
}

int main(int argc, char** argv) {
  	RunServer();

  	return 0;
}

