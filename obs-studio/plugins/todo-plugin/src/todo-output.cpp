
#include <obs-module.h>
#include <obs-hotkey.h>
#include <obs-avc.h>
#include <util/dstr.h>
#include <util/pipe.h>
#include <util/darray.h>
#include <util/platform.h>
#include <util/circlebuf.h>
#include <util/threading.h>
#include "../ffmpeg-mux/ffmpeg-mux.h"

#ifdef _WIN32
#include "util/windows/win-version.h"
#endif

#include <libavformat/avformat.h>

#include <assert.h>
#include <inttypes.h>

#include <string.h>
#include <stdlib.h>

#include <obs-module.h>
#include <stdio.h>

#include "../ffmpeg-mux/ffmpeg-mux.h"

#define MAX_CONVERT_BUFFERS 3
#define MAX_CACHE_SIZE 16

#define FFMPEG_MUX "obs-ffmpeg-mux.exe"


struct video_scaler {
	struct SwsContext *swscale;
	int src_height;
};

typedef struct video_scaler video_scaler_t;

struct video_frame {
	uint8_t *data[MAX_AV_PLANES];
	uint32_t linesize[MAX_AV_PLANES];
};

struct video_input {
	struct video_scale_info conversion;
	video_scaler_t *scaler;
	struct video_frame frame[MAX_CONVERT_BUFFERS];
	int cur_frame;

	void (*callback)(void *param, struct video_data *frame);
	void *param;
};

struct cached_frame_info {
	struct video_data frame;
	int skipped;
	int count;
};

struct video_output {
	struct video_output_info info;

	pthread_t thread;
	pthread_mutex_t data_mutex;
	bool stop;

	os_sem_t *update_semaphore;
	uint64_t frame_time;
	volatile long skipped_frames;
	volatile long total_frames;

	bool initialized;

	pthread_mutex_t input_mutex;
	DARRAY(struct video_input) inputs;

	size_t available_frames;
	size_t first_added;
	size_t last_added;
	struct cached_frame_info cache[MAX_CACHE_SIZE];

	volatile bool raw_active;
	volatile long gpu_refs;
};



struct ffmpeg_muxer {
	obs_output_t *output;
	os_process_pipe_t *pipe;
	int64_t stop_ts;
	uint64_t total_bytes;
	struct dstr path;
	bool sent_headers;
	volatile bool active;
	volatile bool stopping;
	volatile bool capturing;
	const char *name;

	/* replay buffer */
	struct circlebuf packets;
	int64_t cur_size;
	int64_t cur_time;
	int64_t max_size;
	int64_t max_time;
	int64_t save_ts;
	int keyframes;
	obs_hotkey_id hotkey;

	DARRAY(struct encoder_packet) mux_packets;
	pthread_t mux_thread;
	bool mux_thread_joinable;
	volatile bool muxing;
};

void *todo_output_create(obs_data_t *settings, obs_output_t *output)
{
	UNUSED_PARAMETER(settings);

	struct ffmpeg_muxer *stream = (ffmpeg_muxer *)bzalloc(sizeof(*stream));
	stream->output = output;
	stream->name = obs_output_get_name(output);

	return stream;
}

void add_video_encoder_params(struct ffmpeg_muxer *stream, struct dstr *cmd,
			      obs_encoder_t *vencoder)
{
	obs_data_t *settings = obs_encoder_get_settings(vencoder);
	int bitrate = (int)obs_data_get_int(settings, "bitrate");
	video_t *video = obs_get_video();
	const struct video_output_info *info = video_output_get_info(video);

	obs_data_release(settings);

	dstr_catf(cmd, "%s %d %d %d %d %d ", obs_encoder_get_codec(vencoder),
		  bitrate, obs_output_get_width(stream->output),
		  obs_output_get_height(stream->output), (int)info->fps_num,
		  (int)info->fps_den);
}

void add_audio_encoder_params(struct dstr *cmd, obs_encoder_t *aencoder)
{
	obs_data_t *settings = obs_encoder_get_settings(aencoder);
	int bitrate = (int)obs_data_get_int(settings, "bitrate");
	audio_t *audio = obs_get_audio();
	struct dstr name = {0};

	obs_data_release(settings);

	dstr_copy(&name, obs_encoder_get_name(aencoder));
	dstr_replace(&name, "\"", "\"\"");

	dstr_catf(cmd, "\"%s\" %d %d %d ", name.array, bitrate,
		  (int)obs_encoder_get_sample_rate(aencoder),
		  (int)audio_output_get_channels(audio));

	dstr_free(&name);
}

void add_muxer_params(struct dstr *cmd, struct ffmpeg_muxer *stream)
{
	obs_data_t *settings = obs_output_get_settings(stream->output);
	struct dstr mux = {0};

	dstr_copy(&mux, obs_data_get_string(settings, "muxer_settings"));

	dstr_replace(&mux, "\"", "\\\"");
	obs_data_release(settings);

	dstr_catf(cmd, "\"%s\" ", mux.array ? mux.array : "");

	dstr_free(&mux);
}

void build_command_line(struct ffmpeg_muxer *stream, struct dstr *cmd,
			const char *path)
{
	obs_encoder_t *vencoder = obs_output_get_video_encoder(stream->output);
	obs_encoder_t *aencoders[MAX_AUDIO_MIXES];
	int num_tracks = 0;

	for (;;) {
		obs_encoder_t *aencoder = obs_output_get_audio_encoder(
			stream->output, num_tracks);
		if (!aencoder)
			break;

		aencoders[num_tracks] = aencoder;
		num_tracks++;
	}

	dstr_init_move_array(cmd, os_get_executable_path_ptr(FFMPEG_MUX));
	dstr_insert_ch(cmd, 0, '\"');
	dstr_cat(cmd, "\" \"");

	dstr_copy(&stream->path, path);
	dstr_replace(&stream->path, "\"", "\"\"");
	dstr_cat_dstr(cmd, &stream->path);

	dstr_catf(cmd, "\" %d %d ", vencoder ? 1 : 0, num_tracks);

	if (vencoder)
		add_video_encoder_params(stream, cmd, vencoder);

	if (num_tracks) {
		dstr_cat(cmd, "aac ");

		for (int i = 0; i < num_tracks; i++) {
			add_audio_encoder_params(cmd, aencoders[i]);
		}
	}

	add_muxer_params(cmd, stream);
}

void start_pipe(struct ffmpeg_muxer *stream, const char *path)
{
	struct dstr cmd;
	build_command_line(stream, &cmd, path);
	blog(LOG_INFO, cmd.array);
	stream->pipe = os_process_pipe_create(cmd.array, "w");
	dstr_free(&cmd);
}

bool todo_output_start(void *data)
{
	struct ffmpeg_muxer *stream = (ffmpeg_muxer *)data;
	if (!obs_output_can_begin_data_capture(stream->output,
					       0)) /* validation */
		return false;
	if (!obs_output_initialize_encoders(stream->output, 0)) /* initialize */
		return false;
	obs_data_t *settings = obs_output_get_settings(stream->output);
	const char *path = obs_data_get_string(settings, "path");
	blog(LOG_INFO, path);

	start_pipe(stream, path);
	obs_data_release(settings);

	if (!stream->pipe) {
		obs_output_set_last_error(
			stream->output, "HelperProcessFailed");
		blog(LOG_WARNING, "Failed to create process pipe");
		return false;
	}

	/* write headers and start capture */
	os_atomic_set_bool(&stream->active, true);
	os_atomic_set_bool(&stream->capturing, true);
	stream->total_bytes = 0;
	obs_output_begin_data_capture(stream->output, 0);
	obs_output_begin_data_capture(stream->output, 0);
}
