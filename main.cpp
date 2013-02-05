// - - -

#include <AudioToolbox/AudioToolbox.h>
#include <vector>

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
	double m_sample_rate;

	Output (double sample_rate = 44100) : m_sample_rate(sample_rate) {

		AudioStreamBasicDescription description;

		description.mSampleRate       = m_sample_rate;
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

	}

	void start() {
		for (size_t i = 0; i < 2; i++) {
			AudioQueueBufferRef buffer;
			require(AudioQueueAllocateBuffer(m_queue, 2*8*1024, &buffer), "AudioQueueAllocateBuffer");
			processBuffer(buffer);
		}

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

class TriangleNode : public Node {

	double m_last, m_step;
	bool m_rising;

	public:

	TriangleNode(double step) : m_last(0), m_step(step), m_rising(true) {}

	void operator() (double b[], size_t l) {
		for (size_t i = 0; i < l; ++i, ++b) {
			*b = (m_rising ? (m_last += m_step) : (m_last -= m_step));
			if (*b + m_step > 1) { m_rising = false; }
			else if (*b - m_step < -1) { m_rising = true; }
		}
	}
};

class Mixer : public Node {

	std::vector<Node *> m_inputs;

	public:

	void addInput(Node *node){ m_inputs.push_back(node); }

	void operator() (double b[], size_t l) {
		double *your_buf = static_cast<double *>(calloc(l, sizeof(double)));
		memset(b, 0, sizeof(double) * l);
		for (auto input : m_inputs) {
			(*input)(your_buf, l);
			for (size_t j = 0; j < l; j++) {
				b[j] += your_buf[j] / m_inputs.size();
			}
		}
		free(your_buf);
	}
};

int main () {

	Output output;

	TriangleNode a(1000 / (output.m_sample_rate / 4));
	TriangleNode b(500 / (output.m_sample_rate / 4));

	Mixer m;

	m.addInput(&a);
	m.addInput(&b);

	output.m_input = &m;
	output.start();

	return 1;
}
