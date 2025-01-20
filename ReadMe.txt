This is the bot that is able to convert the wav files, voices sent by the user in telegram, to human readable text.

The bot consists of two parts:
* C++ part that is actually bot, polls telegram and reponds with the resulting message
* ML part, that is actually a model from hugging face

To use the bot, you need to install the following separate software applications, that are necessary for wav file preparation:
* ffmpeg https://www.ffmpeg.org/
* sox https://sourceforge.net/projects/sox/

FFMpeg converts ogg to wav.
SOX normalizes the volume.

In order to compile, you need to create config.h file in the root directory of the project and put the following line there:
const std::string BOT_TOKEN = "YOUR_TELEGRAM_BOT_TOKEN";

The bot has been tested on NVidia GeForce RTX 4070Ti 12 Gb + Core i7 14400 / 16 Gb DDR.
Additional changes:
* Flash Attention v2 library has been built from sources and installed as a python library to speed-up inference.
* Torch 2.4.1+cu124 version has been used.

ToDo:
- Add description of the solution
- Add language recognizer https://huggingface.co/speechbrain/lang-id-voxlingua107-ecapa
- Train your own model https://heartbeat.comet.ml/using-machine-learning-for-language-detection-517fa6e68f22