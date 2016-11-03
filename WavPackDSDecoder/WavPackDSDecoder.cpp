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

#include <streams.h>
#include <initguid.h>
#include <mmreg.h>

#include <ks.h>
#include <ksmedia.h>

#include "..\wavpack\wavpack.h"
#include "..\wavpacklib\wavpack_common.h"
#include "..\wavpacklib\wavpack_frame.h"
#include "..\wavpacklib\wavpack_buffer_decoder.h"

#include "IWavPackDSDecoder.h"
#include "..\WavPackDS_GUID.h"
#include "WavPackDSDecoder.h"

#include "WavPackDSDecoderAboutProp.h"
#include "WavPackDSDecoderInfoProp.h"

// ----------------------------------------------------------------------------

#ifdef _DEBUG
#include <stdio.h>
#include <tchar.h>
void DebugLog(const char *pFormat,...) {
    char szInfo[2000];
    
    // Format the variable length parameter list
    va_list va;
    va_start(va, pFormat);
    
    _vstprintf(szInfo, pFormat, va);
    lstrcat(szInfo, TEXT("\r\n"));
    OutputDebugString(szInfo);
    
    va_end(va);
}
#else
#define DebugLog
#endif

// ----------------------------------------------------------------------------
//  Registration setup stuff

#define WAVPACK_DECODER_NAME L"WavPack Audio Decoder"

static const WCHAR g_wszWavPackDSName[]  = WAVPACK_DECODER_NAME;

AMOVIESETUP_MEDIATYPE sudInputType[] =
{
    { &MEDIATYPE_Audio, &MEDIASUBTYPE_WAVPACK },
	{ &MEDIATYPE_Audio, &MEDIASUBTYPE_WavpackHybrid }
};

AMOVIESETUP_MEDIATYPE sudOutputType[] =
{
    { &MEDIATYPE_Audio, &MEDIASUBTYPE_PCM }
};

AMOVIESETUP_PIN sudPins[] =
{
    { L"Input",
        FALSE,                          // bRendered
        FALSE,                          // bOutput
        FALSE,                          // bZero
        FALSE,                          // bMany
        &CLSID_NULL,                    // clsConnectsToFilter
        NULL,                           // ConnectsToPin
        NUMELMS(sudInputType),          // Number of media types
        sudInputType
    },
    { L"Output",
        FALSE,                          // bRendered
        TRUE,                           // bOutput
        FALSE,                          // bZero
        FALSE,                          // bMany
        &CLSID_NULL,                    // clsConnectsToFilter
        NULL,                           // ConnectsToPin
        NUMELMS(sudOutputType),         // Number of media types
        sudOutputType
    }
};

AMOVIESETUP_FILTER sudWavPackDecoder =
{
    &CLSID_WavPackDSDecoder,
    g_wszWavPackDSName,
    MERIT_NORMAL,
    NUMELMS(sudPins),
    sudPins
};

// ----------------------------------------------------------------------------
// COM Global table of objects in this dll

CFactoryTemplate g_Templates[] = 
{
  {
    g_wszWavPackDSName,
    &CLSID_WavPackDSDecoder,
    CWavPackDSDecoder::CreateInstance,
    NULL,
    &sudWavPackDecoder
  }
  ,
  {
    L"WavPack Audio Decoder Info",
    &CLSID_WavPackDSDecoderInfoProp,
    CWavPackDSDecoderInfoProp::CreateInstance,
    NULL,
    NULL
  }
  ,
  {
    L"WavPack Audio Decoder About",
    &CLSID_WavPackDSDecoderAboutProp,
    CWavPackDSDecoderAboutProp::CreateInstance,
    NULL,
    NULL
  }
};

// Count of objects listed in g_cTemplates
int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

// ----------------------------------------------------------------------------

STDAPI DllRegisterServer()
{
    return AMovieDllRegisterServer2(TRUE);
}

// ----------------------------------------------------------------------------

STDAPI DllUnregisterServer()
{
    return AMovieDllRegisterServer2(FALSE);
}

// ----------------------------------------------------------------------------

// The streams.h DLL entrypoint.
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

// The entrypoint required by the MSVC runtimes. This is used instead
// of DllEntryPoint directly to ensure global C++ classes get initialised.
BOOL WINAPI DllMain(HANDLE hDllHandle, DWORD dwReason, LPVOID lpreserved)
{
    return DllEntryPoint(reinterpret_cast<HINSTANCE>(hDllHandle), dwReason, lpreserved);
}

// ----------------------------------------------------------------------------

CUnknown *WINAPI CWavPackDSDecoder::CreateInstance(LPUNKNOWN punk, HRESULT *phr)
{
    CWavPackDSDecoder *pNewObject = new CWavPackDSDecoder(punk, phr);
    if (!pNewObject)
    {
        *phr = E_OUTOFMEMORY;
    }
    return pNewObject;
}

// ----------------------------------------------------------------------------

CWavPackDSDecoder::CWavPackDSDecoder(LPUNKNOWN lpunk, HRESULT *phr) :
    CTransformFilter(NAME("CWavPackDSDecoder"), lpunk, CLSID_WavPackDSDecoder)
    ,m_rtFrameStart(0)
    ,m_TotalSamples(0)
    ,m_SamplesPerBuffer(0)
    ,m_HybridMode(FALSE)
    ,m_MainFrameData(NULL)
    ,m_MainBlockDiscontinuity(TRUE)
    ,m_DecodedFrames(0)
    ,m_CrcError(0)
    ,m_DecodingMode(0)
    ,m_HeadersPresent(0)
    ,m_32bitFloatData(0)

{

}

// ----------------------------------------------------------------------------

CWavPackDSDecoder::~CWavPackDSDecoder()
{
}

// ----------------------------------------------------------------------------

STDMETHODIMP CWavPackDSDecoder::NonDelegatingQueryInterface(REFIID riid, void **ppv)
{
    if(riid == IID_IWavPackDSDecoder)
        return GetInterface((IWavPackDSDecoder *)this, ppv);
    else if (riid == IID_ISpecifyPropertyPages)
        return GetInterface((ISpecifyPropertyPages *)this, ppv);
    else
        return CTransformFilter::NonDelegatingQueryInterface(riid, ppv);
}

// ----------------------------------------------------------------------------

HRESULT CWavPackDSDecoder::CheckInputType(const CMediaType *mtIn)
{
    if ((*mtIn->Type() != MEDIATYPE_Audio) ||
        ((*mtIn->Subtype() != MEDIASUBTYPE_WAVPACK) &&
         (*mtIn->Subtype() != MEDIASUBTYPE_WavpackHybrid)) ||
        (*mtIn->FormatType() != FORMAT_WaveFormatEx))
    {
        return VFW_E_TYPE_NOT_ACCEPTED;
    }

    WAVEFORMATEX *pwfxin = (WAVEFORMATEX*)mtIn->Format();    

    // Currently we allow 3 values for cbSize:
    //   0 indicates we are dealing with WavPack data with headers (perhaps from LAV splitter)
    //   2 indicates Matroska-style WavPack data with stripped headers
    //   8 also indicates stripped headers, but with flags and channel mask

    if (pwfxin->cbSize != 0 &&
        pwfxin->cbSize != sizeof (m_PrivateData.version) &&
        pwfxin->cbSize != sizeof (m_PrivateData))
            return VFW_E_TYPE_NOT_ACCEPTED;    

    if((pwfxin->wBitsPerSample == 8) ||
        (pwfxin->wBitsPerSample == 16) ||
        (pwfxin->wBitsPerSample == 24) ||
        (pwfxin->wBitsPerSample == 32))
    {
        return S_OK;
    }

    return VFW_E_TYPE_NOT_ACCEPTED;    
}

// ----------------------------------------------------------------------------

HRESULT CWavPackDSDecoder::GetMediaType(int iPosition, CMediaType *mtOut)
{
    if (!m_pInput->IsConnected())
    {
        return E_UNEXPECTED;
    }
    
    if (iPosition < 0)
    {
        return E_INVALIDARG;
    }
    
    if (iPosition > 1)
    {
        return VFW_S_NO_MORE_ITEMS;
    }

    WAVEFORMATEX *pwfxin = (WAVEFORMATEX*)m_pInput->CurrentMediaType().Format();
    
    // Some drivers don't like WAVEFORMATEXTENSIBLE when channels are <= 2 so
    // we fall back to a classic WAVEFORMATEX struct in this case 
    
    WAVEFORMATEXTENSIBLE wfex;
    ZeroMemory(&wfex, sizeof(WAVEFORMATEXTENSIBLE));

    bool bUseWavExtensible = (pwfxin->nChannels > 2) ||
        (pwfxin->wBitsPerSample == 24) ||
        (pwfxin->wBitsPerSample == 32);
    
    // If 32-bit data, we default to outputting IEEE float data unless we specifically know that
    // the data is 32-bit integers (which is less common). If we are wrong, and the data really
    // is 32-bit integers, then we'll know this during decode and convert to float then.

    if(pwfxin->wBitsPerSample == 32 &&
        (pwfxin->cbSize < sizeof (m_PrivateData) || !(m_PrivateData.flags & WPFLAGS_INT32DATA)))
    {
        m_32bitFloatData = TRUE;
        wfex.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
        wfex.Format.wFormatTag = bUseWavExtensible ?
            WAVE_FORMAT_EXTENSIBLE :
            WAVE_FORMAT_IEEE_FLOAT;
    }
    else
    {
        wfex.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
        wfex.Format.wFormatTag = bUseWavExtensible ?
            WAVE_FORMAT_EXTENSIBLE :
            WAVE_FORMAT_PCM;
    }

    wfex.Format.cbSize = bUseWavExtensible ? sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX) : 0;
    wfex.Format.nChannels = pwfxin->nChannels;
    wfex.Format.nSamplesPerSec = pwfxin->nSamplesPerSec;
    wfex.Format.wBitsPerSample = pwfxin->wBitsPerSample;
    wfex.Format.nBlockAlign = (unsigned short)((wfex.Format.nChannels * wfex.Format.wBitsPerSample) / 8);
    wfex.Format.nAvgBytesPerSec = wfex.Format.nSamplesPerSec * wfex.Format.nBlockAlign;

    if (pwfxin->nChannels < NUM_DEFAULT_CHANNEL_MASKS)
        wfex.dwChannelMask = DefaultChannelMasks [pwfxin->nChannels];
    else
        wfex.dwChannelMask = KSAUDIO_SPEAKER_DIRECTOUT; // XXX : or SPEAKER_ALL ??

    if(pwfxin->cbSize >= sizeof (m_PrivateData))
        wfex.dwChannelMask = m_PrivateData.channel_mask;

    wfex.Samples.wValidBitsPerSample = wfex.Format.wBitsPerSample;  
    
    mtOut->SetType(&MEDIATYPE_Audio);
    //if(pwfxin->wBitsPerSample == 32)
    //{
    //    mtOut->SetSubtype(&MEDIASUBTYPE_IEEE_FLOAT);
    //}
    //else
    {
        mtOut->SetSubtype(&MEDIASUBTYPE_PCM);
    }
    
    mtOut->SetFormatType(&FORMAT_WaveFormatEx);
    mtOut->SetFormat( (BYTE*) &wfex, sizeof(WAVEFORMATEX) + wfex.Format.cbSize);
    mtOut->SetTemporalCompression(FALSE);
    
    return S_OK;
}

// ----------------------------------------------------------------------------

HRESULT CWavPackDSDecoder::CheckTransform(const CMediaType *mtIn, const CMediaType *mtOut)
{
    if ((*mtOut->Type() != MEDIATYPE_Audio) ||
        (*mtOut->Subtype() != MEDIASUBTYPE_PCM) ||
        (*mtOut->FormatType() != FORMAT_WaveFormatEx))
    {
        return VFW_E_TYPE_NOT_ACCEPTED;
    }

    
    WAVEFORMATEX *pwfxin = (WAVEFORMATEX *)mtIn->Format();
    WAVEFORMATEX *pwfxout = (WAVEFORMATEX *)mtOut->Format();
    if ((pwfxin->nSamplesPerSec != pwfxout->nSamplesPerSec) ||
        (pwfxin->nChannels != pwfxout->nChannels) ||
        (pwfxin->wBitsPerSample != pwfxout->wBitsPerSample))
    {
        return VFW_E_TYPE_NOT_ACCEPTED;
    }

    return CheckInputType(mtIn);
}

// ----------------------------------------------------------------------------

HRESULT CWavPackDSDecoder::DecideBufferSize(IMemAllocator *pAllocator, ALLOCATOR_PROPERTIES *pProperties)
{   
    if (!m_pInput->IsConnected())
    {
        return E_UNEXPECTED;
    }

    WAVEFORMATEX *pwfxin = (WAVEFORMATEX*)m_pInput->CurrentMediaType().Format();

    // Place for ~100ms
    long OutBuffSize = 0;
    m_SamplesPerBuffer = (pwfxin->nSamplesPerSec / 10);
    // required memory for output "buffer" is 4 * samples * num_channels bytes
    OutBuffSize = 4 * m_SamplesPerBuffer * pwfxin->nChannels;
    //OutBuffSize &= 0xFFFFFFF8;
    
    pProperties->cBuffers = 2;
    pProperties->cbBuffer = OutBuffSize;

    ALLOCATOR_PROPERTIES Actual;
    HRESULT hr = pAllocator->SetProperties(pProperties, &Actual);
    if(FAILED(hr))
    {
        return hr;
    }

    if (Actual.cbBuffer < pProperties->cbBuffer ||
        Actual.cBuffers < pProperties->cBuffers)
    {
        return E_INVALIDARG;
    }
    return S_OK;
}

// ----------------------------------------------------------------------------

HRESULT CWavPackDSDecoder::SetMediaType(PIN_DIRECTION direction, const CMediaType *pmt)
{
    HRESULT hr = CTransformFilter::SetMediaType(direction, pmt);

    if(SUCCEEDED(hr) && (direction == PINDIR_INPUT))
    {        
        WAVEFORMATEX* pwfx = (WAVEFORMATEX*)m_pInput->CurrentMediaType().Format();      
        if(pwfx->cbSize > 0)
        {
            memcpy(&m_PrivateData,
                (char*)(pwfx+1),
                min(sizeof(wavpack_codec_private_data),pwfx->cbSize));
        }
        else
            m_HeadersPresent = TRUE;    // no private data means headers must be present

        m_SamplesPerSec = pwfx->nSamplesPerSec;
        m_Channels = pwfx->nChannels;
        m_BitsPerSample = pwfx->wBitsPerSample;

        m_HybridMode = (*m_pInput->CurrentMediaType().Subtype() == MEDIASUBTYPE_WavpackHybrid);

        
        if(m_HybridMode == TRUE)
        {
            m_DecodingMode = DECODING_MODE_HYBRID;
        }
        else
        {
            m_DecodingMode = DECODING_MODE_LOSSLESS_OR_LOSSY;
        }
    }

    return hr;
}

// ----------------------------------------------------------------------------

HRESULT CWavPackDSDecoder::StartStreaming()
{
    HRESULT hr = CTransformFilter::StartStreaming();

    m_DecodedFrames = 0;
    m_CrcError = 0;

    return hr;
}

// ----------------------------------------------------------------------------

HRESULT CWavPackDSDecoder::StopStreaming()
{
    HRESULT hr = CTransformFilter::StopStreaming();

    if (m_MainFrameData) {
        delete [] m_MainFrameData;
        m_MainFrameData = NULL;
    }

    return hr;
}

// ----------------------------------------------------------------------------

HRESULT CWavPackDSDecoder::Receive(IMediaSample *pSample)
{
    //  Check for other streams and pass them on 
    AM_SAMPLE2_PROPERTIES * const pProps = m_pInput->SampleProps(); 
    if ((pProps->dwStreamId != AM_STREAM_MEDIA) &&
        (pProps->dwStreamId != AM_STREAM_BLOCK_ADDITIONNAL))
    {
        return m_pOutput->Deliver(pSample);
    }
    
    ASSERT(pSample);
    // If no output to deliver to then no point sending us data 
    ASSERT(m_pOutput != NULL);

    HRESULT hr = S_OK;
    BYTE *pSrc, *pDst;
    DWORD SrcLength = pSample->GetActualDataLength();
    DebugLog("block size %d ", SrcLength);
    hr = pSample->GetPointer(&pSrc);
    if(FAILED(hr))
        return hr;
    
     // Check for minimal block size
    if(SrcLength < (3 * sizeof(uint32_t)))
    {
        return S_OK;
    }

    WAVEFORMATEX* pwfx = (WAVEFORMATEX*)m_pInput->CurrentMediaType().Format();
    BOOL bSeveralBlocks = (pwfx->nChannels > 2);

    if (pwfx->cbSize >= sizeof (m_PrivateData) && (m_PrivateData.flags & WPFLAGS_SEVERALBLKS))
        bSeveralBlocks = TRUE;
 
    if(pProps->dwStreamId == AM_STREAM_MEDIA)
    {
        REFERENCE_TIME rtStop;
        if(pSample->IsSyncPoint() == S_OK)
        {
            pSample->GetTime(&m_rtFrameStart, &rtStop);
            m_TotalSamples = 0;
        }

        m_MainBlockDiscontinuity = (pSample->IsDiscontinuity() == S_OK);

        if(m_HybridMode == TRUE)
        {
            // save the main data and stop here to wait for correction data
            if (m_MainFrameData) delete [] m_MainFrameData;
            m_MainFrameData = new char [m_MainFrameSize = SrcLength];

            if (!m_MainFrameData)
                return S_FALSE;

            memcpy (m_MainFrameData, pSrc, SrcLength);
            return S_OK;
        }
    }
    
    // Decoding has been significantly simplified by using the new "raw" decode option
    // in WavPack 5.0. We now simply pass the raw data (with or without headers) to the
    // library and all the samples in the frame are available from WavpackUnpackSamples()
    // which will stop decoding once the frame is exhausted.

	WavpackContext *wpc;
	char error [80];

    if (m_HybridMode == TRUE) {
        if (!(wpc = WavpackOpenRawDecoder (m_MainFrameData, m_MainFrameSize,
            pSrc, SrcLength, m_PrivateData.version, error, OPEN_NORMALIZE, 0)))
        {
            // Something is wrong
            DebugLog("WavpackOpenRawDecoder(1) failed, %s", error);
            return S_FALSE;
        }
    }
    else { 
        if (!(wpc = WavpackOpenRawDecoder (pSrc, SrcLength, NULL, 0, m_PrivateData.version, error, OPEN_NORMALIZE, 0)))
        {
            // Something is wrong
            DebugLog("WavpackOpenRawDecoder(2) failed, %s", error);
            return S_FALSE;
        }
    }

    // We can precise the decoding mode now
    if(m_HybridMode == FALSE)
    {
        if(WavpackGetMode (wpc) & MODE_HYBRID)
        {
            m_DecodingMode = DECODING_MODE_LOSSY;
        }
        else
        {
            m_DecodingMode = DECODING_MODE_LOSSLESS;
        }
    }

    while(1)
    {
        // Set up the output sample
        IMediaSample *pOutSample;
        hr = InitializeOutputSample(pSample, &pOutSample);
        if(FAILED(hr))
        {
            break;
        }
    
        DWORD DstLength = pOutSample->GetSize();
        hr = pOutSample->GetPointer(&pDst);
        if(FAILED(hr))
        {
            pOutSample->Release();
            break;
        }

        DstLength &= 0xFFFFFFF8;
    
        long samples = WavpackUnpackSamples (wpc,(int32_t *)pDst, m_SamplesPerBuffer);
        if(samples)
        {
            wavpack_buffer_format_samples(wpc,
                (uchar *) pDst,
                (long*) pDst,
                samples,
                m_32bitFloatData);
            
            DstLength = samples *
                WavpackGetBytesPerSample(wpc) *
                WavpackGetNumChannels (wpc);

            pOutSample->SetActualDataLength(DstLength);
            
            REFERENCE_TIME rtStart, rtStop;
            rtStart = m_rtFrameStart + (REFERENCE_TIME)(((double)m_TotalSamples / WavpackGetSampleRate(wpc)) * 10000000);
            m_TotalSamples += samples;
            rtStop = m_rtFrameStart + (REFERENCE_TIME)(((double)m_TotalSamples / WavpackGetSampleRate(wpc)) * 10000000);

            if(rtStart < 0 && rtStop < 0)
            {
                // No need to deliver this sample it will be skipped
                pOutSample->Release();
                continue;
            }
            pOutSample->SetTime(&rtStart, &rtStop);
            pOutSample->SetSyncPoint(TRUE);
            pOutSample->SetDiscontinuity(m_MainBlockDiscontinuity);
            if(m_MainBlockDiscontinuity == TRUE)
            {
                m_MainBlockDiscontinuity = FALSE;
            }

            hr = m_pOutput->Deliver(pOutSample);
            if(FAILED(hr))
            {
                pOutSample->Release();
                break;
            }
            pOutSample->Release();
        }
        else
        {
            pOutSample->Release();
            break;
        }
    }
    
    m_DecodedFrames++;
    m_CrcError = WavpackGetNumErrors(wpc);

	WavpackCloseFile (wpc);     // close the raw context, not actually the file we're streaming

	if (m_MainFrameData) {
		delete [] m_MainFrameData;
		m_MainFrameData = NULL;
	}
    
    return S_OK;
}

// ============================================================================

// ISpecifyPropertyPages

STDMETHODIMP CWavPackDSDecoder::GetPages(CAUUID *pPages)
{
    pPages->cElems = 2;
    pPages->pElems = (GUID *)CoTaskMemAlloc(pPages->cElems * sizeof(GUID));
    if (!pPages->pElems)
        return E_OUTOFMEMORY;
    
    pPages->pElems[0] = CLSID_WavPackDSDecoderInfoProp;
    pPages->pElems[1] = CLSID_WavPackDSDecoderAboutProp;
    
    return S_OK;
}

// ============================================================================

STDMETHODIMP CWavPackDSDecoder::get_SampleRate(int* sample_rate)
{
    CheckPointer(sample_rate, E_POINTER);
    CAutoLock lock(&m_WPDSLock);
    *sample_rate = m_SamplesPerSec;
    return S_OK;
}

STDMETHODIMP CWavPackDSDecoder::get_Channels(int *channels)
{
    CheckPointer(channels, E_POINTER);
    CAutoLock lock(&m_WPDSLock);
    *channels = m_Channels;
    return S_OK;
}

STDMETHODIMP CWavPackDSDecoder::get_BitsPerSample(int *bits_per_sample)
{
    CheckPointer(bits_per_sample, E_POINTER);
    CAutoLock lock(&m_WPDSLock);
    *bits_per_sample = m_BitsPerSample;
    return S_OK;
}

STDMETHODIMP CWavPackDSDecoder::get_FramesDecoded(int *frames_decoded)
{
    CheckPointer(frames_decoded,E_POINTER);
    CAutoLock lock(&m_WPDSLock);
    *frames_decoded = m_DecodedFrames;
    return S_OK;
}

STDMETHODIMP CWavPackDSDecoder::get_CrcErrors(int *crc_errors)
{
    CheckPointer(crc_errors, E_POINTER);
    CAutoLock lock(&m_WPDSLock);
    *crc_errors = m_CrcError;
    return S_OK;
}

STDMETHODIMP CWavPackDSDecoder::get_DecodingMode(int *decoding_mode)
{
    CheckPointer(decoding_mode, E_POINTER);
    CAutoLock lock(&m_WPDSLock);
    *decoding_mode = m_DecodingMode;
    return S_OK;
}

// ============================================================================
