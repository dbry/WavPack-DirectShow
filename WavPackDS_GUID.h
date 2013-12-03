// ----------------------------------------------------------------------------
// WavPack DirectShow Audio Encoder/Decoder - Splitter
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
#ifndef _WAVPACKDS_GUID_H_
#define _WAVPACKDS_GUID_H_
// ----------------------------------------------------------------------------

// {4B420C26-B393-48b3-8A84-BC60827689E8}
static const GUID CLSID_WavPackDSDecoder = 
    { 0x4b420c26, 0xb393, 0x48b3, { 0x8a, 0x84, 0xbc, 0x60, 0x82, 0x76, 0x89, 0xe8 } };

// {9DD46B28-8A7F-409b-A365-C5CE3946A0C7}
static const GUID CLSID_WavPackDSDecoderAboutProp = 
{ 0x9dd46b28, 0x8a7f, 0x409b, { 0xa3, 0x65, 0xc5, 0xce, 0x39, 0x46, 0xa0, 0xc7 } };

// {0D69FA1E-E90D-4baf-B39E-6EA5EE2A7B49}
static const GUID CLSID_WavPackDSDecoderInfoProp = 
{ 0xd69fa1e, 0xe90d, 0x4baf, { 0xb3, 0x9e, 0x6e, 0xa5, 0xee, 0x2a, 0x7b, 0x49 } };

// ----------------------------------------------------------------------------

// {D8CF6A42-3E09-4922-A452-21DFF10BEEBA}
static const GUID CLSID_WavPackDSSplitter = 
    { 0xd8cf6a42, 0x3e09, 0x4922, { 0xa4, 0x52, 0x21, 0xdf, 0xf1, 0xb, 0xee, 0xba } };

// ----------------------------------------------------------------------------

#define WAVE_FORMAT_WAVPACK 0x5756

// {00005756-0000-0010-8000-00AA00389B71}
static const GUID MEDIASUBTYPE_WAVPACK =
    { WAVE_FORMAT_WAVPACK, 0x000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 };


// {B8CBBAD8-AA1A-4cea-B95E-730041A55EF0}
static const GUID MEDIASUBTYPE_WavpackHybrid =
	{ 0xb8cbbad8, 0xaa1a, 0x4cea, 0xb9, 0x5e, 0x73, 0x0, 0x41, 0xa5, 0x5e, 0xf0};


// {FF5A7C6F-6646-42ab-A1FC-0E6739D56E75}
static const GUID MEDIASUBTYPE_WAVPACK_Stream = 
    { 0xff5a7c6f, 0x6646, 0x42ab, { 0xa1, 0xfc, 0xe, 0x67, 0x39, 0xd5, 0x6e, 0x75 } };

// {F25219CA-AAEA-44e5-92C8-73D305219C7D}
static const GUID MEDIASUBTYPE_WAVPACK_CORRECTION_Stream = 
    { 0xf25219ca, 0xaaea, 0x44e5, { 0x92, 0xc8, 0x73, 0xd3, 0x5, 0x21, 0x9c, 0x7d } };


// ----------------------------------------------------------------------------

// This table provides the default speaker configurations based on number of channels
// up to 5.1 surround. If the WavPack file indicates a different speaker configuration
// than what's here (or there are more than 6 channels) then the splitter will pass the
// actual channel mask in the expanded wavpack_codec_private_data struct below.

static const DWORD DefaultChannelMasks [] = {
    KSAUDIO_SPEAKER_DIRECTOUT,                      // channels = 0 (really a placeholder)
    KSAUDIO_SPEAKER_MONO,                           // channels = 1
    KSAUDIO_SPEAKER_STEREO,                         // channels = 2
    KSAUDIO_SPEAKER_STEREO | SPEAKER_FRONT_CENTER,  // channels = 3
    KSAUDIO_SPEAKER_QUAD,                           // channels = 4
    KSAUDIO_SPEAKER_QUAD | SPEAKER_FRONT_CENTER,    // channels = 5
    KSAUDIO_SPEAKER_5POINT1                         // channels = 6
};

#define NUM_DEFAULT_CHANNEL_MASKS (sizeof (DefaultChannelMasks) / sizeof (DefaultChannelMasks [0]))

// This structure was originally only 2 bytes and contained only the WavPack stream version.
// It has now been expanded to 8 bytes to allow the splitter to communicate non-standard
// channels masks and whether 32-bit audio data is float or int (WavPack supports both).

typedef struct {
	short version, flags;
    DWORD channel_mask;
} wavpack_codec_private_data;

#define WPFLAGS_INT32DATA   0x1

// Flag that identify additionnal block data
// It's correction data in case of WavPack
#define AM_STREAM_BLOCK_ADDITIONNAL 0x80000001

// ----------------------------------------------------------------------------
#endif // _WAVPACKDS_GUID_H_
// ----------------------------------------------------------------------------
