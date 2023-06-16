#include <array>
#include <thread>
#include <functional>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <chrono>
#include <cstdio>
using boost::asio::ip::tcp;

#define MAX_BUFFER_SIZE 1024
#define MAX_NICKNAME 16
#define MAX_FILENAME 100
#define PADDING 24
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

class client
{
    boost::asio::io_context& io_context_;
    boost::asio::ssl::stream<tcp::socket> socket_;
    Message read_msg_;
    std::array<char, MAX_NICKNAME> nickname_;

public:
    client(const std::array<char, MAX_NICKNAME>& nickname,
            boost::asio::io_context& io_context,
            boost::asio::ssl::context& context,
            const tcp::resolver::results_type& endpoints) :
            io_context_(io_context), socket_(io_context, context)
    {
        strcpy(nickname_.data(), nickname.data());
        memset(read_msg_.data.data(), '\0', MAX_BUFFER_SIZE);

        socket_.set_verify_mode(boost::asio::ssl::verify_peer);
        socket_.set_verify_callback(
            std::bind(&client::verify_certificate, this, std::placeholders::_1, std::placeholders::_2));
        
        ssl_connect(endpoints);
    }

    void write(Message& msg)
    {
        io_context_.post(boost::bind(&client::writeImpl, this, msg));
    }

    void close()
    {
        io_context_.post(boost::bind(&client::closeImpl, this));
    }

private:
    bool verify_certificate(bool preverified,
      boost::asio::ssl::verify_context& ctx)
    {
        char subject_name[256];
        X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
        X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);
        std::cout << "Verifying " << subject_name << "\n";

        return preverified;
    }
    // tcp connection
    void ssl_connect(const tcp::resolver::results_type& endpoints)
    {
        boost::asio::async_connect(socket_.lowest_layer(), endpoints,
            [this](const boost::system::error_code& error,
            const tcp::endpoint& /*endpoint*/)
            {
            if (!error)
            {
                handshake();
            }
            else
            {
                std::cout << "Connect failed: " << error.message() << "\n";
            }
            });
    }
    // tls handshake
    void handshake()
    {
        socket_.async_handshake(boost::asio::ssl::stream_base::client,
            [this](const boost::system::error_code& error)
            {
            if (!error)
            {
                // send nickname to server
                boost::asio::async_write(socket_,
                                     boost::asio::buffer(nickname_, nickname_.size()),
                                     boost::bind(&client::readHandler, this, _1));
            }
            else
            {
                std::cout << "Handshake failed: " << error.message() << "\n";
            }
            });
    }
    int find_first_space(Message& arr, int end_name, int length) {
        for (int i = end_name+1; i < length; ++i) {
            if (arr.data.data()[i] == ' ') {
                return i;
            }
        }
        return 0;
    }
    int find_first_space2(const std::array<char, MAX_NICKNAME>& arr, int length) {
        for (int i = 0; i < length; ++i) {
            if (arr.data()[i] == '\0') {
                // std::cout << i << std::endl;
                return i;
            }
        }
        return 0;
    }
    void readHandler(const boost::system::error_code& error)
    {
        if (!error)
        {
            // int pos_last_name = 0; 
            int nick_name_last = 0;
            // int start_msg = 0;
            // int msg_last = 0;
            int first_space = 0;
            int second_space = 0;
            int third_space = 0;
            nick_name_last = find_first_space2(nickname_, 16); // current client nickname length
            // start_msg = nick_name_last + 2;
            // msg_last = start_msg + 2 + nick_name_last;
            first_space = find_first_space(read_msg_, 0, 100);
            second_space = find_first_space(read_msg_, first_space+1, 100);
            third_space = find_first_space(read_msg_, second_space+1, 100);
            // std::cout << "space" << first_space << " "<< second_space << " " << third_space << std::endl;

            // receive file data
            if(read_msg_.data.data()[0]=='-' && read_msg_.data.data()[1]=='f'){
                // -f<file_name> <content_size><content>
                char * fname = strtok(read_msg_.data.data()+2, " ");
                FILE * fp = fopen(fname, "ab");
                char size[4];
                strncpy(size, read_msg_.data.data()+2+strlen(fname)+1, 4);
                int pkt_size = atoi(size);

                fwrite(read_msg_.data.data()+2+strlen(fname)+1+4, sizeof(char), pkt_size, fp);
                fclose(fp);
            }
            // receive secret message
            else if(read_msg_.data.data()[first_space+1] == '-' && read_msg_.data.data()[first_space+2] == 's'){
                std::array<char, MAX_NICKNAME> data_substring;
                std::memcpy(data_substring.data(), read_msg_.data.data() + second_space+1, third_space-second_space-1);
                data_substring[third_space-second_space-1] = '\0';
                const auto result = std::strcmp(data_substring.data(), nickname_.data()) == 0;
                Message specific_msg;
                if(result){
                    std::memcpy(specific_msg.data.data(), read_msg_.data.data() +0, first_space);

                    specific_msg.data[first_space] = ' ';

                    std::memcpy(specific_msg.data.data() + first_space + 1, read_msg_.data.data() + third_space + 1, read_msg_.data.size());
                    // std::cout << read_msg_.data.data() << std::endl;
                    std::cout << "[secret] " << specific_msg.data.data() << std::endl;
                    // std::cout << std::string(read_msg_.data.data() + pos_last_name + 2, read_msg_.data.data() +read_msg_.data.size()) << std::endl;
                }
                // else{
                //     std::cout << "Nickname not exist\n" << std::endl;
                //     std::cout << read_msg_.data.data() << std::endl;   
                // }
                
            }
            // receive message
            else{
                std::cout << read_msg_.data.data() << std::endl;
            }
            
            boost::asio::async_read(socket_,
                                    boost::asio::buffer(read_msg_.data, read_msg_.data.size()),
                                    boost::bind(&client::readHandler, this, _1));
        } else
        {
            closeImpl();
        }
    }

    void writeImpl(Message msg)
    {
        // send file 
        if(msg.data[0]=='-' && msg.data[1]=='f'){
            FILE *fp;
            size_t fsize; /* size of file */
            char fname[MAX_FILENAME];
            int num_pkt; /* total number of packet to send */
            int pkt_size;
            strcpy(fname, msg.data.data()+3);
            
            Message formatted_msg;

            if((fp = fopen(fname, "rb")) == NULL){
                std::cout<< "File not exists ..\n";
            }
            else{
                fseek(fp, 0, SEEK_END);
                fsize = ftell(fp);
                fseek(fp, 0, SEEK_SET);
                pkt_size = MAX_BUFFER_SIZE-2-strlen(fname)-1-4; 
                num_pkt = fsize/(pkt_size) + 1;
                // -f<file_name> <size><content>
                // <size> : 4 character 

                for(int i=0; i<num_pkt; i++)
                {
                    // memset(formatted_msg.data, 0x00, MAX_BUFFER_SIZE);
                    formatted_msg.data.fill('\0');
                    strcat(formatted_msg.data.data(), "-f");
                    strcat(formatted_msg.data.data(), fname);
                    strcat(formatted_msg.data.data(), " ");

                    int tmp_size = fread(formatted_msg.data.data()+2+strlen(fname)+1+4, sizeof(char), pkt_size, fp);
                    // sprintf(formatted_msg.data()+2+strlen(fname)+1, "%4d", tmp_size);
                    char s[5];
                    sprintf(s, "%4d", tmp_size);
                    formatted_msg.data[2+strlen(fname)+1] = s[0];
                    formatted_msg.data[2+strlen(fname)+2] = s[1];
                    formatted_msg.data[2+strlen(fname)+3] = s[2];
                    formatted_msg.data[2+strlen(fname)+4] = s[3];

                    boost::asio::async_write(socket_,
                                     boost::asio::buffer(formatted_msg.data, formatted_msg.data.size()),
                                     boost::bind(&client::writeHandler, this, _1)); 
                }
                fclose(fp);
            }
        }
        // send message 
        else {
            boost::asio::async_write(socket_,
                                     boost::asio::buffer(msg.data, msg.data.size()),
                                     boost::bind(&client::writeHandler, this, _1));
        }
    }

    void writeHandler(const boost::system::error_code& error)
    {
        if (!error)
        {
        } else
        {
            closeImpl();
        }
    }

    void closeImpl()
    {
        socket_.lowest_layer().close();
    }
};

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 4)
        {
            std::cerr << "Usage: chat_client <host> <port> <nickname>\n";
            return 1;
        }
        boost::asio::io_context io_context;
        tcp::resolver resolver(io_context);
        tcp::resolver::query query(argv[1], argv[2]);
        auto endpoints = resolver.resolve(query);
        
        std::array<char, MAX_NICKNAME> nickname;
        strcpy(nickname.data(), argv[3]);

        boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
        ctx.load_verify_file("rootca.crt"); //ca.pem

        client cli(nickname, io_context, ctx, endpoints);

        std::thread t(boost::bind(&boost::asio::io_context::run, &io_context));

        // std::array<char, MAX_BUFFER_SIZE> msg;
        Message msg;
        std::cout << "-------------------------------------\n";
        std::cout << "create chat room: -c <chat_room_name>\nenter chat room: -i <chat_room_name>\nleave chat room: -x\n";
        std::cout << "Secrete chat room: -s <nick name> ~\n";
        std::cout << "send file: -f <file_name>\n";
        std::cout << "-------------------------------------\n\n";

        while (true)
        {
            memset(msg.data.data(), '\0', msg.data.size());
            if (!std::cin.getline(msg.data.data(), MAX_BUFFER_SIZE - PADDING - MAX_NICKNAME))
            {
                std::cin.clear(); //clean up error bit and try to finish reading
            }
            cli.write(msg);
        }

        cli.close();
        t.join();
    } catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
