// - - -

#include <AudioToolbox/AudioToolbox.h>
#include <vector>
#include <math.h>

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

class SineNode : public Node {
	double m_state, m_freq, m_sample_rate;

	public:
	SineNode(double freq, double sample_rate) : m_state(0), m_freq(freq), m_sample_rate(sample_rate) {}

	void operator() (double b[], size_t l) {
		for (size_t i = 0; i < l; i++) {
			b[i] = sin(m_state);
			m_state += (m_freq * 2 * M_PI) / m_sample_rate;
			if (m_state > (M_PI * 2)) m_state -= M_PI * 2;
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
				b[j] += your_buf[j];
			}
		}
		free(your_buf);
	}
};

class Gain : public Node {

	Node *m_input;
	double m_gain;

	public:
	Gain(Node *input, double gain) : m_input(input), m_gain(gain) {}

	void operator() (double b[], size_t l) {
		(*m_input)(b, l);
		for (size_t i = 0; i < l; i++) {
			b[i] *= m_gain;
		}
	}
};

int main () {

	Output output;

	double base_freq = 800;

	Mixer m;

	for (size_t i = 1; i < 15; i++) {
		m.addInput(new Gain(
			new SineNode(base_freq * i, output.m_sample_rate),
			1.0 / (i * i * i * i)
		));
	}

	output.m_input = &m;
	output.start();

	return 1;
}
