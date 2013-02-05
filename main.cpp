// - - -

#include <AudioToolbox/AudioToolbox.h>
#include <functional>

class Output {

	static void audio_queue_cb(void *userData, AudioQueueRef queue, AudioQueueBufferRef buf) {
		reinterpret_cast<Output*>(userData)->processBuffer(buf);
	}

	typedef const std::function<void(AudioQueueBufferRef)> noise_callback_t;

	noise_callback_t m_cb;
	AudioQueueRef m_queue;

	void processBuffer(AudioQueueBufferRef buffer) {

		m_cb(buffer);

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

	Output (noise_callback_t cb) : m_cb(cb) {

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

		AudioQueueBufferRef buffer;
		require(AudioQueueAllocateBuffer(m_queue, 2*8*1024, &buffer), "AudioQueueAllocateBuffer");

		processBuffer(buffer);
	}

	void start() {
		require(AudioQueueStart(m_queue, NULL), "AudioQueueStart");
		CFRunLoopRun();
	}


};

class PopSource {

	bool state;

	public:

	PopSource() : state(false) {}

	void operator() (AudioQueueBufferRef buf) {
		uint32_t frames = buf->mAudioDataBytesCapacity / sizeof(double);
		double *b = (double *)buf->mAudioData;

		for (size_t i = 0; i < frames; i++) {
			*b++ = state ? 0.5 : 0;
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
