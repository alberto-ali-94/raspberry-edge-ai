import logging
import os
import warnings

# Usa i modelli già scaricati, senza richieste al Hub.
#os.environ["HF_HUB_OFFLINE"] = "1"
os.environ["HF_HUB_DISABLE_PROGRESS_BARS"] = "1"

# Avviso noto nella configurazione RNN usata da Kokoro.
warnings.filterwarnings(
    "ignore",
    message=r"dropout option adds dropout after all but last recurrent layer.*",
    category=UserWarning,
    module=r"torch\.nn\.modules\.rnn$",
)

# Avviso di deprecazione interno alla dipendenza.
warnings.filterwarnings(
    "ignore",
    message=r".*torch\.nn\.utils\.weight_norm.*deprecated.*",
    category=FutureWarning,
    module=r"torch\.nn\.utils\.weight_norm$",
)

class FiltroRedirectMac(logging.Filter):
    def filter(self, record):
        return (
            record.getMessage()
            != "NOTE: Redirects are currently not supported in MacOs."
        )

logging.getLogger(
    "torch.distributed.elastic.multiprocessing.redirects"
).addFilter(FiltroRedirectMac())







from time import perf_counter
import sounddevice as sd
import soundfile as sf
import mlx_whisper
import ollama
import sounddevice as sd
from kokoro import KPipeline
import emoji



microfono = 0
durata = 3  # secondi

info = sd.query_devices(microfono, "input")
frequenza = int(info["default_samplerate"])
numero_campioni = int(durata * frequenza)





# Kokoro viene inizializzato una sola volta.
inizio = perf_counter()

voce = KPipeline(
    lang_code="i",
    repo_id="hexgrad/Kokoro-82M",
    device="cpu",
)

print(f"Caricamento Kokoro: {perf_counter() - inizio:.2f} s")


messaggi = [
    {
        "role": "system",
        "content": (
            "Ti chiami Nova. Rispondi brevemente in italiano "
        ),
    }
]




while True:
    comando = input("\nInvio per parlare, q per uscire: ")

    if comando == "q":
        break

    print("Parla ora",flush=True)
    audio = sd.rec(
        numero_campioni,
        samplerate=frequenza,
        channels=1,
        device=microfono,
        dtype="float32",
    )
    sd.wait()
    sf.write("prova.wav", audio, frequenza, subtype="FLOAT")




    # Trascrizione: Whisper riutilizza il modello dopo la prima chiamata.
    inizio = perf_counter()

    risultato = mlx_whisper.transcribe(
        "prova.wav",
        path_or_hf_repo="mlx-community/whisper-small-mlx",
        language="it",
        task="transcribe",
    )

    print(f"Tempo Whisper: {perf_counter() - inizio:.2f} s")

    testo = risultato["text"]
    print("Trascrizione:", testo)

    messaggi.append({
    "role": "user",
    "content": testo,
    })

    inizio = perf_counter()

    risposta = ollama.chat(
        model="qwen3:4b-instruct",
        messages=messaggi,
    )

    print(f"Tempo Qwen: {perf_counter() - inizio:.2f} s")
    
    #Il testo generato lo inserisco nel contesto prima di darlo a Qwen, in modo che il modello
    #abbia lo storico delle risposte
    testo_risposta = risposta.message.content
    messaggi.append({
        "role": "assistant",
        "content": testo_risposta,
    })
    
    print("Nova:", testo_risposta)

    #Qwen scrive spesso delle Emoji, le rimuovo per non farle leggere da Kokoro
    testo_per_voce = emoji.replace_emoji(testo_risposta, replace=" ")
    testo_per_voce = " ".join(testo_per_voce.split())
    if not testo_per_voce:
        continue

    inizio = perf_counter()
    for segmento, fonemi, audio_voce in voce(
        testo_per_voce,
        voice="if_sara",
        speed=1.0,
    ):
        print(
            f"Generazione segmento: "
            f"{perf_counter() - inizio:.2f} s"
        )

        sd.play(audio_voce.numpy(), samplerate=24000, device=1)
        sd.wait()

        inizio = perf_counter()

print("Programma terminato.")