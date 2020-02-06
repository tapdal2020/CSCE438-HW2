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
#include <thread>
#include <string>
#include <vector>
#include <algorithm>
#include <regex>
#include <fstream>
#include <queue>
#include <semaphore.h>

#include <grpc++/grpc++.h>

#include "ts.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;

// Struct to represent a post to a timeline, consisting of a post time, the username of the poster, and the contents
struct Post {
	time_t time;
	std::string poster;
	std::string text;
	Post(time_t _time, std::string _poster, std::string _text) : time(_time), poster(_poster), text(_text) {}
};

// Struct to represent the user, consisting of a username, timeline, and followed users
struct User {
	bool active;
	std::string username;
	sem_t mtx;
	std::vector<Post> timeline;
	std::vector<std::string> followed_users;
	User(std::string _username) : username(_username), active(false) { }
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
    Status ProcessTimeline(ServerContext* context, 
            ServerReaderWriter<PostMessage, PostMessage>* stream) override;
    
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
    
    // Make sure username is not taken by an active user
    auto user_pos = std::find_if(begin(users), end(users), [&](const User &u) { return u.username == request->username(); });
    if (user_pos != end(users) && user_pos->active)
    {
        // Return username already taken by active user
	    reply->set_status(1);
        return Status::OK;
    }
    // Username already exists but is inactive 
    else if (user_pos != end(users))
    {     
    	user_pos->active = true;
    	
        // Read existing data from file
        std::ifstream infile{"data/users/" + request->username() + ".txt"};
  		if (infile) {
  			std::cout << "Found existing user file for " << request->username() << "\n"; 
			std::cout << "Followed users:" << "\n";
  			std::string user_to_follow;
  			while(infile >> user_to_follow) {
  				std::cout << user_to_follow << "\n";
  				user_pos->followed_users.push_back(user_to_follow);
  			}
  			
  			infile.close();	
  		}
  		
  		time_t time;
  		std::string poster;
  		std::string text;
  		std::vector<Post> posts;
  		infile.open("data/timelines/" + request->username() + ".txt");
  		if (infile) {
  			std::cout << "Found timeline file\n";
  			while (infile >> time >> poster >> text) 
  				posts.insert(begin(posts), Post(time, poster, text));
  			
  			for (int i = 0; i < std::min(20, (int) posts.size()); i++) {
  				user_pos->timeline.insert(begin(user_pos->timeline), posts[i]);
  				std::cout << "Added post\n";
  			}
  		}
  		
  		sem_init(&user_pos->mtx, 0, std::min(20, (int) posts.size()));
        
        std::cout << "Registered existing user " << request->username() << "\n";
        std::cout << "initial sem value = " + (int) (std::min(20, (int) posts.size())) << std::endl;

    }
    // Username is not registered
    else if (user_pos == end(users))
    {
        User new_user(request->username());
        new_user.active = true;
        
        // Append username to users file
        std::ofstream outfile;
        outfile.open("data/users.txt", std::ios_base::app);
        if (!outfile) {
        	std::cout << "ERROR: Could not write to data/users.txt" << std::endl;
        	return Status::CANCELLED;
       	}
        outfile << request->username() << "\n";
        outfile.close();
        
        outfile.open("data/users/" + request->username() + ".txt");
        if (!outfile) {
        	std::cout << "ERROR: Could not write to data/user/" << request->username() << std::endl;
        	return Status::CANCELLED;
        }
        outfile << request->username() << "\n";
        outfile.close();
        
  		// New user, add them to list of users
  		new_user.followed_users.push_back(request->username());
  		sem_init(&new_user.mtx, 0, 0);
  		
  		users.push_back(new_user);	
  		
        std::cout << "Registered new user " << request->username() << "\n";
    }
 
    reply->set_status(0);
    return Status::OK;
}

Status TSNServiceImpl::ListUsers(ServerContext* context, const UserRequest* request,
								 ListUsersReply* reply) {
	reply->set_followed_users("");
	reply->set_all_users("");
	
	// Make sure user making request is registered
	auto pos = std::find_if(begin(users), end(users), [&](const User &u) { return u.username == request->username(); });
	if (pos == end(users)) {
		reply->set_status(2);
		return Status::OK;
	}
	
	// Go through all users
	for (User& user : users) {
		reply->set_all_users(reply->all_users() + user.username + "\n");	
	}
	
	// Go through all followed users
	for (std::string user : pos->followed_users) {
		reply->set_followed_users(reply->followed_users() + user + "\n");
	}
	
	reply->set_status(0);
	return Status::OK;
}

Status TSNServiceImpl::FollowUser(ServerContext* context, const FollowUserRequest* request,
								  UserReply* reply) {
	
	if (request->username() == request->user_to_follow()) {
		reply->set_status(1);
		return Status::OK;
	}
	
	// Make sure the username making the request is registered
	auto pos = std::find_if(begin(users), end(users), [&](const User &u) { return u.username == request->username(); });
	if (pos == end(users)) {
		reply->set_status(2);
		return Status::OK;
	}
	
	// Make sure the user to follow is also registered
	auto follow_pos = std::find_if(begin(users), end(users), [&](const User &u) { return u.username == request->user_to_follow(); });
	if (follow_pos == end(users)) {
		reply->set_status(3);
		return Status::OK;
	}
	
	// Make sure the user to follow is not already followed by the user making the request
	for (std::string user : pos->followed_users) {
		if (user == request->user_to_follow()) {
			reply->set_status(1);
			return Status::OK;
		}
	}
	
	std::ofstream outfile;
	outfile.open("data/users/" + request->username() + ".txt", std::ios_base::app);
	if (outfile) {
		pos->followed_users.push_back(request->user_to_follow());
		outfile << request->user_to_follow() << "\n";
		outfile.close();
	}
	else {
		std::cout << "Error: data/users/" + request->username() + " could not be opened!\n";
		reply->set_status(5);
		return Status::OK;
	}
	
	reply->set_status(0);
	return Status::OK;							  
}

Status TSNServiceImpl::UnfollowUser(ServerContext* context, const UnfollowUserRequest* request,
								  UserReply* reply) {
	
	// Check if user is attempting to unregister themselves		  
	if (request->username() == request->user_to_unfollow()) {
		reply->set_status(3);
		return Status::OK;
	}
	
	// Check if user making request is registered
	auto pos = std::find_if(begin(users), end(users), [&](const User &u) { return u.username == request->username(); });
	if (pos == end(users)) {
		reply->set_status(3);
		return Status::OK;
	}
	
	// Attempt to unfollow the user
	bool found = false;
	for (int i = 0; i < pos->followed_users.size(); i++) {
		if (pos->followed_users[i] == request->user_to_unfollow()) {
			pos->followed_users.erase(begin(pos->followed_users) + i);
			found = true;
			
			// Record updates on disk
			std::ofstream outfile;
			outfile.open("data/users/" + request->username() + ".txt");
			if (outfile) {
				for (std::string user : pos->followed_users)
					outfile << user << "\n";
				outfile.close();
			}
			else {
				std::cout << "Error: data/users/" + request->username() + " could not be opened!\n";
				reply->set_status(5);
				return Status::OK;			
			}
			break;
		}
	}
	
	// If the user was not found in the list of followed users
	if (!found) {
		reply->set_status(3);
		return Status::OK;
	}

	reply->set_status(0);
	return Status::OK;						  
}


Status TSNServiceImpl::ProcessTimeline(ServerContext* context, 
            ServerReaderWriter<PostMessage, PostMessage>* stream) {
    
    PostMessage userinfo;
    if (!stream->Read(&userinfo)) {
		std::cout << "ERROR: Client unexpectedly closed connection\n";
		return Status::OK;
	}
    
   	std::thread reader{[stream](TSNServiceImpl* service, std::string username) {
		// Read messages from the client and write them to following users timelines (and to files in ../data/timelines for persistence)
	
		PostMessage p;
    	while(stream->Read(&p)) {      		
            for (User& user : service->users) {
				auto pos = std::find_if(begin(user.followed_users), end(user.followed_users), [&](const User &u) { return u.username == username; });
				if (pos != end(user.followed_users)) {
				
					Post new_post(p.time(), p.sender(), p.content());
					while(user.timeline.size() >= 20) {
						sem_wait(&user.mtx);
						user.timeline.pop_back();
					}
					user.timeline.insert(begin(user.timeline), new_post);
					
					std::ofstream outfile;
					outfile.open("data/timelines/" + user.username + ".txt", std::ios_base::app);
					if (outfile) {
						outfile << new_post.time << " " << new_post.poster << " " << new_post.text << "\n";
						outfile.close();
					}
					else {
						std::cout << "ERROR: Could not open data/timelines/" + user.username + ".txt for writing\n";
					}
					sem_post(&user.mtx);
				}      	
            }
    	}	
    
    }, this, userinfo.sender()};

    std::thread writer{[stream](TSNServiceImpl* service, std::string username) {
		// Constantly look for content added to the users' own timeline and write it to the client
		auto pos = std::find_if(begin(service->users), end(service->users), [&](const User &u) { return u.username == username; });
		if (pos == end(service->users)) {
			return Status::OK;
		}
		
        PostMessage new_post;
        
        while(true){
        	std::cout << "Waiting on mutex for " << username << "\n";
			sem_wait(&pos->mtx);
        	new_post.set_time(pos->timeline.front().time);
        	new_post.set_content(pos->timeline.front().text);
        	new_post.set_sender(pos->timeline.front().poster);
        	pos->timeline.erase(begin(pos->timeline));
            
            if (new_post.sender() != username)
        		stream->Write(new_post);				
		}
   	}, this, userinfo.sender()};

   	//Wait for the threads to finish
   	writer.join();
    reader.join();

    return Status::OK;
}

// Read all users from the users file, marking each as inactive until they re-register
void TSNServiceImpl::recoverData() {
	std::ifstream infile{"data/users.txt"};
	if (infile) {
		std::cout << "Found existing users file, importing users..\n";
		std::string username;
		while(std::getline(infile, username)) {
			std::cout << "Found existing user " << username << "\n";
			User user(username);
			user.active = false;
			users.push_back(user);
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

