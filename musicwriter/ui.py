from PyQt5.QtWidgets import QWidget, QVBoxLayout, QPushButton, QLabel
from PyQt5.QtCore import Qt
import pyqtgraph as pg
import numpy as np

from .audio import AudioRecorder


class MainWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("MusicWriter — Frequency Histogram")
        self.resize(800, 480)

        layout = QVBoxLayout()

        title = QLabel("frequency histogram")
        title.setAlignment(Qt.AlignCenter)
        layout.addWidget(title)

        self.plot = pg.PlotWidget()
        self.plot.setLabel('bottom', 'Frequency', units='Hz')
        self.plot.setLabel('left', 'Magnitude', units='dB')
        self.plot.showGrid(x=True, y=True)
        self.curve = self.plot.plot([], [])
        layout.addWidget(self.plot, stretch=1)

        self.btn = QPushButton("Start audio record")
        self.btn.clicked.connect(self.toggle_recording)
        layout.addWidget(self.btn)

        self.setLayout(layout)

        self.recorder = AudioRecorder()
        self.recorder.spectrum_updated.connect(self.on_spectrum)
        self._running = False

    def toggle_recording(self):
        if not self._running:
            try:
                self.recorder.start()
            except Exception as e:
                self.btn.setText(f"Error: {e}")
                return
            self.btn.setText("Stop recording")
            self._running = True
        else:
            self.recorder.stop()
            self.btn.setText("Start audio record")
            self._running = False

    def on_spectrum(self, freqs, mags):
        # convert to dB for plotting
        mags = np.asarray(mags)
        mags_db = 20 * np.log10(mags + 1e-6)
        self.curve.setData(freqs, mags_db)
