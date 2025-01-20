#include <iostream>
#include <fstream>
#include <filesystem>
#include "config.h"
#include "TelegramSpeechToTextBot.h"


int main() {
    TelegramBot tb(BOT_TOKEN);
    tb.run();

    return 0;
}