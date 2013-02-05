// - - -

#include <AudioToolbox/AudioToolbox.h>

class Node {
	public:
	virtual void operator() (double buf[], size_t length) = 0;
};

class Output {

	static void audio_queue_cb(void *userData, AudioQueueRef queue, AudioQueueBufferRef buf) {
		static_cast<Output*>(userData)->processBuffer(buf);
	}

	AudioQueueRef m_queue;

	void processBuffer(AudioQueueBufferRef buffer) {

		(*m_input)(
			static_cast<double *>(buffer->mAudioData),
			buffer->mAudioDataBytesCapacity / sizeof(double)
		);

		buffer->mAudioDataByteSize = buffer->mAudioDataBytesCapacity;
		AudioQueueEnqueueBuffer(m_queue, buffer, 0, NULL);
	}

	void require(OSStatus err, const char *name) {
		if (err) {
			fprintf(stderr, "%s: %d\n", name, err);
			exit(1);
		}
	}

	public:

	Node *m_input;

	Output (Node *input) : m_input(input) {

		AudioStreamBasicDescription description;

		description.mSampleRate       = 44100;
		description.mFormatID         = kAudioFormatLinearPCM; 
		description.mFormatFlags      = kAudioFormatFlagIsFloat;
		description.mBytesPerPacket   = sizeof(double);
		description.mFramesPerPacket  = 1;
		description.mBytesPerFrame    = sizeof(double);
		description.mChannelsPerFrame = 1;
		description.mBitsPerChannel   = sizeof(double) * 8;


		require(AudioQueueNewOutput(
			&description, audio_queue_cb, this, CFRunLoopGetCurrent(),
			kCFRunLoopCommonModes, 0, &m_queue
		), "AudioQueueNewOutput");

		for (size_t i = 0; i < 2; i++) {
			AudioQueueBufferRef buffer;
			require(AudioQueueAllocateBuffer(m_queue, 2*8*1024, &buffer), "AudioQueueAllocateBuffer");
			processBuffer(buffer);
		}
	}

	void start() {
		require(AudioQueueStart(m_queue, NULL), "AudioQueueStart");
		CFRunLoopRun();
	}


};

class PopSource : public Node {

	bool state;

	public:

	PopSource() : state(false) {}

	void operator() (double buf[], size_t length) {
		for (size_t i = 0; i < length; i++) {
			*buf++ = state ? 0.5 : 0;
		}

		state = !state;
	}

};

int main () {

	PopSource source;
	Output output(source);

	output.start();

	return 1;
}
