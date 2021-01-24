#include <iostream>
#include <memory>
#include <boost/asio.hpp>
#include <optional>
#include <unordered_set>
#include <deque>

using namespace boost::asio;

typedef std::deque<std::string> outgoing;

class chat_participant {
public:
    virtual ~chat_participant() = default;
    virtual void deliver(const std::string& message) = 0;
};

typedef std::shared_ptr<chat_participant> chat_participant_ptr;

class chat_room {
public:

    void join(const chat_participant_ptr& participant) {
        participants.insert(participant);
        participant->deliver("Welcome to chat\n\r");
        for (const auto& message: outgoing_message)
            participant->deliver(message);
    }


    void leave(const chat_participant_ptr& participant) {
        participants.erase(participant);
        std::cout << "left" << std::endl;
    }

    void deliver(std::string const& message) {
        outgoing_message.push_front(message);
        while (outgoing_message.size() > max_recent_msgs) {
            outgoing_message.pop_front();
        }
        
        for (const auto& participant: participants)
            participant->deliver(message);
    }

private:
    std::unordered_set<chat_participant_ptr> participants;
    outgoing outgoing_message;
    enum { max_recent_msgs = 100 };
};


class session : public std::enable_shared_from_this<session>, public chat_participant {
public:
    session(ip::tcp::socket socket, chat_room& room)
            : socket_(std::move(socket)), room_(room)
    {
    }

    void start() {
        room_.join(shared_from_this());
        async_read();
    }

    void deliver(const std::string& msg) {
        bool write_in_progress = !message_que.empty();
        message_que.push_back(msg);
        if (!write_in_progress) {
            on_write();
        }
    }


private:

    void async_read() {
        auto self(shared_from_this());
        ::async_read_until(socket_, buf, "\n",
                         [this, self](boost::system::error_code error_code, std::size_t size) {
            if(!error_code) 
                on_read(error_code, size);
            else
                room_.leave(shared_from_this());
        });
    }

    void on_read(boost::system::error_code error, std::size_t bytes_transferred)
    {
        if(!error)
        {
            std::stringstream message;
            message << socket_.remote_endpoint(error) << "> " << std::istream(&buf).rdbuf();
            buf.consume(bytes_transferred);
            room_.deliver(message.str());
            async_read();
        }
        else
        {
            socket_.close(error);
            room_.leave(shared_from_this());
        }
    }

    void on_write()
    {
        auto self(shared_from_this());
        ::async_write(socket_,boost::asio::buffer(
                message_que.front().data(),message_que.front().length()),
                      [this, self](boost::system::error_code ec, std::size_t size) {
            if (!ec) {
                message_que.pop_front();
                if (message_que.empty())
                {
                    on_write();
                }
            } else {
                room_.leave(shared_from_this());
            }
        });
    }


private:
    chat_room& room_;
    ip::tcp::socket socket_;
    boost::asio::streambuf buf;
    outgoing message_que;
};

class server {
public:
    server(boost::asio::io_context& io_context, std::uint16_t port)
            : io_context(io_context),
              acceptor(io_context, ip::tcp::endpoint(ip::tcp::v4(), port))
    {
        async_accept();
    }

    void async_accept() {
        socket.emplace(io_context);
        acceptor.async_accept(*socket, [&] (boost::system::error_code err) {
            if (!err) {
                std::make_shared<session>(std::move(*socket), room_)->start();
            }
            async_accept();
        });
    }
private:
    std::optional<ip::tcp::socket> socket;
    boost::asio::io_context& io_context;
    ip::tcp::acceptor acceptor;
    chat_room room_;
};

int main() {
    try {
        io_context io_context;
        server srv(io_context, 1234);
        io_context.run();
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
