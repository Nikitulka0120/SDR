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

z = z[9::DECIMATION]

fig, ax = plt.subplots(figsize=(8, 8))
ax.scatter(z.real, z.imag, s=4, alpha=0.35)
ax.set_aspect("equal")
ax.grid(True)
ax.set_xlabel("I")
ax.set_ylabel("Q")
ax.set_title("pluto1.pcm")
plt.show()