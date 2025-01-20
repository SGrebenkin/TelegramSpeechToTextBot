import os
import signal
import uvicorn
from fastapi import FastAPI, Form
from pydantic import BaseModel
import subprocess
import random
import string

import torch
import numpy as np
import soundfile as sf
from transformers import WhisperForConditionalGeneration, WhisperProcessor, pipeline
from scipy.io.wavfile import read
from pydub import AudioSegment


def get_pipeline():

    torch_dtype = torch.bfloat16 # set your preferred type here

    device = 'cpu'
    if torch.cuda.is_available():
        device = 'cuda'
    elif torch.backends.mps.is_available():
        device = 'mps'
        setattr(torch.distributed, "is_initialized", lambda : False) # monkey patching
    device = torch.device(device)

    whisper = WhisperForConditionalGeneration.from_pretrained(
        "antony66/whisper-large-v3-russian", torch_dtype=torch_dtype, low_cpu_mem_usage=True, use_safetensors=True,
        attn_implementation="flash_attention_2" #if your GPU supports it
    )

    processor = WhisperProcessor.from_pretrained("antony66/whisper-large-v3-russian")

    return pipeline(
        "automatic-speech-recognition",
        model=whisper,
        tokenizer=processor.tokenizer,
        feature_extractor=processor.feature_extractor,
        max_new_tokens=256,
        chunk_length_s=30,
        batch_size=16,
        return_timestamps=True,
        torch_dtype=torch_dtype,
        device=device,
    )

app = FastAPI()
asr_pipeline = get_pipeline()

class Task(BaseModel):
    path: str

@app.get("/")
async def root():
    return {'Hello world!'}

@app.post("/convert")
async def convert(path: str = Form(...)):
    name = ''.join(random.choice(string.ascii_uppercase + string.digits) for _ in range(6))
    dest = path+'.{}.wav'.format(name)
    dest_normalized = path+'.{}-normalized.wav'.format(name)
    process = subprocess.run(['ffmpeg', '-i', path, dest])
    print("Running ffmpeg -i " + path + " " + dest)

    if process.returncode != 0:
        return {"Something went wrong with ffmpeg"}

    process = subprocess.run([
        'sox',
        dest,
        '-r', '16k',
        dest_normalized,
        'norm', '-0.5',
        'compand', '0.3,1',
        '-90,-90,-70,-70,-60,-20,0,0',
        '-5', '0', '0.2'
    ])
    if process.returncode != 0:
        return {"Something went wrong with sox"}

    # Load the audio file (Ogg Vorbis or other formats)
    with open(dest_normalized, 'rb') as f:
        audio_data, sample_rate = sf.read(f)

    # If stereo, take the first channel
    if audio_data.ndim > 1:
        audio_data = audio_data[:, 0]

    # Normalize the audio data
    audio_data = audio_data.astype(np.float32)
    audio_data /= np.max(np.abs(audio_data))

    # Normalize the audio data
    audio_data /= np.max(np.abs(audio_data))

    # get the transcription
    # asr = asr_pipeline(audio_data, generate_kwargs={"language": "russian", "max_new_tokens": 256}, return_timestamps=False)
    asr = asr_pipeline(audio_data, generate_kwargs={"max_new_tokens": 256}, return_timestamps=False)

    print(asr)

    os.remove(path)
    os.remove(dest)
    os.remove(dest_normalized)

    return asr

if __name__ == "__main__":
    uvicorn.run(app, host="localhost", port=8080)