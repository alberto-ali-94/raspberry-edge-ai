# Nova installation

This snapshot targets **macOS on Apple Silicon with Python 3.12**. It captures the Python packages installed in the working development environment on 2026-11-12. Installation from scratch has not yet been tested. MLX makes this snapshot specific to Apple Silicon; use a separate dependency set for other platforms.

## Prerequisites

Install Homebrew, uv, and Ollama if they are not already available. Install the external audio tools:

```sh
brew install ffmpeg espeak-ng
```

FFmpeg decodes and resamples audio for Whisper. eSpeak NG provides pronunciation processing for Kokoro. The Python `ollama` package is a client; the Ollama application/service must be installed and running separately.

## Python environment

From this directory, create an environment only if `.venv` does not already exist:

```sh
uv venv --python 3.12 .venv
```

Install all Python dependencies in one command:

```sh
uv pip install --python .venv/bin/python -r requirements.txt
```

The file pins both direct dependencies and the transitive packages found in the existing environment. It is a version snapshot, not a cross-platform lock file with artifact hashes.

## Model files

With Ollama running, download the exact conversational model:

```sh
ollama pull qwen3:4b-instruct
```

Nova also uses `mlx-community/whisper-small-mlx` for transcription and `hexgrad/Kokoro-82M` with the Italian female voice `if_sara` for speech synthesis. These assets are downloaded on their first use and cached separately; `requirements.txt` does not include them. An Internet connection and sufficient disk space are required for the initial downloads. If `HF_HUB_OFFLINE` is enabled in the script, disable it for this first download.

## Audio devices and startup

List the audio devices on the target Mac:

```sh
.venv/bin/python -m sounddevice
```

Update the microphone and playback device identifiers in `nova.py` to match that list. The current script uses microphone `0`, output `1`, and a fixed three-second recording window. Device identifiers can differ between computers or after hardware changes. Allow microphone access when macOS requests it.

Run Nova from this directory:

```sh
.venv/bin/python nova.py
```

Press Enter to record a turn, or type `q` and press Enter to exit. Conversation history is retained in memory during the session, not across restarts. The current script overwrites `prova.wav` for each recording.
