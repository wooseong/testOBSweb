#include <obs-module.h>
#include <obs-frontend-api/obs-frontend-api.h>
#include <util/darray.h>
#include <util/platform.h>
#include <libavutil/log.h>
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <pthread.h>

#ifdef _WIN32
#include <dxgi.h>
#include <util/dstr.h>
#include <util/windows/win-version.h>
#endif

#include <qmainwindow.h>
#include <qpushbutton.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-ffmpeg", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "FFmpeg based sources/outputs/encoders";
}

extern "C" struct obs_output_info todo_output;

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(55, 27, 100)
#define LIBAVUTIL_VAAPI_AVAILABLE
#endif

#ifndef __APPLE__

static const char *nvenc_check_name = "nvenc_check";

#ifdef _WIN32

typedef HRESULT(WINAPI *create_dxgi_proc)(const IID *, IDXGIFactory1 **);

#endif

#ifdef _WIN32
extern bool load_nvenc_lib(void);
#endif

static bool nvenc_supported(void)
{
	av_register_all();

	profile_start(nvenc_check_name);

	AVCodec *nvenc = avcodec_find_encoder_by_name("nvenc_h264");
	void *lib = NULL;
	bool success = false;

	if (!nvenc) {
		goto cleanup;
	}

	/* ------------------------------------------- */

	success = !!lib;

cleanup:
	if (lib)
		os_dlclose(lib);
#if defined(_WIN32)
finish:
#endif
	profile_end(nvenc_check_name);
	return success;
}

#endif

#ifdef LIBAVUTIL_VAAPI_AVAILABLE
static bool vaapi_supported(void)
{
	AVCodec *vaenc = avcodec_find_encoder_by_name("h264_vaapi");
	return !!vaenc;
}
#endif

#ifdef _WIN32
extern void jim_nvenc_load(void);
extern void jim_nvenc_unload(void);
#endif

bool enumproc(void *privateData, obs_source_t *source)
{
	blog(LOG_INFO, "source name : %s", obs_source_get_name(source));
	return true;
}

bool start_multiple_recording()
{
	return false;
}

void add_custom_button()
{
	QMainWindow *mainWindow = (QMainWindow *)obs_frontend_get_main_window();
	QWidget *dockWidgetContents_3 = mainWindow->findChild<QWidget *>(
		"dockWidgetContents_3", Qt::FindChildrenRecursively);

	QPushButton *button = new QPushButton(dockWidgetContents_3);
	button->setText("custom button");
	button->setMaximumSize(130, 20);
	button->setGeometry(4, 100, 224, 15);
	QObject::connect(button, &QPushButton::clicked, [] {
		obs_enum_sources(enumproc, NULL);
		obs_output_t *output = obs_output_create(
			todo_output.id, todo_output.id, nullptr, nullptr);
		obs_encoder_t *aac_encoder = obs_audio_encoder_create(
			"CoreAudio_AAC", "todo_audio", nullptr, 0, nullptr);
		obs_encoder_t *h264_encoder = obs_video_encoder_create(
			"obs_x264", "todo_video", nullptr, nullptr);

		obs_output_set_audio_encoder(output, aac_encoder, 0);
		obs_output_set_video_encoder(output, h264_encoder);

		// ref : SimpleOutput:Update
		int videoBitrate = 2500;
		int audioBitrate = 160;
		bool advanced = false;
		bool enforceBitrate = true;
		const char *custom = nullptr;
		const char *encoder = "x264";
		const char *presetType = "Preset";
		const char *preset = "veryfast";

		obs_data_t *h264_settings = obs_data_create();

		obs_data_set_string(h264_settings, "rate_control", "CBR");
		obs_data_set_int(h264_settings, "bitrate", videoBitrate);

		if (advanced) {
			obs_data_set_string(h264_settings, "preset", preset);
			obs_data_set_string(h264_settings, "x264opts", custom);
		}

		// obs_service_apply_encoder_settings(main->GetService(), h264Settings, aacSettings);
		obs_data_t *aac_settings = obs_data_create();
		if (advanced && !enforceBitrate) {
			obs_data_set_int(h264_settings, "bitrate",
					 videoBitrate);
			obs_data_set_int(aac_settings, "bitrate", audioBitrate);
		}

		video_t *video = obs_get_video();
		enum video_format format = video_output_get_format(video);
		if (format != VIDEO_FORMAT_NV12 && format != VIDEO_FORMAT_I420)
			obs_encoder_set_preferred_video_format(
				h264_encoder, VIDEO_FORMAT_NV12);
		obs_encoder_update(h264_encoder, h264_settings);
		obs_encoder_update(aac_encoder, aac_settings);

		obs_data_release(h264_settings);
		obs_data_release(aac_settings);

		obs_encoder_set_video(h264_encoder, obs_get_video());
		obs_encoder_set_audio(aac_encoder, obs_get_audio());

		obs_output_start(output);
	});
}

bool obs_module_load(void)
{
	// UI Setup
	add_custom_button();
	obs_register_output(&todo_output);
	return true;
}

void obs_module_unload(void) {}
