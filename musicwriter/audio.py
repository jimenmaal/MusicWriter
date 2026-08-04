from PyQt5.QtCore import QObject, pyqtSignal
import sounddevice as sd
import numpy as np
import threading
import queue


class AudioRecorder(QObject):
    # emits (freqs, mags)
    spectrum_updated = pyqtSignal(object, object)

    def __init__(self, sample_rate=44100, fft_size=4096, blocksize=1024):
        super().__init__()
        self.sample_rate = sample_rate
        self.fft_size = fft_size
        self.blocksize = blocksize
        self._q = queue.Queue()
        self._stream = None
        self._thread = None
        self._running = False

    def _audio_callback(self, indata, frames, time_info, status):
        if status:
            # ignore for now
            pass
        # push a copy into the queue for processing
        self._q.put(indata.copy())

    def start(self):
        if self._running:
            return
        self._running = True
        try:
            self._stream = sd.InputStream(channels=1, samplerate=self.sample_rate,
                                           callback=self._audio_callback,
                                           blocksize=self.blocksize)
            self._stream.start()
        except Exception:
            self._running = False
            raise

        self._thread = threading.Thread(target=self._process_loop, daemon=True)
        self._thread.start()

    def stop(self):
        self._running = False
        if self._stream is not None:
            try:
                self._stream.stop()
                self._stream.close()
            except Exception:
                pass
            self._stream = None

    def _process_loop(self):
        buffer = np.zeros(self.fft_size, dtype=np.float32)
        window = np.hanning(self.fft_size)
        while self._running:
            try:
                data = self._q.get(timeout=0.1)
            except queue.Empty:
                continue
            # flatten and convert
            frames = np.asarray(data, dtype=np.float32).flatten()
            n = len(frames)
            if n == 0:
                continue
            # shift buffer and append frames
            buffer = np.roll(buffer, -n)
            if n >= self.fft_size:
                buffer[:] = frames[-self.fft_size:]
            else:
                buffer[-n:] = frames

            # compute FFT
            fft = np.fft.rfft(buffer * window)
            mags = np.abs(fft)
            freqs = np.fft.rfftfreq(self.fft_size, d=1.0 / self.sample_rate)

            # select the user requested band 250-6000 Hz
            mask = (freqs >= 250.0) & (freqs <= 6000.0)
            self.spectrum_updated.emit(freqs[mask], mags[mask])
