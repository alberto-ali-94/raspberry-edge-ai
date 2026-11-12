import sounddevice as sd
import soundfile as sf

microfono = 0
durata = 3  # secondi

info = sd.query_devices(microfono, "input")
frequenza = int(info["default_samplerate"])
numero_campioni = int(durata * frequenza)

print(f"Registrazione di {durata} secondi. Parla ora!", flush=True)

audio = sd.rec(
    numero_campioni,
    samplerate=frequenza,
    channels=1,
    device=microfono,
    dtype="float32",
)

sd.wait()

print("Registrazione terminata.")
print("Frequenza:", frequenza, "Hz")
print("Forma:", audio.shape)
print("Tipo:", audio.dtype)

print("Riascolto...")

sd.play(audio, samplerate=frequenza, device=1)
sd.wait()

print("Riascolto terminato.")

sf.write("prova.wav", audio, frequenza, subtype="FLOAT")
print("Audio salvato in prova.wav")