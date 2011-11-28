import pyaudio
import sys
import numpy
from math import atan
from scipy import fft
from array import array
from pylab import *

chunk_size = 1024
FORMAT = pyaudio.paInt16
CHANNELS = 1
RATE = 44100
upper_sideband = True

p = pyaudio.PyAudio()

stream = p.open(format = FORMAT,
                channels = CHANNELS,
                rate = RATE,
                input = True,
                output = False,
                frames_per_buffer = chunk_size)

print "* recording"

while True:
    """
    This DSP subroutine performs modulation and demodulation of single sideband (SSB) signals.
    The IF is offset from baseband by 11.025KHz.  Filtering is accomplished using FFT
    Convolution and overlap/add.  Variable hang time digital AGC is also provided.
	"""
	# read string of bytes from line in or mic
	chunk = stream.read(chunk_size)
	# represent bytes as signed shorts
	signal = array('h', chunk)
	complex_dft = fft(signal)
	polar_dft = cartToPolar(complex_dft)
	IFshift(polar_dft)
	select_sideband(polar_dft)
	

def cartToPolar(signal):
	"""
	takes a complex numpy array (after DFT) and returns the polar representation
	stores polar coordinates as tuples of the form (magnitude, angle) in a numpy array
	should be O(n)
	"""
	polar_array = numpy.empty((len(signal),), dtype=object)
	for (i, point) in enumerate(signal):
		magnitude = sqrt(point.real**2 + point.imag**2)
		phase = atan(point.imag / point.real)
		polar_array[i] = (magnitude, phase)
	return polar_array
	
def IFshift(signal):
	"""
	takes polar signal representation and shifts sidebands from 11.025KHz IF
	TODO: subclass dtype to make polar indexing more expressive
	"""
	for (i, point) in enumerate(signal):
		if upper_sideband: 					   # Move upper sideband to 0Hz
			signal[i][0] = signal[i + 1024][0] # shift magnitude
			signal[i][1] = signal[i + 1024][1] # shift phase
		else: 					   			   # Move lower sideband to 0Hz
			signal[i + 3072][0] = signal[i + 1][0]
			signal[i + 3072][1] = signal[i + 1][1]

def selectSideband(signal):
	"""
	zeroes out appropriate sideband
	"""
	if upper_sideband:
		for (i, point) in enumerate(signal[0:len(signal/2)]):
			point[0] = 0
	else:
		for (i, point) in enumerate(signal[len(signal/2):]):
			point[0] = 0
		
def plot(signal):
	"""
	plots the DFT of the signal, just for fun
	assumed fft already performed
	"""
	n = len(signal)
	# calculate number of unique points
	unique_pts = ceil((n+1)/2.0)
	# FFT is symmetric, throw away second half
	p = p[0:unique_pts]
	# take magnitude
	p = abs(p)
	# scale by the number of points
	p = p / float(n)
	p = p**2  # square it to get the power 
	
	# Since we dropped half the FFT, we multiply by 2 to keep the same energy.
	# The DC component and Nyquist component, if it exists, are unique and should not be multiplied by 2.
	
    if n % 2 > 0: # odd number of pts
        p[1:len(p)] = p[1:len(p)] * 2
    else:
        p[1:len(p) - 1] = p[1:len(p) - 1] * 2 # even number of points
	
    freqArray = arange(0, unique_pts, 1.0) * (RATE / n)
	# scale to kHz
    plot(freqArray/1000, 10*log10(p), color='k')
    xlabel('Frequency (kHz)')
    ylabel('Power (dB)')

stream.stop_stream()
stream.close()
p.terminate()