# Builds the game's sound effects in sounds/ from Kenney's "Casino Audio"
# pack (CC0, https://kenney.nl/assets/casino-audio). SDL loads WAV without
# any extra library, so everything is written as 16-bit mono WAV.
#
#   pip install numpy soundfile
#   python3 tools/make_sounds.py path/to/kenney_casino-audio/Audio
import sys, os
import numpy as np
import soundfile as sf

SRC = sys.argv[1] if len(sys.argv) > 1 else 'kenney/Audio'
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'sounds')
SR = 44100
os.makedirs(OUT, exist_ok=True)

def load(name):
    d, sr = sf.read(os.path.join(SRC, name + '.ogg'))
    if d.ndim > 1:
        d = d.mean(axis=1)
    assert sr == SR
    return d

def trim(d, threshold=0.01, tail=0.03):
    idx = np.where(np.abs(d) > threshold)[0]
    if len(idx) == 0:
        return d
    start = max(0, idx[0] - int(0.002 * SR))
    end = min(len(d), idx[-1] + int(tail * SR))
    out = d[start:end].copy()
    fade = min(len(out), int(0.01 * SR))
    out[-fade:] *= np.linspace(1, 0, fade)
    return out

def save(name, d, peak=0.85, sr=SR):
    d = d / max(1e-9, np.abs(d).max()) * peak
    sf.write(os.path.join(OUT, name + '.wav'), d, sr, subtype='PCM_16')

# One-shot effects: (output name, source clip, loudness)
EFFECTS = [
    ('sfx-deal-1', 'card-slide-1', 0.80),
    ('sfx-deal-2', 'card-slide-3', 0.80),
    ('sfx-deal-3', 'card-slide-5', 0.80),
    ('sfx-flip', 'card-place-2', 0.85),
    ('sfx-chips-pay-1', 'chips-stack-1', 0.80),
    ('sfx-chips-pay-2', 'chips-stack-3', 0.80),
    ('sfx-chips-take', 'chips-collide-1', 0.75),
    ('sfx-chip-bet', 'chip-lay-1', 0.70),
    ('sfx-shuffle', 'card-shuffle', 0.80),
]
for out, src, peak in EFFECTS:
    save(out, trim(load(src)), peak)

# A soft "tock" for buttons: two short decaying tones, like tapping a
# wooden tabletop, kept quiet so it sits under the card and chip sounds.
n = int(0.07 * SR)
t = np.arange(n) / SR
tap = (np.sin(2 * np.pi * 1320 * t) * np.exp(-t * 90)
       + 0.6 * np.sin(2 * np.pi * 660 * t) * np.exp(-t * 60))
tap[:int(0.002 * SR)] *= np.linspace(0, 1, int(0.002 * SR))
save('sfx-tap', tap, peak=0.45)

print('wrote', sorted(os.listdir(OUT)))
