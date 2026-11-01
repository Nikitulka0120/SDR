#!/usr/bin/env python3
"""Строит созвездие I/Q из файла pluto1.pcm (int16 interleaved)."""

import numpy as np
import matplotlib.pyplot as plt

PCM_PATH = "build/pluto1.pcm"
DECIMATION = 10

raw = np.fromfile(PCM_PATH, dtype=np.int16)

even_count = raw.size // 2 * 2
raw = raw[:even_count]

i = raw[0::2]
q = raw[1::2]
z = i + 1j * q

print(f"Прочитано комплексных сэмплов: {len(z)}")

fig, axes = plt.subplots(2, 5, figsize=(15, 6), sharex=True, sharey=True)

for k, ax in enumerate(axes.flatten()):
    zk = z[k::DECIMATION]
    ax.scatter(zk.real, zk.imag, s=2, alpha=0.35)
    ax.set_aspect("equal")
    ax.grid(True, alpha=0.5)
    ax.set_title(f"offset = {k}", fontsize=10)

plt.tight_layout()
plt.show()