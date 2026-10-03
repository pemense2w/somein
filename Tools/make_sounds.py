"""
Gem Roller - synthesises every sound effect, standard library only.

    python Tools/make_sounds.py [output_dir]      # default: Assets/Audio

Writes 16-bit mono 44.1 kHz WAV files; Tools/ImportAssets.py imports them into Unreal as SW_<name>.
"""

import math
import os
import random
import struct
import sys
import wave

SR = 44100


def n(seconds):
    return int(seconds * SR)


def silence(seconds):
    return [0.0] * n(seconds)


def mix(*tracks):
    """Sum tracks, each given as (samples, start_seconds)."""
    length = max(len(s) + n(at) for s, at in tracks)
    out = [0.0] * length
    for samples, at in tracks:
        o = n(at)
        for i, v in enumerate(samples):
            out[o + i] += v
    return out


def tone(freq, dur, vol=1.0, decay=6.0, attack=0.004, partials=((1.0, 1.0),), vibrato=(0.0, 0.0), wave_kind="sine"):
    """One note. partials: (frequency multiple, amplitude). vibrato: (depth in Hz, rate in Hz)."""
    out = []
    phase = [0.0] * len(partials)
    for i in range(n(dur)):
        t = i / SR
        f = freq + vibrato[0] * math.sin(2 * math.pi * vibrato[1] * t)
        env = (1.0 - math.exp(-t / max(attack, 1e-4))) * math.exp(-decay * t)
        v = 0.0
        for k, (mult, amp) in enumerate(partials):
            phase[k] += 2 * math.pi * f * mult / SR
            p = phase[k]
            if wave_kind == "tri":
                s = (2 / math.pi) * math.asin(math.sin(p))
            elif wave_kind == "square":
                s = math.tanh(3.0 * math.sin(p))
            else:
                s = math.sin(p)
            v += s * amp
        out.append(v * env * vol)
    return out


def sweep(f0, f1, dur, vol=1.0, decay=3.0, curve=1.0, vibrato=(0.0, 0.0), wave_kind="sine", attack=0.003):
    out = []
    ph = 0.0
    total = n(dur)
    for i in range(total):
        t = i / SR
        x = (i / max(total - 1, 1)) ** curve
        f = f0 + (f1 - f0) * x + vibrato[0] * math.sin(2 * math.pi * vibrato[1] * t)
        ph += 2 * math.pi * f / SR
        if wave_kind == "saw":
            s = 2 * ((ph / (2 * math.pi)) % 1.0) - 1.0
        elif wave_kind == "tri":
            s = (2 / math.pi) * math.asin(math.sin(ph))
        else:
            s = math.sin(ph)
        env = (1.0 - math.exp(-t / attack)) * math.exp(-decay * t)
        out.append(s * env * vol)
    return out


def noise(dur, vol=1.0, shape=None, smooth=0.0, seed=1):
    """White noise through a one-pole low-pass (smooth 0..1) and an amplitude shape(t_norm)."""
    rng = random.Random(seed)
    out = []
    y = 0.0
    total = n(dur)
    for i in range(total):
        x = rng.uniform(-1, 1)
        y = smooth * y + (1.0 - smooth) * x
        amp = shape(i / max(total - 1, 1)) if shape else 1.0
        out.append(y * amp * vol)
    return out


BELL = ((1.0, 1.0), (2.0, 0.45), (2.76, 0.25), (4.1, 0.12))
GLASS = ((1.0, 1.0), (2.0, 0.3), (3.0, 0.12))


def gem():
    return mix((tone(1318.5, 0.35, 0.8, decay=9.0, partials=BELL), 0.0),
               (tone(1975.5, 0.45, 0.7, decay=8.0, partials=BELL), 0.065))


def golden():
    notes = [1046.5, 1318.5, 1568.0, 2093.0, 2637.0]
    tracks = [(tone(f, 0.55, 0.6, decay=6.0, partials=BELL), 0.06 * i) for i, f in enumerate(notes)]
    tracks.append((tone(4186.0, 0.7, 0.18, decay=5.0), 0.2))
    return mix(*tracks)


def time_pickup():
    return mix((tone(880.0, 0.22, 0.7, decay=7.0, partials=GLASS, wave_kind="tri"), 0.0),
               (tone(1318.5, 0.5, 0.7, decay=5.0, partials=GLASS, wave_kind="tri"), 0.09),
               (sweep(500, 1300, 0.25, 0.25, decay=6.0), 0.0))


def jump():
    return sweep(280, 760, 0.2, 0.7, decay=9.0, curve=0.7)


def pad():
    return mix((sweep(160, 980, 0.55, 0.8, decay=3.2, curve=0.6, vibrato=(35.0, 22.0)), 0.0),
               (tone(110.0, 0.25, 0.5, decay=14.0), 0.0))


def boost():
    whoosh = noise(0.55, 0.9, shape=lambda x: math.sin(math.pi * min(1.0, x * 1.15)) ** 1.5, smooth=0.55, seed=7)
    rise = sweep(180, 720, 0.5, 0.35, decay=1.2, wave_kind="saw", attack=0.05)
    return mix((whoosh, 0.0), (rise, 0.0))


def dash():
    return noise(0.24, 0.9, shape=lambda x: (1.0 - x) ** 2.2 * min(1.0, x * 30), smooth=0.35, seed=3)


def checkpoint():
    tracks = [(tone(f, 0.75, 0.55, decay=3.5, attack=0.012, partials=GLASS, wave_kind="tri"), 0.045 * i)
              for i, f in enumerate([523.25, 659.25, 783.99])]
    tracks.append((tone(1046.5, 0.8, 0.3, decay=4.0, partials=GLASS), 0.14))
    return mix(*tracks)


def fall():
    return mix((sweep(720, 70, 0.85, 0.8, decay=2.2, curve=0.8, vibrato=(25.0, 9.0), wave_kind="tri"), 0.0),
               (noise(0.2, 0.25, shape=lambda x: (1.0 - x), smooth=0.8, seed=11), 0.55))


def win():
    notes = [(523.25, 0.0, 0.3), (659.25, 0.13, 0.3), (783.99, 0.26, 0.3), (1046.5, 0.39, 1.1)]
    tracks = []
    for f, at, dur in notes:
        tracks.append((tone(f, dur + 0.5, 0.6, decay=2.6 if dur > 1 else 5.0, partials=GLASS, wave_kind="tri"), at))
        tracks.append((tone(f / 2, dur + 0.3, 0.35, decay=3.5, wave_kind="tri"), at))
    tracks.append((tone(2093.0, 1.0, 0.18, decay=3.0), 0.45))
    return mix(*tracks)


def lose():
    notes = [(329.63, 0.0), (261.63, 0.3), (220.0, 0.6)]
    tracks = []
    for i, (f, at) in enumerate(notes):
        last = i == len(notes) - 1
        tracks.append((tone(f, 1.0 if last else 0.4, 0.6, decay=2.4 if last else 6.0, partials=((1.0, 1.0), (2.0, 0.2)), wave_kind="tri",
                            vibrato=(4.0, 5.0) if last else (0.0, 0.0)), at))
    return mix(*tracks)


SOUNDS = {
    "Gem": gem, "Golden": golden, "Time": time_pickup, "Jump": jump, "Pad": pad, "Boost": boost,
    "Dash": dash, "Checkpoint": checkpoint, "Fall": fall, "Win": win, "Lose": lose,
}


def write_wav(path, samples, peak=0.85):
    top = max(1e-9, max(abs(s) for s in samples))
    gain = peak / top
    # short fade-out so nothing clicks at the end
    fade = min(len(samples) // 4, n(0.02))
    frames = bytearray()
    for i, s in enumerate(samples):
        if i >= len(samples) - fade:
            s *= (len(samples) - i) / fade
        frames += struct.pack("<h", int(max(-1.0, min(1.0, s * gain)) * 32767))
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(bytes(frames))


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, "Assets", "Audio")
    os.makedirs(out_dir, exist_ok=True)
    for name, fn in SOUNDS.items():
        samples = fn()
        path = os.path.join(out_dir, name + ".wav")
        write_wav(path, samples)
        print("%-10s %.2fs  %s" % (name, len(samples) / SR, os.path.relpath(path, root)))


if __name__ == "__main__":
    main()
