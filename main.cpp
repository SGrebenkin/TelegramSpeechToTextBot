#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <filesystem>

#include <boost/asio/ssl.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/http.hpp>

#include "config.h"


// Telegram bot token (replace with your bot token)
const std::string BASE_URL_FILE_BOT = "https://api.telegram.org/file/bot" + BOT_TOKEN;
const std::string BASE_URL_BOT = "https://api.telegram.org/bot" + BOT_TOKEN;

// Namespace aliases
namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

// Function to send a GET request
std::string sendGetRequest(const std::string& url) {
    // I/O Context
    asio::io_context ioc;

    // SSL Context
    asio::ssl::context ssl_ctx(asio::ssl::context::tlsv12_client);
    /*ssl_ctx.set_verify_mode(asio::ssl::verify_none);
    ssl_ctx.set_options(boost::asio::ssl::context::default_workarounds |
        boost::asio::ssl::context::no_sslv2 |
        boost::asio::ssl::context::no_sslv3);*/

    // Stream for HTTPS (beast::ssl_stream wraps a TCP stream with SSL)
    beast::ssl_stream<beast::tcp_stream> stream(ioc, ssl_ctx);

    // Parse the URL
    auto const pos = url.find("://");
    auto const host = url.substr(pos + 3, url.find('/', pos + 3) - (pos + 3));
    auto const target = url.substr(url.find(host) + host.length());

    // Resolve the host
    asio::ip::tcp::resolver resolver(ioc);
    auto const results = resolver.resolve(host, "https");

    // Connect to the host
    beast::get_lowest_layer(stream).connect(results);

    // Perform SSL handshake
    stream.handshake(asio::ssl::stream_base::client);

    // Set up the HTTP GET request
    http::request<http::string_body> req{ http::verb::get, target, 11 };
    req.set(http::field::host, host);
    req.set(http::field::user_agent, "Boost.Beast/TelegramBot");

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
        //throw beast::system_error{ ec };
    }

    // Return the response body
    return res.body();
}

// Function to send a POST request
std::string sendPostRequest(const std::string& url, const std::string& data) {
    // I/O Context
    asio::io_context ioc;

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
    asio::ip::tcp::resolver resolver(ioc);
    auto const results = resolver.resolve(host, port);

    if (scheme == "https") {
        // SSL Context
        asio::ssl::context ssl_ctx(asio::ssl::context::tlsv12_client);

        // Stream for HTTPS (beast::ssl_stream wraps a TCP stream with SSL)
        beast::ssl_stream<beast::tcp_stream> stream(ioc, ssl_ctx);

        // Connect to the host
        beast::get_lowest_layer(stream).connect(results);

        // Perform SSL handshake
        stream.handshake(asio::ssl::stream_base::client);

        // Set up the HTTP POST request
        http::request<http::string_body> req{ http::verb::post, target, 11 };
        req.set(http::field::host, host);
        req.set(http::field::content_type, "application/json");
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
        beast::tcp_stream stream(ioc);

        // Connect to the host
        beast::error_code ec;
        stream.connect(results, ec);
        if (ec) {
            std::cerr << "Error connecting to host: " << ec.message() << std::endl;
            BOOST_THROW_EXCEPTION(std::runtime_error("Error connecting to host: " + ec.message()));
        }

        // Set up the HTTP POST request
        http::request<http::string_body> req{ http::verb::post, target, 11 };
        req.set(http::field::host, host + ":" + port);
        req.set(http::field::content_type, "application/x-www-form-urlencoded");
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

// Function to handle updates
void handleUpdates(const json::value& updates) {
    for (const auto& update : updates.as_array()) {
        if (update.as_object().contains("message")) {
            auto message = update.as_object().at("message").as_object();
            auto chatId = message.at("chat").as_object().at("id").as_int64();
            auto responseUrl = BASE_URL_BOT + "/sendMessage";

            if (message.contains("voice")) {
				auto fileId = message.at("voice").as_object().at("file_id").as_string();
				std::string url = BASE_URL_BOT + "/getFile?file_id=" + fileId.c_str();
                std::string response = sendGetRequest(url);

                // parse response
                /*{
                    "ok": true,
                        "result" : {
                        "file_id": "AwACAgIAAxkBAAMdZ3LuUla3r5kwsYf2-9e1tfZ-YHsAAjVcAALkjlBJXs4LBAUpCa82BA",
                            "file_unique_id" : "AgADNVwAAuSOUEk",
                            "file_size" : 903152,
                            "file_path" : "voice/file_0.oga"
                    }
                }*/

                auto parsed = json::parse(response);
                auto root = parsed.as_object();
                if (root["ok"].as_bool()) {
					auto result = root.at("result").as_object();
					auto filePath = result.at("file_path").as_string();

                    // get file url for download
                    std::string url = BASE_URL_FILE_BOT + "/" + filePath.c_str();
                    //auto fileName = std::string("files/") + fileId.c_str() + ".ogg";

                    std::filesystem::path fileFullPath = std::filesystem::current_path() / "files" / (std::string(fileId.c_str()) + ".ogg");
                    std::string fileName = fileFullPath.string();

                    // download file
                    std::ofstream outfile(fileName, std::ofstream::binary);
					outfile << sendGetRequest(url);
					outfile.close();
                    std::cout << "Saved to " << fileName << std::endl;

                    // make post request to api
                    //json::object postPredictData{{"path", fileName}};
                    //std::string response = sendPostRequest("http://localhost:8080/convert", json::serialize(postPredictData));

                    std::string postData = "path=" + fileName;
                    std::string response = sendPostRequest("http://localhost:8080/convert", postData);

                    // get json from response
					auto parsed = json::parse(response);
                    auto text = parsed.at("text").as_string();
                    std::cout << text << std::endl;

                    sendPostRequest(responseUrl, json::serialize(json::object({
                        {"chat_id", chatId},
                        {"text", text}
                    })));
                }
            }
        }
    }
}

int main() {
    std::cout << "Bot is running..." << std::endl;
    std::string lastUpdateId = "0";

    //{
    //  "ok": true,
    //  "result": [
    //    {
    //      "update_id": 499172526,
    //      "message": {
    //        "message_id": 21,
    //        "from": {
    //          "id": 124411882,
    //          "is_bot": false,
    //          "first_name": "Sergey",
    //          "last_name": "Grebenkin",
    //          "username": "bingobongooo",
    //          "language_code": "en"
    //        },
    //        "chat": {
    //          "id": 124411882,
    //          "first_name": "Sergey",
    //          "last_name": "Grebenkin",
    //          "username": "bingobongooo",
    //          "type": "private"
    //        },
    //        "date": 1735582427,
    //        "text": "Hi"
    //      }
    //    },
    //    {
    //      "update_id": 499172527,
    //      "message": {
    //        "message_id": 22,
    //        "from": {
    //          "id": 124411882,
    //          "is_bot": false,
    //          "first_name": "Sergey",
    //          "last_name": "Grebenkin",
    //          "username": "bingobongooo",
    //          "language_code": "en"
    //        },
    //        "chat": {
    //          "id": 124411882,
    //          "first_name": "Sergey",
    //          "last_name": "Grebenkin",
    //          "username": "bingobongooo",
    //          "type": "private"
    //        },
    //        "date": 1735583276,
    //        "text": "privet"
    //      }
    //    }
    //  ]
    //}

    while (true) {
        std::string url = BASE_URL_BOT + "/getUpdates?offset=" + lastUpdateId;
        std::string response = sendGetRequest(url);

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

    return 0;
}