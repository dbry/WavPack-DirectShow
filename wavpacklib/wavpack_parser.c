// ----------------------------------------------------------------------------
// WavPack lib for Matroska
// ----------------------------------------------------------------------------
// Copyright christophe.paris@free.fr
// Parts by David Bryant http://www.wavpack.com
// Distributed under the BSD Software License
// ----------------------------------------------------------------------------

#include "../wavpack/wavpack_local.h"
#include "wavpack_common.h"
#include "wavpack_frame.h"
#include "wavpack_parser.h"

// ----------------------------------------------------------------------------

int add_block(WavPack_parser* wpp, uint32_t block_data_size);

static uint32_t find_sample(WavPack_parser* wpp,
							uint32_t header_pos,
							uint32_t sample);

static uint32_t find_header (WavpackStreamReader *reader,
							 void *id,
							 uint32_t filepos,
							 WavpackHeader *wphdr);

int32_t get_wp_block(WavPack_parser *wpp,
					 int* is_final_block);


static uint32_t seek_final_index (WavpackStreamReader *reader, void *id);

// ----------------------------------------------------------------------------

///////////////////////////// local table storage ////////////////////////////

const uint32_t sample_rates [] = { 6000, 8000, 9600, 11025, 12000, 16000, 22050,
24000, 32000, 44100, 48000, 64000, 88200, 96000, 192000 };

// ----------------------------------------------------------------------------

static int my_read_metadata_buff (WavpackMetadata *wpmd,
						   uchar **buffptr,
							uchar *blockptr_end)
{
   uchar *buffend = blockptr_end;
   
   if (buffend - *buffptr < 2)
	   return FALSE;
   
   wpmd->id = *(*buffptr)++;
   wpmd->byte_length = *(*buffptr)++ << 1;
   
   if (wpmd->id & ID_LARGE) {
	   wpmd->id &= ~ID_LARGE;
	   
	   if (buffend - *buffptr < 2)
		   return FALSE;
	   
	   wpmd->byte_length += *(*buffptr)++ << 9; 
	   wpmd->byte_length += *(*buffptr)++ << 17;
   }
   
   if (wpmd->id & ID_ODD_SIZE) {
	   wpmd->id &= ~ID_ODD_SIZE;
	   wpmd->byte_length--;
   }
   
   if (wpmd->byte_length) {
	   if (buffend - *buffptr < wpmd->byte_length + (wpmd->byte_length & 1)) {
		   wpmd->data = NULL;
		   return FALSE;
	   }
	   
	   wpmd->data = *buffptr;
	   (*buffptr) += wpmd->byte_length + (wpmd->byte_length & 1);
   }
   else
	   wpmd->data = NULL;
   
   return TRUE;
}

static int my_process_metadata (WavPack_parser *wpp, WavpackMetadata *wpmd)
{	
	switch (wpmd->id) {

	case ID_SAMPLE_RATE:
        if (wpmd->byte_length == 3) {
            unsigned char *byteptr = wpmd->data;
            int sample_rate = (int32_t) *byteptr++;
            sample_rate |= (int32_t) *byteptr++ << 8;
            sample_rate |= (int32_t) *byteptr++ << 16;
            DebugLog("my_process_metadata(): got custom sample rate of %d!", sample_rate);
            wpp->sample_rate = sample_rate;
        }
        return TRUE;

	case ID_CHANNEL_INFO:
        if (wpmd->byte_length >= 1 && wpmd->byte_length <= 6) {
            uint32_t bytecnt = wpmd->byte_length, channel_mask = 0, shift = 0;
            unsigned char *byteptr = wpmd->data;

            if (bytecnt == 6) {
                byteptr += 3;
                channel_mask = (int32_t) *byteptr++;
                channel_mask |= (int32_t) *byteptr++ << 8;
                channel_mask |= (int32_t) *byteptr++ << 16;
            }
            else
                while (--bytecnt) {
                    channel_mask |= (uint32_t) *++byteptr << shift;
                    shift += 8;
                }
           
            DebugLog("my_process_metadata(): got channel mask of %03x!", channel_mask);
            wpp->channel_mask = channel_mask;
        }
        return TRUE;

	default:
		return TRUE;
	}
}

static void my_scan_metadata (WavPack_parser *wpp, uchar* blockbuff, uint32_t len)
{
	WavpackMetadata wpmd;
	uchar *blockptr_start = blockbuff;
	uchar *blockptr_end = blockbuff + len;

	while (my_read_metadata_buff (&wpmd, &blockptr_start, blockptr_end))
	{
		if (!my_process_metadata (wpp, &wpmd))
		{
			break;
		}
	}
}

// ----------------------------------------------------------------------------

WavPack_parser* wavpack_parser_new(WavpackStreamReader* io, int is_correction)
{
	uint32_t bcount = 0;
	int is_final_block = FALSE;
	int striped_header_len = 0;
	WavPack_parser* wpp = wp_alloc(sizeof(WavPack_parser));
	if(!wpp)
	{
		return NULL;
	}

	wpp->io = io;
	wpp->is_correction = is_correction;
	
	// TODO :we could determinate if it's a correction file by parsing first block metadata

	wpp->fb = frame_buffer_new();
	if(!wpp->fb)
	{
		wavpack_parser_free(wpp);
		return NULL;
	}

	// Read the first frame
	do {
		// Get next WavPack block info
		bcount = get_wp_block(wpp, &is_final_block);
		if(bcount == -1)
		{
			break;
		}

		if(wpp->fb->nb_block == 0)
		{
			// Store the first header
			wpp->first_wphdr = wpp->wphdr;
			wpp->several_blocks = !is_final_block;
			// Assume those data never change
			wpp->bits_per_sample = wpp->block_bits_per_sample;
			wpp->sample_rate = wpp->block_sample_rate;

			// Make sure we have the total number of samples
			if(wpp->first_wphdr.total_samples == (uint32_t)-1)
			{
				// Seek at the end of the file to guess total_samples
				uint32_t curr_pos = wpp->io->get_pos(wpp->io);
				uint32_t final_index = seek_final_index (wpp->io, wpp->io);
				if (final_index != (uint32_t) -1)
				{
					wpp->first_wphdr.total_samples = final_index - wpp->first_wphdr.block_index;
				}				 
				// restaure position
				wpp->io->set_pos_abs(wpp->io, curr_pos);
			}
		}

		wpp->channel_count += wpp->block_channel_count;

		striped_header_len = strip_wavpack_block(wpp->fb, &wpp->wphdr, wpp->io, bcount,
			!wpp->is_correction, wpp->several_blocks);

        // if this is the first block, we scan the metadata for special cases (specifically
        // non-standard sampling rates and channel masks)

		if (wpp->fb->nb_block == 1) {
            wpp->channel_mask = 0x5 - wpp->channel_count;   // default basic case of 1 or 2 channels
            my_scan_metadata (wpp, wpp->fb->data + striped_header_len, wpp->fb->len - striped_header_len);
        }
	} while(is_final_block == FALSE);
	
	if(wpp->fb->len > 0)
	{
        // based on reading the first block(s), calculate the maximum buffer required for an entire frame
        unsigned int samples_per_block = wpp->sample_rate;  // "high" mode defaults to 1 second blocks

        // but this is scaled back for multichannel or high sample-rate files
        while (samples_per_block * wpp->channel_count > 150000)
            samples_per_block /= 2;

        // make sure user did not override the default
        if (wpp->block_samples_per_block > samples_per_block)
            samples_per_block = wpp->block_samples_per_block;

        // this is an estimate based on zero compression (which would be the case with FS white noise)
        wpp->suggested_buffer_size = samples_per_block * wpp->channel_count * wpp->block_bits_per_sample / 8;

        // now we add a percentage for possible overhead (more for float data)
        if (wpp->first_wphdr.flags & FLOAT_DATA)
		    wpp->suggested_buffer_size += wpp->suggested_buffer_size >> 1;
        else
		    wpp->suggested_buffer_size += wpp->suggested_buffer_size >> 3;

        // finally add overhead of WavPack headers
		wpp->suggested_buffer_size += (sizeof(WavpackHeader) * wpp->fb->nb_block);

        DebugLog("wavpack_parser_new(): samples in first block = %d, suggested_buffer_size = %d",
            wpp->block_samples_per_block, wpp->suggested_buffer_size);

		return wpp;
	}
	else
	{
		wavpack_parser_free(wpp);
		return NULL;
	}
}

// ----------------------------------------------------------------------------

unsigned long wavpack_parser_read_frame(
	WavPack_parser* wpp,
	unsigned char* dst,
	unsigned long* FrameIndex,
	unsigned long* FrameLen)
{
	int is_final_block = FALSE;
	uint32_t bcount = 0;
	uint32_t frame_len_bytes = 0;

	if(wpp->fb->len > 0)
	{
		*FrameIndex = wpp->wphdr.block_index;
		*FrameLen = wpp->wphdr.block_samples;		 
		wp_memcpy(dst, wpp->fb->data, wpp->fb->len);
	}
	else
	{
		do {
			// Get next WavPack block info
			bcount = get_wp_block(wpp, &is_final_block);
			if(bcount == -1)
			{
				wpp->wvparser_eof = 1;
				break;
			}
			strip_wavpack_block(wpp->fb, &wpp->wphdr, wpp->io, bcount,
				!wpp->is_correction,  wpp->several_blocks);
		} while(is_final_block == FALSE);

		*FrameIndex = wpp->wphdr.block_index;
		*FrameLen = wpp->wphdr.block_samples;
		wp_memcpy(dst, wpp->fb->data, wpp->fb->len);
	}

	frame_len_bytes = wpp->fb->len;

	frame_reset(wpp->fb);

	return frame_len_bytes;
}

// ----------------------------------------------------------------------------


void wavpack_parser_seek(WavPack_parser* wpp, uint64 seek_pos_100ns)
{
	uint32_t sample_pos = (uint32_t)((seek_pos_100ns / 10000000.0) * wpp->sample_rate);
	uint32_t newpos = find_sample(wpp, 0, sample_pos);

	DebugLog("%c wavpack_parser_seek : seeking at pos = %d",
		wpp->is_correction ? 'C': 'N',
		newpos);

	if(wpp->io->set_pos_abs(wpp->io, newpos) == 0)
	{
		wpp->wvparser_eof = 0;
	}
	else
	{
		wpp->wvparser_eof = 1;
	}
}

// ----------------------------------------------------------------------------

int wavpack_parser_eof(WavPack_parser* wpp)
{
	return wpp->wvparser_eof;
}

// ----------------------------------------------------------------------------

void wavpack_parser_free(WavPack_parser* wpp)
{
	if(wpp != NULL)
	{
		if(wpp->fb != NULL)
		{
			frame_buffer_free(wpp->fb);
			wpp->fb = NULL;
		}		 
		wp_free(wpp);
	}
}

// ----------------------------------------------------------------------------

static void
little_endian_to_native(void *data, char *format)
{
	uint8_t *cp = (uint8_t *)data;
	int32_t temp;
	
	while (*format) 
	{
		switch (*format) 
		{
		case 'L':
			temp = cp [0] + ((int32_t) cp [1] << 8) + ((int32_t) cp [2] << 16) + ((int32_t) cp [3] << 24);
			* (int32_t *) cp = temp;
			cp += 4;
			break;
			
		case 'S':
			temp = cp [0] + (cp [1] << 8);
			* (short *) cp = (short) temp;
			cp += 2;
			break;
			
		default:
			if (isdigit(*format))
				cp += *format - '0';
			
			break;
		}
		
		format++;
	}
}

// ----------------------------------------------------------------------------

// Read from current file position until a valid 32-byte WavPack 4.0 header is
// found and read into the specified pointer. The number of bytes skipped is
// returned. If no WavPack header is found within 1 meg, then a -1 is returned
// to indicate the error. No additional bytes are read past the header and it
// is returned in the processor's native endian mode. Seeking is not required.

static uint32_t read_next_header (WavpackStreamReader *reader, void *id, WavpackHeader *wphdr)
{
	char buffer [sizeof (*wphdr)], *sp = buffer + sizeof (*wphdr), *ep = sp;
	uint32_t bytes_skipped = 0;
	int bleft;
	
	while (1) {
		if (sp < ep) {
			bleft = (int)(ep - sp);
			wp_memcpy (buffer, sp, bleft);
		}
		else
			bleft = 0;
		
		if (reader->read_bytes (id, buffer + bleft, sizeof (*wphdr) - bleft) != sizeof (*wphdr) - bleft)
			return -1;
		
		sp = buffer;
		
		if (*sp++ == 'w' && *sp == 'v' && *++sp == 'p' && *++sp == 'k' &&
			!(*++sp & 1) && sp [2] < 16 && !sp [3] && sp [5] == 4 &&
			sp [4] >= (MIN_STREAM_VERS & 0xff) && sp [4] <= (MAX_STREAM_VERS & 0xff))
		{
			wp_memcpy (wphdr, buffer, sizeof (*wphdr));
			little_endian_to_native (wphdr, WavpackHeaderFormat);
			return bytes_skipped;
		}
		
		while (sp < ep && *sp != 'w')
			sp++;
		
		if ((bytes_skipped += (uint32_t)(sp - buffer)) > 1024 * 1024)
			return -1;
	}
}


// ----------------------------------------------------------------------------

int32_t
get_wp_block(WavPack_parser *wpp, int* is_final_block)
{
	uint32_t bcount = 0;
	uint32_t data_size = 0;
	uint32_t bytes_per_sample = 0;

	*is_final_block = FALSE;

	wpp->block_channel_count = 0;
	wpp->block_sample_rate = 0;
	wpp->block_bits_per_sample = 0;
	wpp->block_samples_per_block = 0;
	
	DebugLog("%c get_wp_block : current pos = %d",
		wpp->is_correction ? 'C' : 'N',
		wpp->io->get_pos(wpp->io));

	// read next WavPack header
	bcount = read_next_header(wpp->io, wpp->io, &wpp->wphdr);

	if(bcount > 0)
	{
		DebugLog("%c get_wp_block : skipped %d",
			wpp->is_correction ? 'C' : 'N',
			bcount);
	}

	if (bcount == (uint32_t) -1)
	{
		return -1;
	}

	// if there's audio samples in there...
	if (wpp->wphdr.block_samples)
	{
		if((wpp->wphdr.flags & SRATE_MASK) == SRATE_MASK)
		{
			wpp->block_sample_rate = 44100;
		}
		else
		{
			wpp->block_sample_rate = (wpp->wphdr.flags & SRATE_MASK) >> SRATE_LSB;
			wpp->block_sample_rate = sample_rates[wpp->block_sample_rate];
		}

		bytes_per_sample = ((wpp->wphdr.flags & BYTES_STORED) + 1);
	  
		wpp->block_bits_per_sample = (bytes_per_sample * 8) - 
			((wpp->wphdr.flags & SHIFT_MASK) >> SHIFT_LSB);

		wpp->block_samples_per_block = wpp->wphdr.block_samples;

		wpp->block_channel_count = (wpp->wphdr.flags & MONO_FLAG) ? 1 : 2;

		if (wpp->wphdr.flags & FINAL_BLOCK)
		{
			*is_final_block = TRUE;
		}
	}
	else
	{
		// printf ("non-audio block found\n");
		return -1;
	}

	data_size = wpp->wphdr.ckSize - sizeof(WavpackHeader) + 8;
	
	return data_size;
}

// ----------------------------------------------------------------------------

// Find a valid WavPack header, searching either from the current file position
// (or from the specified position if not -1) and store it (endian corrected)
// at the specified pointer. The return value is the exact file position of the
// header, although we may have actually read past it. Because this function
// is used for seeking to a specific audio sample, it only considers blocks
// that contain audio samples for the initial stream to be valid.

#define BUFSIZE 4096

static uint32_t find_header (WavpackStreamReader *reader,
							 void *id,
							 uint32_t filepos,
							 WavpackHeader *wphdr)
{
	char *buffer = wp_alloc(BUFSIZE), *sp = buffer, *ep = buffer;
	
	if (filepos != (uint32_t) -1 && reader->set_pos_abs(id, filepos)) {
		wp_free(buffer);
		return -1;
	}
	
	while (1) {
		int bleft;
		
		if (sp < ep) {
			bleft = (int)(ep - sp);
			wp_memcpy(buffer, sp, bleft);
			ep -= (sp - buffer);
			sp = buffer;
		}
		else {
			if (sp > ep)
			{
				if (reader->set_pos_rel (id, (int32_t)(sp - ep), SEEK_CUR)) {
					wp_free(buffer);
					return -1;
				}
			}
				
			sp = ep = buffer;
			bleft = 0;
		}
		
		ep += reader->read_bytes (id, ep, BUFSIZE - bleft);
		
		if (ep - sp < 32) {
			wp_free(buffer);
			return -1;
		}
		
		while (sp + 32 <= ep) {
			if (*sp++ == 'w' && *sp == 'v' && *++sp == 'p' && *++sp == 'k' &&
				!(*++sp & 1) && sp [2] < 16 && !sp [3] && sp [5] == 4 &&
				sp [4] >= (MIN_STREAM_VERS & 0xff) && sp [4] <= (MAX_STREAM_VERS & 0xff)) {
				wp_memcpy(wphdr, sp - 4, sizeof (*wphdr));
				little_endian_to_native(wphdr, WavpackHeaderFormat);
				
				if (wphdr->block_samples && (wphdr->flags & INITIAL_BLOCK)) {
					wp_free(buffer);
					return reader->get_pos (id) - (ep - sp + 4);
				}
				
				if (wphdr->ckSize > 1024)
					sp += wphdr->ckSize - 1024;
			}
		}
	}
}

// ----------------------------------------------------------------------------

// Find the WavPack block that contains the specified sample. If "header_pos"
// is zero, then no information is assumed except the total number of samples
// in the file and its size in bytes. If "header_pos" is non-zero then we
// assume that it is the file position of the valid header image contained in
// the first stream and we can limit our search to either the portion above
// or below that point. If a .wvc file is being used, then this must be called
// for that file also.

static uint32_t find_sample(WavPack_parser* wpp,
							uint32_t header_pos,
							uint32_t sample)
{
	uint32_t file_pos1 = 0, file_pos2 = wpp->io->get_length(wpp->io);
	uint32_t sample_pos1 = 0, sample_pos2 = wpp->first_wphdr.total_samples;
	double ratio = 0.96;
	int file_skip = 0;

	if (sample >= wpp->first_wphdr.total_samples)
		return -1;

	if (header_pos && wpp->wphdr.block_samples) {
		if (wpp->wphdr.block_index > sample) {
			sample_pos2 = wpp->wphdr.block_index;
			file_pos2 = header_pos;
		}
		else if (wpp->wphdr.block_index + wpp->wphdr.block_samples <= sample) {
			sample_pos1 = wpp->wphdr.block_index;
			file_pos1 = header_pos;
		}
		else
			return header_pos;
	}

	while (1) {
		double bytes_per_sample;
		uint32_t seek_pos;

		bytes_per_sample = file_pos2 - file_pos1;
		bytes_per_sample /= sample_pos2 - sample_pos1;
		seek_pos = file_pos1 + (file_skip ? 32 : 0);
		seek_pos += (uint32_t)(bytes_per_sample * (sample - sample_pos1) * ratio);
		seek_pos = find_header(wpp->io, wpp->io, seek_pos, &wpp->wphdr);

		//if (seek_pos != (uint32_t) -1)
		//	  wpp->wphdr.block_index -= wpc->initial_index; // todo

		if (seek_pos == (uint32_t) -1 || seek_pos >= file_pos2) {
			if (ratio > 0.0) {
				if ((ratio -= 0.24) < 0.0) {
					ratio = 0.0;
				}
			}
			else
				return -1;
		}
		else if (wpp->wphdr.block_index > sample) {
			sample_pos2 = wpp->wphdr.block_index;
			file_pos2 = seek_pos;
		}
		else if (wpp->wphdr.block_index + wpp->wphdr.block_samples <= sample) {
			if (seek_pos == file_pos1)
				file_skip = 1;
			else {
				sample_pos1 = wpp->wphdr.block_index;
				file_pos1 = seek_pos;
			}
		} else {	   
			return seek_pos;
		}
	}
}

// ----------------------------------------------------------------------------

// This function is used to seek to end of a file to determine its actual
// length in samples by reading the last header block containing data.
// Currently, all WavPack files contain the sample length in the first block
// containing samples, however this might not always be the case. Obviously,
// this function requires a seekable file or stream and leaves the file
// pointer undefined. A return value of -1 indicates the length could not
// be determined.

static uint32_t seek_final_index (WavpackStreamReader *reader, void *id)
{
	uint32_t result = (uint32_t) -1, bcount;
	WavpackHeader wphdr;
	uchar *tempbuff;
	
	if (reader->get_length (id) > 1200000L)
		reader->set_pos_rel (id, -1048576L, SEEK_END);
	
	while (1) {
		bcount = read_next_header (reader, id, &wphdr);
		
		if (bcount == (uint32_t) -1)
			return result;
		
		tempbuff = wp_alloc (wphdr.ckSize + 8);
		wp_memcpy (tempbuff, &wphdr, 32);

		if (reader->read_bytes (id, tempbuff + 32, wphdr.ckSize - 24) != wphdr.ckSize - 24) {
			wp_free (tempbuff);
			return result;
		}

		wp_free (tempbuff);

		if (wphdr.block_samples && (wphdr.flags & FINAL_BLOCK))
			result = wphdr.block_index + wphdr.block_samples;
	}
}

// ----------------------------------------------------------------------------
