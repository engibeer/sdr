#include <al.h>    // OpenAL header files
#include <alc.h>
#include <iostream>

#include "mkl_dfti.h"

#include <list>

using std::list;

#define FREQ 44100   // Sample rate
#define CAP_SIZE 4096 // How much to capture at a time (affects latency)
#define NFFT 4096    // number of FFT bins
#define FILTERTAPS 2048 // number of taps in bandpass filter

int main(int argC, char* argV[])
{
	std::cout << "running...";
    list<ALuint> bufferQueue; // A quick and dirty queue of buffer objects

    ALenum errorCode = 0;
    ALuint helloBuffer[16], helloSource[1];
    ALCdevice* audioDevice = alcOpenDevice(NULL); // Request default audio device
    errorCode = alcGetError(audioDevice);
    ALCcontext* audioContext = alcCreateContext(audioDevice, NULL); // Create the audio context
    alcMakeContextCurrent(audioContext);
    errorCode = alcGetError(audioDevice);
    // Request the default capture device with a half-second buffer
    ALCdevice* inputDevice = alcCaptureOpenDevice(NULL, FREQ, AL_FORMAT_MONO16, FREQ/2);
    errorCode = alcGetError(inputDevice);
    alcCaptureStart(inputDevice); // Begin capturing
    errorCode = alcGetError(inputDevice);

    alGenBuffers(16, &helloBuffer[0]); // Create some buffer-objects
    errorCode = alGetError();

    // Queue our buffers onto an STL list
    for (int ii=0; ii < 16; ++ii) {
        bufferQueue.push_back(helloBuffer[ii]);
    }

    alGenSources(1, &helloSource[0]); // Create a sound source
    errorCode = alGetError();

    short buffer[FREQ*2]; // A buffer to hold captured audio
	short RealIn[NFFT]; // FFT buffers
	short ImagIn[NFFT];

    ALCint samplesIn=0;  // How many samples are captured
    ALint availBuffers=0; // Buffers to be recovered
    ALuint myBuff; // The buffer we're using
    ALuint buffHolder[16]; // An array to hold catch the unqueued buffers
    bool done = false;
    while (!done) { // Main loop

        // Poll for recoverable buffers
        alGetSourcei(helloSource[0], AL_BUFFERS_PROCESSED, &availBuffers);
        if (availBuffers > 0) {
            alSourceUnqueueBuffers(helloSource[0], availBuffers, buffHolder);
            for (int ii=0; ii < availBuffers; ++ii) {
                // Push the recovered buffers back on the queue
                bufferQueue.push_back(buffHolder[ii]);
            }
        }

        // Poll for captured audio
        alcGetIntegerv(inputDevice, ALC_CAPTURE_SAMPLES, 1, &samplesIn);
        if (samplesIn > CAP_SIZE) {
            // Grab the sound
            alcCaptureSamples(inputDevice, buffer, CAP_SIZE);
   
			//The DSP subroutine performs modulation and demodulation of single sideband signals.
			//The IF is offset from baseband by 11.025KHz.  Filtering is accomplished using FFT
			//Convolution and overlap/add.  Variable hang time digital AGC is also provided.
			//
			//Parameters:
			//inBuffer() - buffer containing the quadrature sampled signal from the ADC
			//outBuffer() - buffer returned by the DSP subroutine for output to the DAC

			//================================ COPY BUFFERS ======================================
            for (int ii = 0; ii < CAP_SIZE-1; ++ii) {
			  RealIn[ii / 2] = buffer[ii];
              ImagIn[ii / 2] = buffer[ii + 1];
			  buffer[ii] *= .1; // Make it quieter, just for kicks
            }

			//============================== FFT CONVERSION ======================================
			//float _Complex x[32];
			float y[34];
			DFTI_DESCRIPTOR_HANDLE my_desc1_handle;
			DFTI_DESCRIPTOR_HANDLE my_desc2_handle;
			MKL_LONG status;
			//...put input data into x[0],...,x[31]; y[0],...,y[31]
			status = DftiCreateDescriptor( &my_desc1_handle, DFTI_SINGLE, DFTI_COMPLEX, 1, 32);
			status = DftiCommitDescriptor( my_desc1_handle );

			status = DftiComputeForward( my_desc1_handle, ImagIn);

			status = DftiFreeDescriptor(&my_desc1_handle);
			/* result is x[0], ..., x[31]*/
			status = DftiCreateDescriptor( &my_desc2_handle, DFTI_SINGLE, DFTI_REAL, 1, 32);
			status = DftiCommitDescriptor( my_desc2_handle);
			status = DftiComputeForward( my_desc2_handle, y);
			status = DftiFreeDescriptor(&my_desc2_handle);
			/* result is given in CCS format*/
			//nspzrFftNip RealIn, ImagIn, RealOut, ImagOut, order, NSP_Forw
    
		    //nspdbrCartToPolar RealOut, ImagOut, M, P, NFFT    // Cartesian to polar



            // Stuff the captured data in a buffer-object
            if (!bufferQueue.empty()) { // We just drop the data if no buffers are available
                myBuff = bufferQueue.front(); bufferQueue.pop_front();
                alBufferData(myBuff,AL_FORMAT_MONO16,buffer,CAP_SIZE*sizeof(short),FREQ);

                // Queue the buffer
                alSourceQueueBuffers(helloSource[0],1,&myBuff);

                // Restart the source if needed
                // (if we take too long and the queue dries up,
                //  the source stops playing).
                ALint sState=0;
                alGetSourcei(helloSource[0], AL_SOURCE_STATE, &sState);
                if (sState != AL_PLAYING) {
                    alSourcePlay(helloSource[0]);
                }
            }
        }
    }
    // Stop capture
    alcCaptureStop(inputDevice);
    alcCaptureCloseDevice(inputDevice);

    // Stop the sources
    alSourceStopv(1,&helloSource[0]);
    for (int ii=0; ii < 1; ++ii) {
        alSourcei(helloSource[ii], AL_BUFFER,0);
    }

    // Clean-up 
    alDeleteSources(1, &helloSource[0]); 
    alDeleteBuffers(16, &helloBuffer[0]);
    errorCode = alGetError();
    alcMakeContextCurrent(NULL);
    errorCode = alGetError();
    alcDestroyContext(audioContext);
    alcCloseDevice(audioDevice);

    return 0;
}