/* crossfade.c

cc -o crossfade crossfade.c -l sndfile

** Copyright (C) 2008 Victor Paesa
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.


 |
 | original_sound
 +--------------------                                  -----
 |                    \          sound_to_mix          /
 |                     \     --------------------     /
 |                      \   /                    \   /
 |                       \ /                      \ /
 |                        \                        \
 |                       / \                      / \
 |                      /   \____________________/   \
 |                     /                              \
-+--------------------/--------------------------------------
 |

*/

#include <stdio.h>
#include <stdlib.h>

/* Include this header file to use functions from libsndfile. */
#include <sndfile.h>

/*
struct SF_INFO
{	sf_count_t	frames;
	int			samplerate;
	int			channels;
	int			format;
	int			sections;
	int			seekable;
};

** The number of items actually read/written = frames * number of channels.
*/

static char *usage_str = "\
Usage:\n\
\n\
crossfade overlap_time volume_mix original_sound start_mix_time sound_to_mix final_sound\n\
    overlap_time      time in seconds that the sounds overlap\n\
    volume_mix        a float number from 0.0 to 1.0\n\
    original_sound    the longest of the sounds to crossfade\n\
    start_mix_time    time in seconds when crossfade starts in original_sound\n\
    sound_to_mix      the other sound to crossfade\n\
    final_sound       the final result sound\n\
\n\
Example:\n\
crossfade 0.5 0.9 soundtrack.wav 23.40 voice.wav soundtrack_and_voice.wav\n\
\n\
Caveats:\n\
You cannot crossfade further than the length of the original_sound.\n\
";


int main(int argc, char **argv)
{   /* This is a buffer of double precision floating point values
    ** which will hold our data while we process it.
    */
    static double *original_data, *tomix_data;

    /* A SNDFILE is very much like a FILE in the Standard C library. The
    ** sf_open function return an SNDFILE* pointer when they sucessfully
	** open the specified file.
    */
    SNDFILE      *original, *tomix, *final;

    /* A pointer to an SF_INFO stutct is passed to sf_open.
    ** On read, the library fills this struct with information about the file.
    ** On write, the struct must be filled in before calling sf_open.
    */
    SF_INFO       original_info, tomix_info;
    int           item_count;
    double        overlap_time; // atof(argv[1]);
    double        volume_mix;   // atof(argv[2]);
    const char	 *original_name = argv[3];
    double       start_mix_time; // atof(argv[4]);
    const char	 *tomix_name   = argv[5];
    const char	 *final_name   = argv[6];

    int n, chan, start_mix_frame, overlap_frames, original_frames;
    double *sd1, *sd2, ratio, inv_overlap_frames;

    /* Here's where we open the input file. We pass sf_open the file name and
    ** a pointer to an SF_INFO struct.
    ** On successful open, sf_open returns a SNDFILE* pointer which is used
    ** for all subsequent operations on that file.
    ** If an error occurs during sf_open, the function returns a NULL pointer.
	**
	** If you are trying to open a raw headerless file you will need to set the
	** format and channels fields of original_info before calling sf_open(). For
	** instance to open a raw 16 bit stereo PCM file you would need the following
	** two lines:
	**
	**		original_info.format   = SF_FORMAT_RAW | SF_FORMAT_PCM_16;
	**		original_info.channels = 2;
    */
    if (argc != 7) {
        puts(usage_str);
        return 1;
    }
    
    overlap_time        = atof(argv[1]);
    volume_mix          = atof(argv[2]);
    start_mix_time      = atof(argv[4]);

    if (! (original = sf_open(original_name, SFM_READ, &original_info))) {
        /* Open failed so print an error message. */
        printf("Not able to open input file %s.\n", original_name);
        puts(sf_strerror(NULL));
        return  1;
    };

    if (! (sf_command(original, SFC_SET_NORM_DOUBLE, NULL, SF_FALSE))) {
        printf("Not able to avoid normalization in %s.\n", original_name);
        puts(sf_strerror(NULL));
        return  1;
    };

    // Read the whole original_sound
    if (!(original_data = malloc(original_info.frames * original_info.channels * sizeof(double)))) {
        printf("Could not allocate %d bytes.\n", (int) original_info.frames * original_info.channels * sizeof(double));
        return 1;
    }
    item_count = sf_read_double(original, original_data, original_info.frames * original_info.channels);
    //printf("item_count original:%d.\n", item_count);
    if (original_info.frames * original_info.channels != item_count) {
        printf("Tried to read %d items, but read %d.\n", (int) original_info.frames * original_info.channels, item_count);
        return 1;
    }
    
    if (! (tomix = sf_open(tomix_name, SFM_READ, &tomix_info))) {
        /* Open failed so print an error message. */
        printf("Not able to open input file %s.\n", tomix_name);
        puts(sf_strerror(NULL));
        return  1;
    };

    if (! (sf_command(tomix, SFC_SET_NORM_DOUBLE, NULL, SF_FALSE))) {
        printf("Not able to avoid normalization in %s.\n", tomix_name);
        puts(sf_strerror(NULL));
        return  1;
    };
    //printf("original.frames:%lld tomix.frames:%lld\n", original_info.frames , tomix_info.frames);

    // Read the whole sound to mix
    if (! (tomix_data = malloc(tomix_info.frames * tomix_info.channels * sizeof(double)))) {
        printf("Could not allocate %d bytes.\n", (int) tomix_info.frames * tomix_info.channels * sizeof(double));
        return 1;
    }
    item_count = sf_read_double(tomix, tomix_data, tomix_info.frames * tomix_info.channels);
    //printf("item_count tomix:%d\n", item_count);
    if (tomix_info.frames * tomix_info.channels != item_count) {
        printf("Tried to read %d items, but read %d.\n", (int) tomix_info.frames * tomix_info.channels, item_count);
        return 1;
    }
        
    start_mix_frame = original_info.samplerate * start_mix_time;
    overlap_frames =  original_info.samplerate * overlap_time;
    //printf("start_mix_frame:%d overlap_frames:%d\n", start_mix_frame , overlap_frames);

    if (original_info.samplerate != tomix_info.samplerate) {
        printf("Sound to mix '%s' must have the same %d sample rate as original sound '%s'.\n", 
            tomix_name, original_info.samplerate, original_name);
        return 1;
    }
    if (original_info.channels != tomix_info.channels) {
        printf("Sound to mix '%s' must have the same %d channels as original sound '%s'.\n", 
            tomix_name, original_info.channels, original_name);
        return 1;
    }
    if ((start_mix_frame + tomix_info.frames) > original_info.frames) {
        printf("Crossfade does not fit into original sound length.\n"
        "  Either shrink sound to mix'%s' to %lf seconds or\n"
        "  enlarge original sound '%s' to %lf seconds.\n", 
        tomix_name, (original_info.frames - start_mix_frame)/(double)original_info.samplerate,
        original_name, (start_mix_frame + tomix_info.frames)/(double)original_info.samplerate);
        return 1;
    }

    sd1 = original_data + start_mix_frame * original_info.channels;
    sd2 = tomix_data;
    inv_overlap_frames = 1.0 / overlap_frames;

    // First slope
    for (n=0; n < overlap_frames; n++) {
        ratio = volume_mix * inv_overlap_frames;
        for (chan=0; chan < original_info.channels; chan++) {
            *sd1 = *sd2 * ratio + *sd1 * (1.0 - ratio);
            sd1++; sd2++;
        }
    }
   
   // Plateau
    for (n=0; n < (tomix_info.frames -2*overlap_frames); n++) {
        for (chan=0; chan < original_info.channels; chan++) {
            *sd1 = *sd2 * volume_mix + *sd1 * (1.0 - volume_mix);
            sd1++; sd2++;
        }
    }

   // Second slope
    for (n=0; n < overlap_frames; n++) {
        ratio = volume_mix * (1.0 - inv_overlap_frames);
        for (chan=0; chan < original_info.channels; chan++) {
            *sd1 = *sd2 * ratio + *sd1 * (1.0 - ratio);
            sd1++; sd2++;
        }
    }

    /* Open the output file. */
    original_frames = original_info.frames;
    if (! (final = sf_open(final_name, SFM_WRITE, &original_info)))
    {   printf("Not able to open output file %s.\n", final_name);
        puts(sf_strerror(NULL));
        return  1;
        };

    if (! (sf_command(final, SFC_SET_NORM_DOUBLE, NULL, SF_FALSE))) {
        printf("Not able to avoid normalization in %s.\n", final_name);
        puts(sf_strerror(NULL));
        return  1;
    };

    item_count = sf_write_double(final, original_data, original_frames * original_info.channels);
    
    /* Close input and output files. */
    sf_close(tomix);
    free(tomix_data);
    sf_close(original);
    sf_close(final);
    free(original_data);

    return 0;
} /* main */
