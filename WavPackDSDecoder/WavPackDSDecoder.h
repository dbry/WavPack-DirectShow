// ----------------------------------------------------------------------------
// WavPack DirectShow Audio Decoder
// ----------------------------------------------------------------------------
// Copyright (C) 2005 Christophe Paris (christophe.paris <at> free.fr)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// aint with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
// Please see the file COPYING in this directory for full copyright
// information.
//-----------------------------------------------------------------------------


class CWavPackDSDecoder :   public CTransformFilter
                            , public ISpecifyPropertyPages
                            , public IWavPackDSDecoder
{

public :
    DECLARE_IUNKNOWN
    static CUnknown *WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr); 

    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void **ppv);

    CWavPackDSDecoder(LPUNKNOWN lpunk, HRESULT *phr);
    virtual ~CWavPackDSDecoder();
 
    // ----- CTransformFilter ----- 
    HRESULT CheckInputType(const CMediaType *mtIn);
    HRESULT CheckTransform(const CMediaType *mtIn, const CMediaType *mtOut);
    HRESULT DecideBufferSize(IMemAllocator *pAllocator, ALLOCATOR_PROPERTIES *pprop);
    HRESULT GetMediaType(int iPosition, CMediaType *pMediaType);
    HRESULT SetMediaType(PIN_DIRECTION direction, const CMediaType *pmt);   
	HRESULT Receive(IMediaSample *pSample);
	
	HRESULT StartStreaming();
	HRESULT StopStreaming();

    // ----- ISpecifyPropertyPages -----
	STDMETHODIMP GetPages(CAUUID *pPages);

    // ----- IWavPackDSDecoder -----
    STDMETHODIMP get_SampleRate(int* sample_rate);
    STDMETHODIMP get_Channels(int *channels);
    STDMETHODIMP get_BitsPerSample(int *bits_per_sample);
    STDMETHODIMP get_FramesDecoded(int *frames_decoded);
    STDMETHODIMP get_CrcErrors(int *crc_errors);
    STDMETHODIMP get_DecodingMode(int *decoding_mode);

private:
	REFERENCE_TIME m_rtFrameStart;
	__int64 m_TotalSamples;
	long m_SamplesPerBuffer;
	wavpack_codec_private_data m_PrivateData;

	BOOL m_MainBlockDiscontinuity;

	BOOL m_HybridMode;

    common_frame_data m_CommonFrameData;
    char* m_MainFrameData;

    int m_SamplesPerSec;
    int m_Channels;
    int m_BitsPerSample;
    int m_DecodedFrames;
    int m_CrcError;
    int m_DecodingMode;
    int m_HeadersPresent;
    int m_32bitFloatData;
    int m_MainFrameSize;

    CCritSec    m_WPDSLock; // serialize access
};
