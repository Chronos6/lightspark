/**************************************************************************
    Lightspark, a free flash player implementation

    Copyright (C) 2009,2010  Alessandro Pignotti (a.pignotti@sssup.it)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**************************************************************************/

#ifndef _DECODER_H
#define _DECODER_H

#include <GL/glew.h>
#include <inttypes.h>
#include "threading.h"
#include "graphics.h"
#include "flv.h"
#ifdef ENABLE_LIBAVCODEC
extern "C"
{
#include <libavcodec/avcodec.h>
}
#else
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 2
#endif

namespace lightspark
{

class VideoDecoder
{
private:
	bool resizeGLBuffers;
protected:
	enum STATUS { PREINIT=0, INIT, VALID};
	STATUS status;
	uint32_t frameWidth;
	uint32_t frameHeight;
	bool setSize(uint32_t w, uint32_t h);
	bool resizeIfNeeded(TextureBuffer& tex);
public:
	VideoDecoder():resizeGLBuffers(false),status(PREINIT),frameWidth(0),frameHeight(0),frameRate(0){}
	virtual ~VideoDecoder(){}
	virtual bool decodeData(uint8_t* data, uint32_t datalen, uint32_t time)=0;
	virtual bool discardFrame()=0;
	virtual void skipUntil(uint32_t time)=0;
	//NOTE: the base implementation returns true if resizing of buffers should be done
	//This should be called in every derived implementation
	virtual bool copyFrameToTexture(TextureBuffer& tex)=0;
	uint32_t getWidth()
	{
		return frameWidth;
	}
	uint32_t getHeight()
	{
		return frameHeight;
	}
	bool isValid() const
	{
		return status==VALID;
	}
	float frameRate;
};

class NullVideoDecoder: public VideoDecoder
{
public:
	bool decodeData(uint8_t* data, uint32_t datalen, uint32_t time)
	{
		return true;
	}
	bool discardFrame()
	{
		return false;
	}
	void skipUntil(uint32_t time)
	{

	}
	bool copyFrameToTexture(TextureBuffer& tex)
	{
		return false;
	}
};

#ifdef ENABLE_LIBAVCODEC
class FFMpegVideoDecoder: public VideoDecoder
{
private:
	class YUVBuffer
	{
	public:
		uint8_t* ch[3];
		uint32_t time;
		YUVBuffer(){ch[0]=NULL;ch[1]=NULL;ch[2]=NULL;}
		~YUVBuffer()
		{
			if(ch[0])
			{
				free(ch[0]);
				free(ch[1]);
				free(ch[2]);
			}
		}
	};
	class YUVBufferGenerator
	{
	private:
		uint32_t bufferSize;
	public:
		YUVBufferGenerator(uint32_t b):bufferSize(b){}
		void init(YUVBuffer& buf) const;
	};
	GLuint videoBuffers[2];
	unsigned int curBuffer;
	AVCodecContext* codecContext;
	BlockingCircularQueue<YUVBuffer,20> buffers;
	Mutex mutex;
	bool initialized;
	AVFrame* frameIn;
	void copyFrameToBuffers(const AVFrame* frameIn, uint32_t time);
	void setSize(uint32_t w, uint32_t h);
	bool fillDataAndCheckValidity();
public:
	FFMpegVideoDecoder(uint8_t* initdata, uint32_t datalen);
	~FFMpegVideoDecoder();
	bool decodeData(uint8_t* data, uint32_t datalen, uint32_t time);
	bool discardFrame();
	void skipUntil(uint32_t time);
	bool copyFrameToTexture(TextureBuffer& tex);
};
#endif

class AudioDecoder
{
protected:
	class FrameSamples
	{
	public:
		int16_t samples[AVCODEC_MAX_AUDIO_FRAME_SIZE/2] __attribute__ ((aligned (16)));
		int16_t* current;
		uint32_t len;
		uint32_t time;
		FrameSamples():current(samples),len(AVCODEC_MAX_AUDIO_FRAME_SIZE),time(0){}
	};
	class FrameSamplesGenerator
	{
	public:
		void init(FrameSamples& f) const {f.len=AVCODEC_MAX_AUDIO_FRAME_SIZE;}
	};
	BlockingCircularQueue<FrameSamples,30> samplesBuffer;
	enum STATUS { PREINIT=0, INIT, VALID};
	STATUS status;
public:
	/**
	  	The AudioDecoder contains audio buffers that must be aligned to 16 bytes, so we redefine the allocator
	*/
	void* operator new(size_t);
	void operator delete(void*);
	AudioDecoder():status(PREINIT),sampleRate(0){}
	virtual ~AudioDecoder(){};
	virtual uint32_t decodeData(uint8_t* data, uint32_t datalen, uint32_t time)=0;
	bool hasDecodedFrames() const
	{
		return !samplesBuffer.isEmpty();
	}
	uint32_t getFrontTime() const;
	uint32_t getBytesPerMSec() const
	{
		return sampleRate*channelCount*2/1000;
	}
	uint32_t copyFrame(int16_t* dest, uint32_t len);
	/**
	  	Skip samples until the given time

		@param time the desired time in millisecond
		@param a fractional time in microseconds
	*/
	void skipUntil(uint32_t time, uint32_t usecs);
	/**
	  	Skip all the samples
	*/
	void skipAll();
	bool discardFrame();
	bool isValid() const
	{
		return status==VALID;
	}
	uint32_t sampleRate;
	uint32_t channelCount;
};

class NullAudioDecoder: public AudioDecoder
{
public:
	NullAudioDecoder();
	uint32_t decodeData(uint8_t* data, uint32_t datalen, uint32_t time)
	{
		return 0;
	}
};

#ifdef ENABLE_LIBAVCODEC
class FFMpegAudioDecoder: public AudioDecoder
{
private:
	AVCodecContext* codecContext;
	bool fillDataAndCheckValidity();
public:
	FFMpegAudioDecoder(FLV_AUDIO_CODEC codec, uint8_t* initdata, uint32_t datalen);
	uint32_t decodeData(uint8_t* data, uint32_t datalen, uint32_t time);
};
#endif

};
#endif
