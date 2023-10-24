#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <ctime>
#include <iomanip>
#include <sstream>

using boost::asio::ip::tcp;

#pragma pack(push, 1)
struct LogRecord {
    char sensor_id[32]; // supondo um ID de sensor de até 32 caracteres
    std::time_t timestamp; // timestamp UNIX
    double value; // valor da leitura
};
#pragma pack(pop)

std::time_t string_to_time_t(const std::string& time_string) {
    std::tm tm = {};
    std::istringstream ss(time_string);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return std::mktime(&tm);
}

class session
  : public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket)
    : socket_(std::move(socket))
  {
  }

  void start()
  {
    read_message();
  }

private:
  void read_message()
  {
    auto self(shared_from_this());
    boost::asio::async_read_until(socket_, buffer_, "\r\n",
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          {
            std::istream is(&buffer_);
            std::string message(std::istreambuf_iterator<char>(is), {});
            std::cout << "Received: " << message << std::endl;


            //separando a mensagem recebida em partes
            std::string msg = "";
            std::string id = "";
            std::string data = "";
            std::string leitura = "";
            int contador_separadores = 0;

            for (size_t i = 0; i < message.length(); ++i) {
                if(message[i] != '|' && contador_separadores == 0) {
                    msg += message[i];
                }
                else if(message[i] != '|' && contador_separadores == 1) {
                    id += message[i];
                }
                else if(message[i] != '|' && contador_separadores == 2) {
                    data += message[i];
                }
                else if(message[i] != '|' && contador_separadores == 3) {
                    leitura += message[i];
                }
                else if (message[i] == '|') {
                    contador_separadores++;
                }
            }

            //analisando cada mensagem
            if (msg == "LOG") {
                //criando o vetor de char
                char sensor_id[32];
                for (int i = 0; i < 32; i++) {
                    sensor_id[i] = '#';
                }
                //atribuindo o id ao vetor
                for (int i = 0; i < id.length(); i++) {
                    sensor_id[i] = id[i];
                }

                //convertendo a data
                std::time_t time = string_to_time_t(data);

                //convertendo a leitura
                double value = std::stod(leitura);
                
                //criando o registro
                LogRecord rec;
                for (int i =0; i<32; i++) {
                    rec.sensor_id[i] = sensor_id[i];
                }
                rec.timestamp = time;
                rec.value = value;

            }
            write_message(message);
          }
        });
  }
  
  void write_message(const std::string& message)
  {
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(message),
        [this, self, message](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            read_message();
          }
        });
  }

  tcp::socket socket_;
  boost::asio::streambuf buffer_;
};

class server
{
public:
  server(boost::asio::io_context& io_context, short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
  {
    accept();
  }

private:
  void accept()
  {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket)
        {
          if (!ec)
          {
            std::make_shared<session>(std::move(socket))->start();
          }

          accept();
        });
  }

  tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
  if (argc != 2)
  {
    std::cerr << "Usage: chat_server <port>\n";
    return 1;
  }

  boost::asio::io_context io_context;

  server s(io_context, std::atoi(argv[1]));

  io_context.run();

  return 0;
}
