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
#include "TelegramSpeechToTextBot.h"


int main() {
    std::cout << "Bot is running..." << std::endl;
    std::string lastUpdateId = "0";

    TelegramBot tb(BOT_TOKEN);
    tb.run();

    return 0;
}