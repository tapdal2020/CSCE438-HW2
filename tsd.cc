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

// Struct to represent a post to a timeline, consisting of a post time and the contents
struct Post {
	time_t time;
	std::string text;
	Post(time_t _time, std::string _text) : time(_time), text(_text) {}
};

// Struct to represent the user, consisting of a username, posts, and followed users
struct User {
	std::string username;
	std::vector<Post> posts;
	std::vector<User*> followed_users;
	std::vector<time_t> last_read_post_time; // parallel to followed_users indicating the last
	User(std::string _username) : username(_username) {}
};

// Logic and data behind the server's behavior.
class TSNServiceImpl final : public TSN::Service {
    Status AddUser(ServerContext* context, const UserRequest* request,
                   UserReply* reply) override;
    Status ListUsers(ServerContext* context, const UserRequest* request,
    			     ListUsersReply* reply) override;
    Status FollowUser(ServerContext* context, const FollowUserRequest* request,
    				  UserReply* reply) override;
    Status UnfollowUser(ServerContext* context, const UnfollowUserRequest* request,
    				  UserReply* reply) override;    
    
    std::ifstream infile;
   	std::ofstream outfile;
   	std::vector<User> users;
    
    public:
    	void recoverData();
    	
};

Status TSNServiceImpl::AddUser(ServerContext* context, const UserRequest* request,
								UserReply* reply) {
    // Make sure username contains only valid characters
    std::regex pattern("[A-Za-z1-9\\_\\.\\-]+");
    if (!regex_match(request->username(), pattern))
    {
        // Return invalid username
        reply->set_status(3);
        return Status::OK;
    }
    
    // Make sure username is not taken
    else if (std::find_if(begin(users), end(users), [&](const User &u) { return u.username == request->username(); }) != end(users))
    {
        // Return username already exists
	    reply->set_status(1);
        return Status::OK;
    }
    // Username is valid and available
    else
    {
    	// Add username to list of users
        users.push_back(User(request->username()));
        
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

Status TSNServiceImpl::ListUsers(ServerContext* context, const UserRequest* request,
								 ListUsersReply* reply) {
	reply->set_followed_users("");
	reply->set_all_users("");
	
	auto pos = std::find_if(begin(users), end(users), [&](const User &u) { return u.username == request->username(); });
	if (pos == end(users))
	{
		reply->set_status(2);
		return Status::OK;
	}
	
	for (User user : users) {
		reply->set_all_users(reply->all_users() + user.username + "\n");	
	}
	
	for (User* user : pos->followed_users) {
		reply->set_followed_users(reply->followed_users() + user->username + "\n");
	}
	
	reply->set_status(0);
	return Status::OK;
}

Status TSNServiceImpl::FollowUser(ServerContext* context, const FollowUserRequest* request,
								  UserReply* reply) {
	
	if (request->username() == request->user_to_follow()) {
		reply->set_status(4);
		return Status::OK;
	}
	
	auto pos = std::find_if(begin(users), end(users), [&](const User &u) { return u.username == request->username(); });
	if (pos == end(users)) {
		reply->set_status(2);
		return Status::OK;
	}
	
	auto follow_pos = std::find_if(begin(users), end(users), [&](const User &u) { return u.username == request->user_to_follow(); });
	if (follow_pos == end(users)) {
		reply->set_status(2);
		return Status::OK;
	}
	
	for (User* user : pos->followed_users) {
		if (user->username == request->user_to_follow()) {
			reply->set_status(1);
			return Status::OK;
		}
	}
	
	pos->followed_users.push_back(&*follow_pos);
	pos->last_read_post_time.push_back(time(NULL));
	
	reply->set_status(0);
	return Status::OK;							  
}

Status TSNServiceImpl::UnfollowUser(ServerContext* context, const UnfollowUserRequest* request,
								  UserReply* reply) {
	
	if (request->username() == request->user_to_unfollow()) {
		reply->set_status(4);
		return Status::OK;
	}
								  
	auto pos = std::find_if(begin(users), end(users), [&](const User &u) { return u.username == request->username(); });
	if (pos == end(users)) {
		reply->set_status(2);
		return Status::OK;
	}
	
	bool found = false;
	for (int i = 0; i < pos->followed_users.size(); i++) {
		if (pos->followed_users[i]->username == request->user_to_unfollow()) {
			pos->followed_users.erase(begin(pos->followed_users) + i);
			pos->last_read_post_time.erase(begin(pos->last_read_post_time) + i);
			found = true;
			break;
		}
	}
	
	if (!found) {
		reply->set_status(2);
		return Status::OK;
	}

	reply->set_status(0);
	return Status::OK;						  
}

void TSNServiceImpl::recoverData() {
	infile.open("data/users.txt");
	if (infile) {
		std::cout << "Found existing users file, importing users..\n";
		std::string user;
		while(std::getline(infile, user)) {
			std::cout << "Registering new user " << user << "\n";
			users.push_back(User(user));
		}
		infile.close();
	}	
}

void RunServer() {
  	std::string server_address("0.0.0.0:3010");
  	TSNServiceImpl service;
  	service.recoverData();
		
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

