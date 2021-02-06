#include <iostream>
#include <deque>
#include "boost/asio.hpp"

using namespace boost::asio;

typedef std::deque<std::string> chat_message_queue;

class chat_client {
public:
    chat_client(boost::asio::io_context& io_context,
                const ip::tcp::resolver::results_type& endpoints)
                : io_context_(io_context),
                socket_(io_context) {
        connect(endpoints);
    }

    void close() {
        boost::asio::post(io_context_,[this](){socket_.close();});
    }

    void write(const std::string& msg) {
        boost::asio::post(io_context_, [this, msg]() {
            bool write_message = !message_queue.empty();
            message_queue.push_back(msg);
            if (!write_message) {
                do_write();
            }
        });
    }

private:
    void connect(const ip::tcp::resolver::results_type& endpoints) {
        boost::asio::async_connect(socket_, endpoints, [this]
                (boost::system::error_code ec, const ip::tcp::endpoint&){
           if(!ec)
               do_read();
        });
    }
    void do_read() {
        boost::asio::async_read_until(socket_, buf, "\n\r", [this]
        (boost::system::error_code ec, std::size_t size) {
            if(!ec) {
                on_read(ec, size);
            } else {
                socket_.close();
            }
        });
    }

    void on_read(boost::system::error_code ec, std::size_t bytes_transferred) {

        if(!ec) {

            std::stringstream message;
            message << socket_.remote_endpoint(ec) << "> " << std::istream(&buf).rdbuf();
            buf.consume(bytes_transferred);
            std::cout << message.str();
            do_read();
        }
        else {
            socket_.close();
        }
    }

    void do_write() {

        async_write(socket_, boost::asio::buffer(message_queue.front().data(),
                                                 message_queue.front().length()),
                    [this](boost::system::error_code ec, std::size_t size) {

            if(!ec) {
                message_queue.pop_front();
                if(message_queue.empty()) {
                    do_write();
                }
            } else {
             socket_.close();
            }
        });
    }


    boost::asio::io_context& io_context_;
    ip::tcp::socket socket_;
    boost::asio::streambuf buf;
    chat_message_queue message_queue;

};


int main() {

    try {
        boost::asio::io_context io_context;
        ip::tcp::resolver resolver(io_context);
        ip::tcp::resolver::query query("127.0.0.1","1234");
        auto endpoint = resolver.resolve(query);
        chat_client client(io_context, endpoint);

        std::thread t([&io_context](){io_context.run();});
        std::string line;
        while (std::getline(std::cin, line)) {
            client.write(line);
        }
        client.close();
        t.join();
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
