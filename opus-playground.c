/* opus-codec.org test&torture tool
 *
 * Copyright (C) 2012 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <float.h>
#include <getopt.h>

#include <sndfile.h>
#include <opus/opus.h>

#ifndef VERSION
#define VERSION "0.0.0"
#endif

#ifdef HAVE_OPUS_CUSTOM
#include <opus/opus_custom.h>
#endif

#define ERREXIT(FMT,...) { fprintf(stderr, FMT"\n", ##__VA_ARGS__); return EXIT_FAILURE; }
#define OPERR(FMT,...)  if (err != OPUS_OK) { fprintf(stderr, FMT ": %d\n", ##__VA_ARGS__, err); return 1; }
#define OPWARN(FMT,...)  if (err != OPUS_OK) { fprintf(stderr, FMT ": %d\n", ##__VA_ARGS__, err); }
#define NOTIFY(FMT,...)  if (!(notify&1)) { printf(" - "FMT"\n", ##__VA_ARGS__); notify|=1; }

static void buffer_constant (float* const b, const int n, const float v) {
	int i;
  for (i = 0; i < n; ++i) {
    b[i] = v;
  }
}

static void buffer_impulse (float* const b, const int n, const float v) {
	int i;
  b[0] = v;
  for (i = 1; i < n; ++i) {
    b[i] = 0;
  }
}

static void buffer_rand_impulse (float* const b, const int n, const float v) {
	int i;
  for (i = 0; i < n; ++i) {
    b[i] = 0;
  }
	int spike = rand() % n;
	b[spike] = v;
}


static void buffer_sine (float* const b, const int n, const int p, const int sr, const int f, const float v) {
	int i;
  for (i = 0; i < n; ++i) {
    b[i] = v * sin (2 * M_PI * f * (i+p) / (double)sr);
  }
}

static void buffer_rect (float* const b, const int n, const float v0, const float v1) {
	int i;
  int const m = n / 2;
  for (i = 0; i < m; ++i) {
    b[i] = v0;
  }
  for (i = m; i < n; ++i) {
    b[i] = v1;
  }
}

static int buffer_has_denormals (const float * const b, const int n) {
	int i;
	for (i = 0; i < n; ++i) {
		if (fpclassify (b[i]) == FP_SUBNORMAL) {
		return 1;
		}
	}
	return 0;
}


struct opuscodec {
#ifdef HAVE_OPUS_CUSTOM
	OpusCustomMode *opus_mode;
	OpusCustomEncoder *encoder_custom;
	OpusCustomDecoder *decoder_custom;
#endif

	OpusEncoder *encoder;
	OpusDecoder *decoder;

	int period_size;
	int CompressedMaxSizeByte;
	unsigned char *EncodedBytes;
};

int opus_vanilla_init(struct opuscodec *cd, const int sample_rate, const int kbps) {

	// setup Opus en/decoders
	int err = OPUS_OK;
	cd->encoder = opus_encoder_create(sample_rate, 1, OPUS_APPLICATION_RESTRICTED_LOWDELAY, &err);
	OPERR("Cannot create Opus encoder");
	err = opus_encoder_ctl(cd->encoder, OPUS_SET_BITRATE(kbps*1024)); // bits per second
	OPWARN("OPUS_SET_BITRATE failed");
	err = opus_encoder_ctl(cd->encoder, OPUS_SET_COMPLEXITY(10));
	OPWARN("OPUS_SET_COMPLEXITY failed");
	err = opus_encoder_ctl(cd->encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));
	OPWARN("OPUS_SIGNAL_MUSIC failed");

	cd->decoder = opus_decoder_create(sample_rate, 1, &err);
	OPERR("Cannot create Opus decoder");

	opus_encoder_init(cd->encoder, sample_rate, 1, OPUS_APPLICATION_RESTRICTED_LOWDELAY);
	opus_decoder_init(cd->decoder, sample_rate, 1);

	int nfo;
	err = opus_encoder_ctl(cd->encoder, OPUS_GET_LOOKAHEAD(&nfo));
	OPWARN("OPUS_GET_LOOKAHEAD failed") else
	printf("Encoder lookahead delay : %d\n", nfo);

	return 0;
}

void opus_vanilla_process(struct opuscodec *cd, float * const in, float * const out) {
	int encoded_byte_cnt = opus_encode_float(cd->encoder, in, cd->period_size, cd->EncodedBytes, cd->CompressedMaxSizeByte);
	int decoded_byte_cnt = opus_decode_float(cd->decoder, cd->EncodedBytes, encoded_byte_cnt, out, cd->period_size, 0);
	if (decoded_byte_cnt != cd->period_size) {
		printf("en/decoded sample-count does not match: want:%d , got:%d\n", cd->period_size, decoded_byte_cnt);
	}
}

void opus_vanilla_destroy(struct opuscodec *cd) {
	opus_encoder_destroy(cd->encoder);
	opus_decoder_destroy(cd->decoder);
}

#ifdef HAVE_OPUS_CUSTOM
int opus_custom_init(struct opuscodec *cd, const int sample_rate, const int kbps) {
	// setup Opus en/decoders
	int err = OPUS_OK;
	cd->opus_mode = opus_custom_mode_create(sample_rate, cd->period_size, &err );
	OPERR("Cannot create Opus mode");

	cd->encoder_custom = opus_custom_encoder_create(cd->opus_mode, 1, &err);
	OPERR("Cannot create Opus encoder");
	err = opus_custom_encoder_ctl(cd->encoder_custom, OPUS_SET_BITRATE(kbps*1024)); // bits per second
	OPWARN("OPUS_SET_BITRATE failed");
	err = opus_custom_encoder_ctl(cd->encoder_custom, OPUS_SET_COMPLEXITY(10));
	OPWARN("OPUS_SET_COMPLEXITY failed");
	err = opus_custom_encoder_ctl(cd->encoder_custom, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));
	OPWARN("OPUS_SIGNAL_MUSIC failed");
	err = opus_custom_encoder_ctl(cd->encoder_custom, OPUS_SET_SIGNAL(OPUS_APPLICATION_RESTRICTED_LOWDELAY));
	OPWARN("OPUS_APPLICATION_RESTRICTED_LOWDELAY failed");

	cd->decoder_custom = opus_custom_decoder_create(cd->opus_mode, 1, &err);
	OPERR("Cannot create Opus decoder");

	opus_custom_encoder_init(cd->encoder_custom, cd->opus_mode, 1);
	opus_custom_decoder_init(cd->decoder_custom, cd->opus_mode, 1);

	int nfo;
	err = opus_custom_encoder_ctl(cd->encoder_custom, OPUS_GET_LOOKAHEAD(&nfo));
	OPWARN("OPUS_GET_LOOKAHEAD failed") else
	printf("Encoder lookahead delay : %d\n", nfo);

	return 0;
}

void opus_custom_process(struct opuscodec *cd, float * const in, float * const out) {
		int encoded_byte_cnt = opus_custom_encode_float(cd->encoder_custom, in, cd->period_size, cd->EncodedBytes, cd->CompressedMaxSizeByte);
		int decoded_byte_cnt = opus_custom_decode_float(cd->decoder_custom, cd->EncodedBytes, encoded_byte_cnt, out,cd-> period_size);
		if (decoded_byte_cnt != cd->period_size) {
			printf("en/decoded sample-count does not match: want:%d , got:%d\n", cd->period_size, decoded_byte_cnt);
		}
}

void opus_custom_destroy(struct opuscodec *cd) {
	opus_custom_encoder_destroy(cd->encoder_custom);
	opus_custom_decoder_destroy(cd->decoder_custom);
	opus_custom_mode_destroy(cd->opus_mode);
}
#endif

static void usage (int status) {
	printf ("opus-torture - test Opus.\n\n");
	printf ("Usage: opus-torture [ OPTIONS ] <filename>\n\n");
	printf ("Options:\n\
  -c, --custom               use opus-custom mode (default: off)\n\
  -e, --evil                 enable evil tests e.g. denormals\n\
  -f, --float                write 32bit float WAV file\n\
  -h, --help                 display this help and exit\n\
  -k, --kbps <num>           kilobit per second for encoding (default 128)\n\
  -p, --period <num>         block-size (default 120)\n\
  -s, --samplerate <num>     audio sample rate (default 48000)\n\
  -V, --version              print version information and exit\n\
\n");
	printf ("\n\
opus-torture writes a stereo-file wav file with various audio patterns\n\
channel 1 is the generated raw audio,\n\
channel 2 has the same signal passed though an opus en/decode cycle\n\
\n\
WARNING: YOU ARE NOT SUPPOSED TO LISTEN TO THE GENERATED AUDIO FILE!\n\
It may damage your equipment. Inspect it with a wave-editor or analyze it\n\
with sndfile-tools,..\n\
\n\
if opus custom is used '-c', the period-size '-p' needs to be a power of\n\
two >=64 and <=1024. Vanilla Opus accepts period-sizes which are multiples\n\
of 120 samples.\n\
By default a 16bit signed integer wav-file is created. This can be overridden\n\
with the '-f' option.\n\
\n");
	printf ("Report bugs to Robin Gareus <robin@gareus.org>\n"
	        "Website and manual: <https://github.com/x42/opus-torture>\n");
	exit (status);
}

static struct option const long_options[] =
{
	{"help", no_argument, 0, 'h'},
	{"version", no_argument, 0, 'V'},
	{"custom", no_argument, 0, 'c'},
	{"evil", no_argument, 0, 'e'},
	{"float", no_argument, 0, 'f'},
	{"kbps", required_argument, 0, 'k'},
	{"period", required_argument, 0, 'p'},
	{"sample-rate", required_argument, 0, 's'},
	{NULL, 0, NULL, 0}
};


int main (int argc, char ** argv)  {
	struct opuscodec cd;

	int (*op_init) (struct opuscodec *cd, int sample_rate, int kbps);
	void (*op_process) (struct opuscodec *cd, float *in, float *out);
	void (*op_destroy) (struct opuscodec *cd);

	op_init = &opus_vanilla_init;
	op_process = &opus_vanilla_process;
	op_destroy = &opus_vanilla_destroy;

	int sample_rate = 48000;
	int period_size = 120; /* custom: 64 <= ps <= 1024 ; norm: N * 120 */
	int kbps = 128;
	int custom = 0;
	int evil = 0;
	int sf_format = SF_FORMAT_PCM_16;
	char *outfile;

	int c;
	while ((c = getopt_long (argc, argv,
			   "c"  /* custom */
			   "e"  /* evil */
			   "f"  /* float */
			   "h"  /* help */
			   "k:" /* kbps */
			   "p:" /* period */
			   "s:" /* sample-rate */
			   "V", /* version */
			   long_options, (int *) 0)) != EOF)
	{
		switch (c) {
			case 's':
				sample_rate = atoi(optarg);
				break;

			case 'f':
				sf_format = SF_FORMAT_FLOAT;
				break;

			case 'k':
				kbps = atoi(optarg);
				break;

			case 'p':
				period_size = atoi(optarg);
				break;

			case 'e':
				evil = 1;
				break;

			case 'c':
#ifdef HAVE_OPUS_CUSTOM
				custom = 1;
				op_init = &opus_custom_init;
				op_process = &opus_custom_process;
				op_destroy = &opus_custom_destroy;
#else
				fprintf(stderr, "this version has been compiled w/o Opus-custom support\n");
				exit(EXIT_FAILURE);
#endif
				break;

			case 'V':
				printf ("opus-torture version %s\n\n", VERSION);
				printf ("Copyright (C) GPL 2012 Robin Gareus <robin@gareus.org>\n");
				exit (0);

			case 'h':
				usage (0);

			default:
				usage (EXIT_FAILURE);
		}
	}

	if (optind >= argc) {
		usage (EXIT_FAILURE);
	}
	outfile = argv[optind];

	// check for sane settings

	if (sample_rate < 8000 || sample_rate > 192000) {
		ERREXIT("Invalid Sample Rate");
	}

	if (!custom) {
		if (period_size % 120 || period_size < 120) {
			ERREXIT("Invalid Period size");
		}
	} else {
		if (period_size < 64 || period_size > 1024
				|| /* not power of two */ (period_size & (period_size - 1))
				) {
			ERREXIT("Invalid Period size");
		}
	}
	if (kbps < 1 || kbps > 512) {
		ERREXIT("Invalid bitrate");
	}

	const int loop = sample_rate / period_size;

	srand(time(NULL)); // needed for buffer_rand_impulse

	//open output file
	SF_INFO sfnfo;
	memset(&sfnfo, 0, sizeof(SF_INFO));
	sfnfo.samplerate = sample_rate;
	sfnfo.channels = 2;
	sfnfo.format = SF_FORMAT_WAV | sf_format;
	SNDFILE* sf = sf_open(outfile, SFM_WRITE, &sfnfo);
	int mode = 0;
	if (!sf) {
		ERREXIT("Cannot open output file");
	}

	// allocate buffers
	cd.CompressedMaxSizeByte = (kbps * period_size * 1024) / (sample_rate * 8);
	cd.period_size = period_size;
	cd.EncodedBytes = (unsigned char*) malloc(cd.CompressedMaxSizeByte);
	float *float_in = (float*) malloc(period_size * sizeof(float));
	float *float_out = (float*) malloc(period_size * sizeof(float));

	if (!float_in || !float_out || !cd.EncodedBytes) {
		printf("OOM\n");
		return 3;
	}

	if (op_init(&cd, sample_rate, kbps)) {
		goto error;
	}

	int total = 0;
	for (mode=0; mode < (evil?11:7); ++mode) {
		int l;
		int notify=0;
		for (l=0; l < loop; l++) {

			switch(mode) {
				case 1:
					NOTIFY("Impulse amp:0.5");
					buffer_impulse(float_in, period_size, .5);
					break;
				case 2:
					NOTIFY("Rect wave amp:-0.5..0.5");
					buffer_rect(float_in, period_size, .5, -.5);
					break;
				case 3:
					NOTIFY("Rect wave amp:0.0..1.0");
					buffer_rect(float_in, period_size, 1.0, 0);
					break;
				case 4:
					NOTIFY("Constant 1e-27");
					buffer_constant (float_in, period_size, 1e-27);
					break;
				case 5:
					NOTIFY("Sine-wave 440Hz, amp:.5");
					buffer_sine (float_in, period_size, (period_size * l), sample_rate, 440, .5);
					break;
				case 6:
					NOTIFY("Random impulses amp:0.5");
					buffer_rand_impulse(float_in, period_size, 0.5);
					break;

					/* evil stuff :) */
				case 7:
					NOTIFY("Rect wave amp:-1.5..1.5");
					buffer_rect(float_in, period_size, -1.5, 1.5);
					break;
				case 8:
					NOTIFY("Constant 1e-38 -- denormal");
					buffer_constant (float_in, period_size, 1e-38);
					break;
				case 9:
					NOTIFY("Constant FLT_MIN (%e)", FLT_MIN);
					buffer_constant (float_in, period_size, FLT_MIN);
					break;
				case 10:
					NOTIFY("Sine-wave 440Hz, amp:1.5");
					buffer_sine (float_in, period_size, (period_size * l), sample_rate, 440, 1.5);
					break;

				default: /* 0 -- silence */
					buffer_constant (float_in, period_size, 0);
					break;
			}

			op_process(&cd, float_in, float_out);

			if (!(notify&4) && buffer_has_denormals(float_in, period_size)) {
				printf(" !   input buffer has denormals.\n");
				notify|=4;
			}

			if (!(notify&2) && buffer_has_denormals(float_out, period_size)) {
				printf(" !   output buffer has denormals.\n");
				notify|=2;
			}

			int s;
			// interleave and write 1 sample at a time. Inefficient, but hey
			for (s=0; s < period_size; ++s) {
				float d[2];
				d[0] = float_in[s];
				d[1] = float_out[s];
				sf_writef_float(sf, d, 1);
			}

			total += period_size;

		} /* for each period */
	} /* for each mode */

	printf("wrote %d frames to '%s'\n", total, outfile);

error:
	if (sf) sf_close(sf);

	free(cd.EncodedBytes);
	op_destroy(&cd);
	free(float_in);
	free(float_out);

	return 0;
}
