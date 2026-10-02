# Builds the game's sound effects in sounds/ from Kenney's "Casino Audio"
# pack (CC0, https://kenney.nl/assets/casino-audio), plus a generated
# casino background loop. SDL loads WAV without any extra library, so
# everything is written as 16-bit mono WAV.
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

# Casino background: a murmur of voices, the odd chip stack being handled
# somewhere nearby, and a faint slot machine chime now and then. Built
# 4 seconds longer than the loop and crossfaded so it loops seamlessly.
rng = np.random.default_rng(21)
AMB_SR = 22050
LOOP = 30.0
total = int((LOOP + 4.0) * AMB_SR)

def bandpass_noise(n, lo, hi):
    spec = np.fft.rfft(rng.standard_normal(n))
    f = np.fft.rfftfreq(n, 1 / AMB_SR)
    spec[(f < lo) | (f > hi)] = 0
    return np.fft.irfft(spec, n)

murmur = np.zeros(total)
t = np.arange(total) / AMB_SR
for v in range(18):
    lo = rng.uniform(250, 600)
    voice = bandpass_noise(total, lo, lo * rng.uniform(2.5, 4.5))
    # syllables: a few bumps a second, with pauses between phrases
    syll = 0.5 + 0.5 * np.sin(2 * np.pi * rng.uniform(3, 6) * t + rng.uniform(0, 6.28))
    phrase = (np.sin(2 * np.pi * rng.uniform(0.1, 0.3) * t + rng.uniform(0, 6.28)) > -0.2).astype(float)
    phrase = np.convolve(phrase, np.ones(2000) / 2000, mode='same')
    murmur += voice * syll ** 2 * phrase * rng.uniform(0.5, 1.0)

# a little room: smear with a short decaying-noise reverb
ir_len = int(0.5 * AMB_SR)
ir = rng.standard_normal(ir_len) * np.exp(-np.linspace(0, 7, ir_len))
murmur = np.fft.irfft(np.fft.rfft(murmur, total + ir_len) * np.fft.rfft(ir, total + ir_len))[:total]
murmur /= np.abs(murmur).max()
amb = murmur * 0.6

def resample(d, factor):
    idx = np.arange(0, len(d), factor)
    return np.interp(idx, np.arange(len(d)), d)

chip_names = ['chips-handle-1', 'chips-handle-2', 'chips-handle-4', 'chips-stack-2', 'chips-stack-4', 'chips-collide-2']
chips = [resample(trim(load(n)), SR / AMB_SR) for n in chip_names]
pos = rng.uniform(0.3, 1.5)
while pos < LOOP + 3:
    c = chips[rng.integers(len(chips))] * rng.uniform(0.10, 0.22)
    s = int(pos * AMB_SR)
    e = min(total, s + len(c))
    amb[s:e] += c[:e - s]
    pos += rng.uniform(0.6, 2.5)

def chime(freqs, dur=0.18):
    out = []
    for fr in freqs:
        n = int(dur * AMB_SR)
        tt = np.arange(n) / AMB_SR
        out.append((np.sin(2 * np.pi * fr * tt) + 0.3 * np.sin(4 * np.pi * fr * tt)) * np.exp(-tt * 9))
    return np.concatenate(out)

for start in np.arange(rng.uniform(2, 5), LOOP, rng.uniform(7, 10)):
    base = rng.choice([523.25, 587.33, 659.25])
    c = chime([base, base * 1.25, base * 1.5, base * 2]) * 0.05
    s = int(start * AMB_SR)
    e = min(total, s + len(c))
    amb[s:e] += c[:e - s]

n_loop = int(LOOP * AMB_SR)
xf = total - n_loop
fade = np.linspace(0, 1, xf)
loop = amb[:n_loop].copy()
loop[:xf] = loop[:xf] * fade + amb[n_loop:] * (1 - fade)
save('sfx-ambience', loop, peak=0.5, sr=AMB_SR)

print('wrote', sorted(os.listdir(OUT)))
