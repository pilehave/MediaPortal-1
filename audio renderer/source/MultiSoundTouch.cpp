
#include "stdafx.h"

#include "MultiSoundTouch.h"

#define DEFINE_STREAM_FUNC(funcname, paramtype, paramname) \
  void CMultiSoundTouch::funcname(paramtype paramname) \
  { \
    if (m_Streams) \
    { \
      for(int i=0; i<m_nStreamCount; i++) \
        m_Streams[i].processor->funcname(paramname); \
    } \
  }

// move these in a separate common header 
#define SAFE_DELETE(p)       { if(p) { delete (p);     (p)=NULL; } }
#define SAFE_DELETE_ARRAY(p) { if(p) { delete[] (p);   (p)=NULL; } }

typedef void (CMultiSoundTouch::*DeInterleaveFunc)(const soundtouch::SAMPLETYPE *inBuffer, short *outBuffer, uint count);
typedef void (CMultiSoundTouch::*InterleaveFunc)(const soundtouch::SAMPLETYPE *inBuffer, short *outBuffer, uint count);


CMultiSoundTouch::CMultiSoundTouch() 
: m_nChannels(0)
, m_Streams(NULL)
, m_nStreamCount(0)
{
  // nothing else to do
}

CMultiSoundTouch::~CMultiSoundTouch()
{
  setChannels(0);
}


DEFINE_STREAM_FUNC(setRate,float, newRate)
DEFINE_STREAM_FUNC(setTempo, float, newTempo)
DEFINE_STREAM_FUNC(setRateChange, float, newRate)
DEFINE_STREAM_FUNC(setTempoChange, float, newTempo)
DEFINE_STREAM_FUNC(setPitchOctaves, float, newPitch)
DEFINE_STREAM_FUNC(setPitchSemiTones, int, newPitch)
DEFINE_STREAM_FUNC(setPitchSemiTones, float, newPitch)
DEFINE_STREAM_FUNC(setSampleRate, uint, srate)
DEFINE_STREAM_FUNC(flush,,)
DEFINE_STREAM_FUNC(clear,,)

uint CMultiSoundTouch::numUnprocessedSamples() const
{
  uint maxSamples = 0;
  for (int i=0 ; i<m_nStreamCount; i++)
  {
    uint samples = m_Streams[i].processor->numUnprocessedSamples();
    if (maxSamples == 0 || maxSamples < samples)
      maxSamples = samples;
  }
  return maxSamples;
}

/// Returns number of samples currently available.
uint CMultiSoundTouch::numSamples() const
{
  uint minSamples = 0;
  for (int i=0 ; i<m_nStreamCount; i++)
  {
    uint samples = m_Streams[i].processor->numSamples();
    if (minSamples == 0 || minSamples > samples)
      minSamples = samples;
  }
  return minSamples;
}

/// Returns nonzero if there aren't any samples available for outputting.
int CMultiSoundTouch::isEmpty() const
{
  for (int i=0 ; i<m_nStreamCount; i++)
  {
    if (m_Streams[i].processor->isEmpty())
      return true;
  }
  return false;
}


void CMultiSoundTouch::setChannels(int channels)
{
  int newStreamCount = channels/2 + (channels % 2);
  StreamProcessor *newStreams = NULL;
  m_nChannels = channels;
  if (channels > 0)
  {
    // create new streams
    newStreams = new StreamProcessor[newStreamCount];
    for(int i=0; i<newStreamCount; i++)
    {
      newStreams[i].channels = (channels>1)? 2:1;
      newStreams[i].processor = new soundtouch::SoundTouch();
      newStreams[i].processor->setChannels(newStreams[i].channels);
      channels -= newStreams[i].channels;
    }
  }
  // delete old ones
  StreamProcessor *oldStreams = m_Streams;
  int oldStreamCount = m_nStreamCount;
  {
    // this block might need to be protected by a lock
    m_Streams = newStreams;
    m_nStreamCount = newStreamCount;
  }
  for(int i=0; i<oldStreamCount; i++)
  {
    SAFE_DELETE(oldStreams[i].processor);
  }
  SAFE_DELETE_ARRAY(oldStreams);
}

bool CMultiSoundTouch::putSamples(const short *inBuffer, long inSamples)
{
  if(m_Streams == NULL)
    return false;

  if(m_nChannels <= 2)
  {
    m_Streams[0].processor->putSamples(inBuffer, inSamples);
  }
  else
  {
    for(int i=0; i<m_nStreamCount; i++)
    {
      StreamProcessor *stream = &m_Streams[i];
      long inSamplesRemaining = inSamples;
      const short *streamInBuf = inBuffer;

      DeInterleaveFunc deInterleave = (stream->channels == 1? &CMultiSoundTouch::MonoDeInterleave : &CMultiSoundTouch::StereoDeInterleave);
      
      // input samples
      while(inSamplesRemaining)
      {
        uint batchLen = (inSamplesRemaining < SAMPLE_LEN? inSamplesRemaining : SAMPLE_LEN);
        (this->*deInterleave)(streamInBuf, m_temp, batchLen);
        stream->processor->putSamples(m_temp, batchLen);
        inSamplesRemaining -= batchLen;
        streamInBuf += batchLen * m_nChannels;
      }
      inBuffer += stream->channels;
    }
  }
  return true;
}

uint CMultiSoundTouch::receiveSamples(short *outBuffer, uint maxSamples)
{
  if(m_Streams == NULL)
    return 0;

  if(m_nChannels <= 2)
  {
    return m_Streams[0].processor->receiveSamples(outBuffer, maxSamples);
  }
  else
  {
    uint outSamples = 0;
    for(int i=0; i<m_nStreamCount; i++)
    {
      StreamProcessor *stream = &m_Streams[i];

      InterleaveFunc interleave = (stream->channels == 1? &CMultiSoundTouch::MonoInterleave : &CMultiSoundTouch::StereoInterleave);
      
      // output samples
      short *streamOutBuf = outBuffer;
      long outSampleSpace = maxSamples;
      while(outSampleSpace > 0)
      {
        uint batchLen = (outSampleSpace < SAMPLE_LEN? outSampleSpace : SAMPLE_LEN);
        batchLen = stream->processor->receiveSamples(m_temp, batchLen);
        (this->*interleave)(m_temp, streamOutBuf, batchLen);
        if(batchLen == 0)
          break;
        outSampleSpace -= batchLen;
        streamOutBuf += batchLen * m_nChannels;
      }
      if(outSamples < maxSamples-outSampleSpace)
        outSamples = maxSamples-outSampleSpace;
      
      outBuffer += stream->channels;
    }
    return outSamples;
  }
}

// Internal functions to separate/merge streams out of/into sample buffers (from IMediaSample)
// All buffers are arrays of Int16.
// The sample buffers are assumed to be SampleCount*Channels words long (1 word = 2 bytes)
// The stereo stream buffers are assumed to be 2*SampleCount words long (1 word = sizeof(soundtouch::SAMPLETYPE))
// The mono stream buffers are assumed to be SampleCount words long (1 word = sizeof(soundtouch::SAMPLETYPE))
bool CMultiSoundTouch::ProcessSamples(const short *inBuffer, long inSamples, short *outBuffer, long *outSamples, long maxOutSamples)
{
  if (!putSamples(inBuffer, inSamples))
    return false;

  *outSamples = receiveSamples(outBuffer, maxOutSamples);
  return true;
}



void CMultiSoundTouch::StereoDeInterleave(const short *inBuffer, soundtouch::SAMPLETYPE *outBuffer, uint count)
{
  while(count--)
  {
    *outBuffer++ = *inBuffer;
    *outBuffer++ = *(inBuffer+1);
    inBuffer += m_nChannels;
  }
}

void CMultiSoundTouch::StereoInterleave(const soundtouch::SAMPLETYPE *inBuffer, short *outBuffer, uint count)
{
  while(count--)
  {
    *outBuffer = *inBuffer;
    *(outBuffer+1) = *inBuffer++;
    outBuffer += m_nChannels;
  }
}

void CMultiSoundTouch::MonoDeInterleave(const short *inBuffer, soundtouch::SAMPLETYPE *outBuffer, uint count)
{
  while(count--)
  {
    *outBuffer++ = *inBuffer;
    inBuffer += m_nChannels;
  }
}

void CMultiSoundTouch::MonoInterleave(const soundtouch::SAMPLETYPE *inBuffer, short *outBuffer, uint count)
{
  while(count--)
  {
    *outBuffer = *inBuffer;
    outBuffer += m_nChannels;
  }
}

