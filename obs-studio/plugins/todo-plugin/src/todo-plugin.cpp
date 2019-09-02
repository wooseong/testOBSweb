#include <obs-module.h>
#include <obs-frontend-api.h>


#include <QtCore/QTimer>
#include <QtWidgets/QAction>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/qpushbutton.h>
#include <qdockwidget.h>


///* Defines common functions (required) */
OBS_DECLARE_MODULE()


bool enumproc(void *privateData, obs_source_t *source)
{
	obs_source *s = source;
//	const char *name = obs_source_get_name(source);
//	blog(LOG_INFO, "success, name : %s", name);
//	video_t *video = obs_get_video();
//
//	for (size_t i = 0; i < video->inputs.num; i++) {
//		const char *info_id_template = "multivideo_record";
//		char *id = (char *)malloc(strlen(info_id_template) + 10);
//		sprintf(id, "%s%d", info_id_template, i);
//		struct video_input *input = video->inputs.array + i;
//		obs_output_info *info_per_file =
//			(obs_output_info *)malloc(sizeof(obs_output_info));
//		obs_register_output(info_per_file);
//		obs_output_t *output_per_file =
//			obs_output_create(id, id, nullptr, nullptr);
//		obs_output_start(output_per_file);
//	}
	return true;
}

bool obs_module_load(void)
{
	// UI Setup
	QMainWindow *mainWindow = (QMainWindow *)obs_frontend_get_main_window();
	QWidget *dockWidgetContents_3 = mainWindow->findChild<QWidget *>(
		"dockWidgetContents_3", Qt::FindChildrenRecursively);

	QPushButton *button = new QPushButton(dockWidgetContents_3);
	button->setText("custom button");
	button->setMaximumSize(130, 20);
	button->setGeometry(4, 100, 224, 15);
	QObject::connect(button, &QPushButton::clicked,
		[]
		{
			obs_enum_sources(enumproc, NULL);
		});

	obs_enum_sources(
		enumproc,
		NULL); // 이 때 obs 가 초기화되어있지 않아 source 가 null 이다.
	return true;
}
