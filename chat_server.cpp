#include <cstdlib>
#include <functional>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <thread>
#include <mutex>
#include <algorithm>
#include <iomanip>
#include <array>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/thread/thread.hpp>
#include <cstdio>
// g++ chat_server.cpp -o server -std=c++11 -lboost_thread -lpthread

using boost::asio::ip::tcp;

#define MAX_BUFFER_SIZE 1024
#define MAX_NICKNAME 16
struct Message {
  std::array<char, MAX_BUFFER_SIZE> data;

  Message() {
    data.fill('\0');
  }

  void set(const std::string& str) {
    std::copy(str.begin(), str.end(), data.begin());
  }

  const char* c_str() const {
    return data.data();
  }
};
namespace
{
class workerThread
{
public:
    static void run(std::shared_ptr<boost::asio::io_context> io_context)
    {
        {
            std::lock_guard < std::mutex > lock(m);
            std::cout << "[" << std::this_thread::get_id() << "] Thread starts" << std::endl;
        }

        io_context->run();

        {
            std::lock_guard < std::mutex > lock(m);
            std::cout << "[" << std::this_thread::get_id() << "] Thread ends" << std::endl;
        }

    }
private:
    static std::mutex m;
};

std::mutex workerThread::m;
}

class participant
{
public:
    virtual ~participant() {}
    virtual void onMessage(Message & msg) = 0;
//     std::string getNickname() const{
//         return nickname_;
//     }
};

class chatRoom {
    std::unordered_set<std::shared_ptr<participant>> participants_;
    std::unordered_set<std::shared_ptr<participant>> getParticipants(){
        return participants_;
    }
    std::unordered_map<std::shared_ptr<participant>, std::string> name_table_;

public:
    void enter(std::shared_ptr<participant> participant, const std::string & nickname)
    {
        // insert participant
        participants_.insert(participant);
        name_table_[participant] = nickname;
        
        // send enter message to every participants in room
        Message formatted_msg;
        memset(formatted_msg.data.data(), '\0', MAX_BUFFER_SIZE);
        strcat(formatted_msg.data.data(), nickname.substr(0, nickname.size()-2).c_str());
        strcat(formatted_msg.data.data(), " has joined.");

        std::for_each(participants_.begin(), participants_.end(),
                      boost::bind(&participant::onMessage, _1, std::ref(formatted_msg)));
    }

    void leave(std::shared_ptr<participant> participant)
    {
        // erase participant
        std::string nickname = name_table_[participant];
        participants_.erase(participant);
        name_table_.erase(participant);

        // send leave message to every participants in room 
        Message formatted_msg;
        memset(formatted_msg.data.data(), '\0', MAX_BUFFER_SIZE);
        strcat(formatted_msg.data.data(), nickname.substr(0, nickname.size()-2).c_str());
        strcat(formatted_msg.data.data(), " has left the room.");

        std::for_each(participants_.begin(), participants_.end(),
                      boost::bind(&participant::onMessage, _1, std::ref(formatted_msg)));
    }

    void broadcast(Message& msg, std::shared_ptr<participant> participant)
    {
        // send message to every participants in room 
        std::string nickname = name_table_[participant];

        if(msg.data.data()[0]=='-' && msg.data.data()[1]=='f'){
            for(auto iter = participants_.begin(); iter!=participants_.end(); iter++)
            {
                if(nickname != name_table_[(*iter)])
                    (*iter)->onMessage(std::ref(msg));
                    // without std::ref -> image send file... why..???
            }

            // std::for_each(participants_.begin(), participants_.end(),
            //             boost::bind(&participant::onMessage, _1, std::ref(msg))); 
        }
        else{
            Message formatted_msg;
            memset(formatted_msg.data.data(), '\0', MAX_BUFFER_SIZE);
            strcat(formatted_msg.data.data(), nickname.c_str());
            strcat(formatted_msg.data.data(), msg.data.data());

            std::for_each(participants_.begin(), participants_.end(),
                        boost::bind(&participant::onMessage, _1, std::ref(formatted_msg)));
        }

    }
};

std::unordered_set<std::shared_ptr<chatRoom>> chatRooms;
std::unordered_map<std::string, std::shared_ptr<chatRoom>> room_table;


class personInRoom: public participant,
                    public std::enable_shared_from_this<personInRoom>
{
    boost::asio::ssl::stream<tcp::socket> socket_;
    boost::asio::io_context::strand& strand_;
    std::string room_name_;
    std::array<char, MAX_NICKNAME> nickname_;
    std::array<char, MAX_BUFFER_SIZE> read_msg_;

public:
    personInRoom(tcp::socket socket, 
                boost::asio::ssl::context& io_context,
                 boost::asio::io_context::strand& strand)
                 : socket_(std::move(socket), io_context), strand_(strand)
    {
        room_name_="";
    }

    void start()
    {
        do_handshake();
        // boost::asio::async_read(socket_,
        //                         boost::asio::buffer(nickname_, nickname_.size()),
        //                         strand_.wrap(boost::bind(&personInRoom::nicknameHandler, shared_from_this(), _1)));
    }

    void onMessage(Message& msg)
    {
        boost::asio::async_write(socket_,
                                     boost::asio::buffer(msg.data.data(), msg.data.size()),
                                     strand_.wrap(boost::bind(&personInRoom::writeHandler, shared_from_this(), _1)));
    }

private:
    void nicknameHandler(const boost::system::error_code& error)
    {
        strcat(nickname_.data(), ": ");

        // send chatting room list 
        Message formatted_msg;
        memset(formatted_msg.data.data(), '\0', MAX_BUFFER_SIZE);
        if(room_table.empty()){
            strcat(formatted_msg.data.data(), "No chatting rooms... try to make a chatting room [-c <chat_room_name>]");
        }
        else{
            strcat(formatted_msg.data.data(), "Chatting rooms .. try to join a chatting room [-i <chat_room_name>]\n");
            for(auto elem: room_table){
                strcat(formatted_msg.data.data(), "\t- ");
                strcat(formatted_msg.data.data(), elem.first.c_str());
                strcat(formatted_msg.data.data(), "\n");
            }
        }
        boost::asio::async_write(socket_,
                                     boost::asio::buffer(formatted_msg.data.data(), formatted_msg.data.size()),
                                     strand_.wrap(boost::bind(&personInRoom::writeHandler, shared_from_this(), _1)));

        // and read again
        boost::asio::async_read(socket_,
                                boost::asio::buffer(read_msg_.data, read_msg_.data.size()),
                                strand_.wrap(boost::bind(&personInRoom::readHandler, shared_from_this(), _1)));
    }

    

    void do_handshake()
    {
        auto self(shared_from_this());
        socket_.async_handshake(boost::asio::ssl::stream_base::server,
            [this, self](const boost::system::error_code& error)
            {
                if (!error)
                {
                    // do_read();
                    boost::asio::async_read(socket_,
                        boost::asio::buffer(nickname_, nickname_.size()),
                        strand_.wrap(boost::bind(&personInRoom::nicknameHandler, shared_from_this(), _1)));
                }
            });
    }

    void readHandler(const boost::system::error_code& error)
    {
        if (!error)
        {
            // person is participating chat room now ..
            if(room_name_.size() > 0){
                // leave chat room 
                if(read_msg_.data.data()[0]=='-' && read_msg_.data.data()[1]=='x')
                {
                    // leave chat room 
                    auto room_ = room_table[room_name_];
                    room_->leave(shared_from_this());
                    room_name_="";

                    // send chatting room list again 
                    Message formatted_msg;
                    memset(formatted_msg.data.data(), '\0', MAX_BUFFER_SIZE);
                    if(room_table.empty()){
                        strcat(formatted_msg.data.data(), "No chatting rooms... try to make a chatting room [-c <chat_room_name>]");
                    }
                    else{
                        strcat(formatted_msg.data.data(), "Chatting rooms .. try to join a chatting room [-i <chat_room_name>]\n");
                        for(auto elem: room_table){
                            strcat(formatted_msg.data.data(), "\t- ");
                            strcat(formatted_msg.data.data(), elem.first.c_str());
                            strcat(formatted_msg.data.data(), "\n");
                        }
                    }
                    boost::asio::async_write(socket_,
                                                boost::asio::buffer(formatted_msg.data.data(), formatted_msg.data.size()),
                                                strand_.wrap(boost::bind(&personInRoom::writeHandler, shared_from_this(), _1)));
                    // read again 
                    boost::asio::async_read(socket_,
                                        boost::asio::buffer(read_msg_.data, read_msg_.data.size()),
                                        strand_.wrap(boost::bind(&personInRoom::readHandler, shared_from_this(), _1)));
                }
                // else if(read_msg_.data.data()[0] == '-' && read_msg_.data.data()[1]=='w'){
                //     std::string msg_str(read_msg_.data.data());
                //     std::istringstream iss(msg_str);

                //     std::string cmd, receiver_nickname, message;
                //     iss >> cmd >> receiver_nickname;
                //     std::getline(iss, message);

                //     whisper(receiver_nickname, message);
                // }
                // send message 
                else{
                    auto room_ = room_table[room_name_];
                    room_->broadcast(read_msg_, shared_from_this());

                    boost::asio::async_read(socket_,
                                        boost::asio::buffer(read_msg_.data, read_msg_.data.size()),
                                        strand_.wrap(boost::bind(&personInRoom::readHandler, shared_from_this(), _1)));
                }
            }
            // person does not enter any room yet...
            else{
                // create a room 
                if(read_msg_.data.data()[0]=='-' && read_msg_.data.data()[1]=='c')
                {   
                    char get_name[40];
                    strcpy(get_name, read_msg_.data.data()+3);
                    room_name_ = get_name;
                    std::shared_ptr<chatRoom> new_room(new chatRoom());
                    room_table[room_name_] = new_room;
                    new_room->enter(shared_from_this(), std::string(nickname_.data()));

                    boost::asio::async_read(socket_,
                                        boost::asio::buffer(read_msg_.data, read_msg_.data.size()),
                                        strand_.wrap(boost::bind(&personInRoom::readHandler, shared_from_this(), _1)));
                }
                // enter a room 
                else if(read_msg_.data.data()[0]=='-' && read_msg_.data.data()[1]=='i')
                {   
                    char get_name[40];
                    strcpy(get_name, read_msg_.data.data()+3);
                    room_name_ = get_name;
                    // if corresponding room name is not exist 
                    if(room_table.find(room_name_)==room_table.end()){
                        Message formatted_msg;
                        memset(formatted_msg.data.data(), '\0', MAX_BUFFER_SIZE);
                        strcat(formatted_msg.data.data(), "Please enter correct chatting room name ..");
                        room_name_="";
                        boost::asio::async_write(socket_,
                                            boost::asio::buffer(formatted_msg.data.data(), formatted_msg.data.size()),
                                            strand_.wrap(boost::bind(&personInRoom::writeHandler, shared_from_this(), _1)));
                    }
                    // there is a room corresponding to room name
                    else{
                        auto room_ = room_table[room_name_];
                        room_->enter(shared_from_this(), std::string(nickname_.data()));
                    }

                    boost::asio::async_read(socket_,
                                        boost::asio::buffer(read_msg_.data, read_msg_.data.size()),
                                        strand_.wrap(boost::bind(&personInRoom::readHandler, shared_from_this(), _1)));
                }
            }
            
        }
        else
        {
            if(room_name_.size()>0){
                auto room_ = room_table[room_name_];
                room_->leave(shared_from_this());
            }
        }
    }


    void writeHandler(const boost::system::error_code& error)
    {
        if (!error)
        {
        }
        else
        {
            if(room_name_.size()>0){
                auto room_ = room_table[room_name_];
                room_->leave(shared_from_this());
            }
        }
    }
};

class server
{
    boost::asio::ssl::context io_context_;
    boost::asio::io_service::strand& strand_;
    tcp::acceptor acceptor_;

    void do_accept()
    {
        acceptor_.async_accept(
            [this](const boost::system::error_code& error, tcp::socket socket)
            {
                if (!error)
                {
                    std::shared_ptr<personInRoom> new_participant(new personInRoom(std::move(socket), io_context_, strand_));
                    new_participant->start();
                }

                do_accept();
            });
    }

    std::string get_password() const
    {
        return "test";
    }


public:
    server(boost::asio::io_context& io_context,
            boost::asio::io_context::strand& strand, const tcp::endpoint& endpoint)
        ://io_context_(boost::asio::ssl::context::sslv23),
        strand_(strand), acceptor_(io_context, endpoint),
        io_context_(boost::asio::ssl::context::sslv23)
    {
        io_context_.set_options(
            boost::asio::ssl::context::default_workarounds
            | boost::asio::ssl::context::no_sslv2
            | boost::asio::ssl::context::single_dh_use);
        io_context_.set_password_callback(std::bind(&server::get_password, this));
        io_context_.use_certificate_chain_file("server.pem");
        io_context_.use_private_key_file("server.pem", boost::asio::ssl::context::pem);
        io_context_.use_tmp_dh_file("dh4096.pem");

        do_accept();
    }
};

//----------------------------------------------------------------------

int main(int argc, char* argv[])
{
    try
    {
        if (argc < 3)
        {
            std::cerr << "Usage: chat_server <ip> <port>\n";
            return 1;
        }

        std::shared_ptr<boost::asio::io_context> io_context(new boost::asio::io_context);
        boost::shared_ptr<boost::asio::io_context::work> work(new boost::asio::io_context::work(*io_context));
        boost::shared_ptr<boost::asio::io_context::strand> strand(new boost::asio::io_context::strand(*io_context));

        std::cout << "[" << std::this_thread::get_id() << "]" << "server starts" << std::endl;

        tcp::endpoint endpoint(boost::asio::ip::make_address(argv[1]), std::atoi(argv[2]));
        server chat_server(*io_context, *strand, endpoint);

        io_context->run();
    } catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}