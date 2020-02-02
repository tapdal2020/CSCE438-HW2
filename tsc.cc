//#include <iostream>
//#include <memory>
//#include <thread>
//#include <vector>
//#include <string>
#include <sstream>
#include <unistd.h>
#include <grpc++/grpc++.h>
#include "client.h"

#include "ts.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

class Client : public IClient
{
    public:
	    Client(const std::string& hname,
               const std::string& uname,
               const std::string& p)
	    :hostname(hname), username(uname), port(p) {}
	
    protected:
        virtual int connectTo();
        virtual IReply processCommand(std::string& input);
        virtual void processTimeline();

    private:
        std::string hostname;
        std::string username;
        std::string port;
        
        // You can have an instance of the client stub
        // as a member variable.
    	std::unique_ptr<TSN::Stub> stub_;
};

int main(int argc, char** argv) {

    std::string hostname = "localhost";
    std::string username = "default";
    std::string port = "3010";
    int opt = 0;
    while ((opt = getopt(argc, argv, "h:u:p:")) != -1){
        switch(opt) {
        case 'h':
        	hostname = optarg;
		break;
        case 'u':
            username = optarg;
		break;
        case 'p':
            port = optarg;
		break;
        default:
            std::cerr << "Invalid Command Line Argument\n";
        }
    }

    Client myc(hostname, username, port);

    // You MUST invoke "run_client" function to start business logic
    myc.run_client();

    return 0;
}

int Client::connectTo()
{
	// ------------------------------------------------------------
    // In this function, you are supposed to create a stub so that
    // you can call service methods in the processCommand/processTimeline
    // functions. That is, the stub should be accessible when you want
    // to call any service methods in those functions.
	// ------------------------------------------------------------

    // Connect to the server
    stub_ = TSN::NewStub(std::shared_ptr<Channel>(
    					 grpc::CreateChannel(hostname + ":" + port, 
    					 grpc::InsecureChannelCredentials())));
    
    // Data we are sending to the server
    UserRequest request;
    request.set_username(username);
    
    // Structure for the data we expect to get back from the server
    UserReply reply;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = stub_->AddUser(&context, request, &reply);

    // Act upon its status.
    if (status.ok() && (IStatus) reply.status() == IStatus::SUCCESS) {
      	return 1;
    } 
	else {
      	return -1;
    }
}

IReply Client::processCommand(std::string& input)
{
	// ------------------------------------------------------------
	// GUIDE 1:
	// In this function, you are supposed to parse the given input
    // command and create your own message so that you call an 
    // appropriate service method. The input command will be one
    // of the followings:
	//
	// FOLLOW <username>
	// UNFOLLOW <username>
	// LIST
    // TIMELINE
	//
	// - JOIN/LEAVE and "<username>" are separated by one space.
	// ------------------------------------------------------------
	
	
	IReply ire;
	
	ClientContext context;
	Status status;
	
	std::stringstream ss(input);
    std::string command;
    ss >> command;
    
    if (command == "FOLLOW") {
    	FollowUserRequest request;
    	UserReply reply;		
		request.set_username(username);
		
    	std::string userToFollow;
    	ss >> userToFollow;
    	request.set_user_to_follow(userToFollow);
    		
    	status = stub_->FollowUser(&context, request, &reply);
    	if (status.ok()) {
    		ire.comm_status = (IStatus) reply.status();
    	}
    	else {
    		ire.comm_status = IStatus::FAILURE_UNKNOWN;
    	}
    }
    else if (command == "UNFOLLOW") {
    	UnfollowUserRequest request;
    	UserReply reply;
    	request.set_username(username);	
    	
    	std::string userToUnfollow;
    	ss >> userToUnfollow;
    	request.set_user_to_unfollow(userToUnfollow);
    		
    	status = stub_->UnfollowUser(&context, request, &reply);
    	if (status.ok()) {
    		ire.comm_status = (IStatus) reply.status();
    	}
    	else {
    		ire.comm_status = IStatus::FAILURE_UNKNOWN;
    	}    	    
    }
    else if (command == "LIST") {
		UserRequest request;
		ListUsersReply reply;
		request.set_username(username);
		
    	status = stub_->ListUsers(&context, request, &reply);
    	if (status.ok()) {
    		ire.comm_status = (IStatus) reply.status();
    	}
    	else {
    		ire.comm_status = IStatus::FAILURE_UNKNOWN;
    	}
    	
    	std::stringstream all_users(reply.all_users());
    	std::string user;
    	
    	if (reply.all_users() != "") {
    		while (std::getline(all_users, user, '\n')) {
    			ire.all_users.push_back(user);
    		}
    	}
    	
    	std::stringstream following_users(reply.followed_users());
    	
    	if (reply.followed_users() != "") {
    		while (std::getline(following_users, user, '\n')) {
    			ire.following_users.push_back(user);
    		}
    	}
    }
    else {
    	ire.comm_status = FAILURE_UNKNOWN;
    }
		
    // ------------------------------------------------------------
	// GUIDE 2:
	// Then, you should create a variable of IReply structure
	// provided by the client.h and initialize it according to
	// the result. Finally you can finish this function by returning
    // the IReply.
	// ------------------------------------------------------------
    
	// ------------------------------------------------------------
    // HINT: How to set the IReply?
    // Suppose you have "Join" service method for JOIN command,
    // IReply can be set as follow:
    // 
    //     // some codes for creating/initializing parameters for
    //     // service method
    //     IReply ire;
    //     grpc::Status status = stub_->Join(&context, /* some parameters */);
    //     ire.grpc_status = status;
    //     if (status.ok()) {
    //         ire.comm_status = SUCCESS;
    //     } else {
    //         ire.comm_status = FAILURE_NOT_EXISTS;
    //     }
    //      
    //      return ire;
    // 
    // IMPORTANT: 
    // For the command "LIST", you should set both "all_users" and 
    // "following_users" member variable of IReply.
	// ------------------------------------------------------------
    
    return ire;
}

void Client::processTimeline()
{
	// ------------------------------------------------------------
    // In this function, you are supposed to get into timeline mode.
    // You may need to call a service method to communicate with
    // the server. Use getPostMessage/displayPostMessage functions
    // for both getting and displaying messages in timeline mode.
    // You should use them as you did in hw1.
	// ------------------------------------------------------------

    // ------------------------------------------------------------
    // IMPORTANT NOTICE:
    //
    // Once a user enter to timeline mode , there is no way
    // to command mode. You don't have to worry about this situation,
    // and you can terminate the client program by pressing
    // CTRL-C (SIGINT)
	// ------------------------------------------------------------
}
