#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/http.hpp>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

class HTTPClient {
public:
    HTTPClient() : ioc_(), ssl_ctx_(asio::ssl::context::tlsv12_client) {}

    std::string sendGetRequest(const std::string& url) {
        return sendRequest(url, "", http::verb::get, "application/json");
    }

    std::string sendPostRequest(const std::string& url, const std::string& data, const std::string& content_type) {
        return sendRequest(url, data, http::verb::post, content_type);
    }

private:
    asio::io_context ioc_;
    asio::ssl::context ssl_ctx_;

    std::string sendRequest(const std::string& url, const std::string& data, http::verb method, const std::string& content_type) {
        // Parse the URL
        auto const scheme_end = url.find("://");
        if (scheme_end == std::string::npos) {
            throw std::invalid_argument("Invalid URL: no scheme found");
        }

        auto const scheme = url.substr(0, scheme_end);
        auto const host_start = scheme_end + 3;
        auto const host_end = url.find('/', host_start);
        auto const host_port = url.substr(host_start, host_end - host_start);
        auto const target = url.substr(host_end);

        // Separate host and port
        auto const colon_pos = host_port.find(':');
        std::string host = host_port.substr(0, colon_pos);
        std::string port = (colon_pos == std::string::npos) ? (scheme == "https" ? "443" : "80") : host_port.substr(colon_pos + 1);

        // Resolve the host
        asio::ip::tcp::resolver resolver(ioc_);
        auto const results = resolver.resolve(host, port);

        if (scheme == "https") {
            // Stream for HTTPS (beast::ssl_stream wraps a TCP stream with SSL)
            beast::ssl_stream<beast::tcp_stream> stream(ioc_, ssl_ctx_);

            // Connect to the host
            beast::get_lowest_layer(stream).connect(results);

            // Perform SSL handshake
            stream.handshake(asio::ssl::stream_base::client);

            // Set up the HTTP request
            http::request<http::string_body> req{ method, target, 11 };
            req.set(http::field::host, host + ":" + port);
            req.set(http::field::content_type, content_type);
            req.body() = data;
            req.prepare_payload();

            // Send the request
            http::write(stream, req);

            // Receive the response
            beast::flat_buffer buffer;
            http::response<http::string_body> res;
            http::read(stream, buffer, res);

            // Shutdown the SSL connection
            beast::error_code ec;
            stream.shutdown(ec);
            if (ec && ec != beast::errc::not_connected) {
                std::cerr << ec.what() << std::endl;
            }

            // Return the response body
            return res.body();
        }
        else if (scheme == "http") {
            // Stream for HTTP
            beast::tcp_stream stream(ioc_);

            // Connect to the host
            beast::error_code ec;
            stream.connect(results, ec);
            if (ec) {
                std::cerr << "Error connecting to host: " << ec.message() << std::endl;
                BOOST_THROW_EXCEPTION(std::runtime_error("Error connecting to host: " + ec.message()));
            }

            // Set up the HTTP request
            http::request<http::string_body> req{ method, target, 11 };
            req.set(http::field::host, host + ":" + port);
            req.set(http::field::content_type, content_type);
            req.body() = data;
            req.prepare_payload();

            // Send the request
            http::write(stream, req);

            // Receive the response
            beast::flat_buffer buffer;
            http::response<http::string_body> res;
            http::read(stream, buffer, res);

            // Return the response body
            return res.body();
        }
        else {
            throw std::invalid_argument("Unsupported URL scheme: " + scheme);
        }
    }
};

class TelegramBot {
public:
    TelegramBot(const std::string& bot_token)
        : bot_token_(bot_token),
          base_url_file_bot("https://api.telegram.org/file/bot" + bot_token),
          base_url_bot("https://api.telegram.org/bot" + bot_token),
          http_client() {}

    void handleUpdates(const json::value& updates) {
        for (const auto& update : updates.as_array()) {
            if (update.as_object().contains("message")) {
                auto message = update.as_object().at("message").as_object();
                auto chatId = message.at("chat").as_object().at("id").as_int64();
                auto responseUrl = base_url_bot + "/sendMessage";

                if (message.contains("voice")) {
                    auto fileId = message.at("voice").as_object().at("file_id").as_string();
                    std::string url = base_url_bot + "/getFile?file_id=" + fileId.c_str();
                    std::string response = http_client.sendGetRequest(url);

                    auto parsed = json::parse(response);
                    auto root = parsed.as_object();
                    if (root["ok"].as_bool()) {
                        auto result = root.at("result").as_object();
                        auto filePath = result.at("file_path").as_string();

                        // get file url for download
                        std::string url = base_url_file_bot + "/" + filePath.c_str();
                        std::filesystem::path fileFullPath = std::filesystem::current_path() / "files" / (std::string(fileId.c_str()) + ".ogg");
                        std::string fileName = fileFullPath.string();

                        // download file
                        std::ofstream outfile(fileName, std::ofstream::binary);
                        outfile << http_client.sendGetRequest(url);
                        outfile.close();
                        std::cout << "Saved to " << fileName << std::endl;

                        // make post request to api
                        std::string postData = "path=" + fileName;
                        std::string response = http_client.sendPostRequest("http://localhost:8080/convert", postData, "application/x-www-form-urlencoded");

                        // get json from response
                        auto parsed = json::parse(response);
                        auto text = parsed.at("text").as_string();
                        std::cout << text << std::endl;

                        http_client.sendPostRequest(responseUrl, json::serialize(json::object({
                            {"chat_id", chatId},
                            {"text", text}
                        })), "application/json");
                    }
                }
            }
        }
    }

    void run() {
        std::cout << "Bot is running..." << std::endl;
        std::string lastUpdateId = "0";

        while (true) {
            std::string url = base_url_bot + "/getUpdates?offset=" + lastUpdateId;
            std::string response = http_client.sendGetRequest(url);

            auto parsed = json::parse(response);
            auto root = parsed.as_object();
            if (root["ok"].as_bool()) {
                auto updates = root["result"];
                if (!updates.as_array().empty()) {
                    std::cerr << response << std::endl;
                    lastUpdateId = std::to_string(updates.as_array().back().as_object()["update_id"].as_int64() + 1);
                    handleUpdates(updates);
                }
            }

            // Delay to avoid flooding Telegram servers
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

private:
    std::string bot_token_;
    std::string base_url_file_bot;
    std::string base_url_bot;
    HTTPClient http_client;
};
