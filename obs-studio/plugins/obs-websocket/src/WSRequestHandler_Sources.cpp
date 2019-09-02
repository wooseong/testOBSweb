#include <QtCore/QString>
#include <QtCore/QBuffer>
#include <QtCore/QFileInfo>
#include <QtGui/QImage>
#include <QtGui/QImageWriter>

#include "Utils.h"

#include "WSRequestHandler.h"

/**
* List all sources available in the running OBS instance
*
* @return {Array<Object>} `sources` Array of sources
* @return {String} `sources.*.name` Unique source name
* @return {String} `sources.*.typeId` Non-unique source internal type (a.k.a type id)
* @return {String} `sources.*.type` Source type. Value is one of the following: "input", "filter", "transition", "scene" or "unknown"
*
* @api requests
* @name GetSourcesList
* @category sources
* @since 4.3.0
*/
HandlerResponse WSRequestHandler::HandleGetSourcesList(WSRequestHandler* req)
{
	OBSDataArrayAutoRelease sourcesArray = obs_data_array_create();

	auto sourceEnumProc = [](void* privateData, obs_source_t* source) -> bool {
		obs_data_array_t* sourcesArray = (obs_data_array_t*)privateData;

		OBSDataAutoRelease sourceData = obs_data_create();
		obs_data_set_string(sourceData, "name", obs_source_get_name(source));
		obs_data_set_string(sourceData, "typeId", obs_source_get_id(source));

		QString typeString = "";
		enum obs_source_type sourceType = obs_source_get_type(source);
		switch (sourceType) {
		case OBS_SOURCE_TYPE_INPUT:
			typeString = "input";
			break;

		case OBS_SOURCE_TYPE_FILTER:
			typeString = "filter";
			break;

		case OBS_SOURCE_TYPE_TRANSITION:
			typeString = "transition";
			break;

		case OBS_SOURCE_TYPE_SCENE:
			typeString = "scene";
			break;

		default:
			typeString = "unknown";
			break;
		}
		obs_data_set_string(sourceData, "type", typeString.toUtf8());

		obs_data_array_push_back(sourcesArray, sourceData);
		return true;
	};
	obs_enum_sources(sourceEnumProc, sourcesArray);

	OBSDataAutoRelease response = obs_data_create();
	obs_data_set_array(response, "sources", sourcesArray);
	return req->SendOKResponse(response);
}

/**
* Get a list of all available sources types
*
* @return {Array<Object>} `types` Array of source types
* @return {String} `types.*.typeId` Non-unique internal source type ID
* @return {String} `types.*.displayName` Display name of the source type
* @return {String} `types.*.type` Type. Value is one of the following: "input", "filter", "transition" or "other"
* @return {Object} `types.*.defaultSettings` Default settings of this source type
* @return {Object} `types.*.caps` Source type capabilities
* @return {Boolean} `types.*.caps.isAsync` True if source of this type provide frames asynchronously
* @return {Boolean} `types.*.caps.hasVideo` True if sources of this type provide video
* @return {Boolean} `types.*.caps.hasAudio` True if sources of this type provide audio
* @return {Boolean} `types.*.caps.canInteract` True if interaction with this sources of this type is possible
* @return {Boolean} `types.*.caps.isComposite` True if sources of this type composite one or more sub-sources
* @return {Boolean} `types.*.caps.doNotDuplicate` True if sources of this type should not be fully duplicated
* @return {Boolean} `types.*.caps.doNotSelfMonitor` True if sources of this type may cause a feedback loop if it's audio is monitored and shouldn't be
*
* @api requests
* @name GetSourceTypesList
* @category sources
* @since 4.3.0
*/
HandlerResponse WSRequestHandler::HandleGetSourceTypesList(WSRequestHandler* req)
{
	OBSDataArrayAutoRelease idsArray = obs_data_array_create();

	const char* id;
	size_t idx = 0;

	QHash<QString, QString> idTypes;

	idx = 0;
	while (obs_enum_input_types(idx++, &id)) {
		idTypes.insert(id, "input");
	}

	idx = 0;
	while (obs_enum_filter_types(idx++, &id)) {
		idTypes.insert(id, "filter");
	}

	idx = 0;
	while (obs_enum_transition_types(idx++, &id)) {
		idTypes.insert(id, "transition");
	}

	idx = 0;
	while (obs_enum_source_types(idx++, &id)) {
		OBSDataAutoRelease item = obs_data_create();

		obs_data_set_string(item, "typeId", id);
		obs_data_set_string(item, "displayName", obs_source_get_display_name(id));
		obs_data_set_string(item, "type", idTypes.value(id, "other").toUtf8());

		uint32_t caps = obs_get_source_output_flags(id);
		OBSDataAutoRelease capsData = obs_data_create();
		obs_data_set_bool(capsData, "isAsync", caps & OBS_SOURCE_ASYNC);
		obs_data_set_bool(capsData, "hasVideo", caps & OBS_SOURCE_VIDEO);
		obs_data_set_bool(capsData, "hasAudio", caps & OBS_SOURCE_AUDIO);
		obs_data_set_bool(capsData, "canInteract", caps & OBS_SOURCE_INTERACTION);
		obs_data_set_bool(capsData, "isComposite", caps & OBS_SOURCE_COMPOSITE);
		obs_data_set_bool(capsData, "doNotDuplicate", caps & OBS_SOURCE_DO_NOT_DUPLICATE);
		obs_data_set_bool(capsData, "doNotSelfMonitor", caps & OBS_SOURCE_DO_NOT_SELF_MONITOR);
		obs_data_set_bool(capsData, "isDeprecated", caps & OBS_SOURCE_DEPRECATED);

		obs_data_set_obj(item, "caps", capsData);

		OBSDataAutoRelease defaultSettings = obs_get_source_defaults(id);
		obs_data_set_obj(item, "defaultSettings", defaultSettings);

		obs_data_array_push_back(idsArray, item);
	}

	OBSDataAutoRelease response = obs_data_create();
	obs_data_set_array(response, "types", idsArray);
	return req->SendOKResponse(response);
}

/**
* Get the volume of the specified source.
*
* @param {String} `source` Source name.
*
* @return {String} `name` Source name.
* @return {double} `volume` Volume of the source. Between `0.0` and `1.0`.
* @return {boolean} `muted` Indicates whether the source is muted.
*
* @api requests
* @name GetVolume
* @category sources
* @since 4.0.0
*/
HandlerResponse WSRequestHandler::HandleGetVolume(WSRequestHandler* req)
{
	if (!req->hasField("source")) {
		return req->SendErrorResponse("missing request parameters");
	}

	QString sourceName = obs_data_get_string(req->data, "source");
	if (sourceName.isEmpty()) {
		return req->SendErrorResponse("invalid request parameters");
	}

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName.toUtf8());
	if (!source) {
		return req->SendErrorResponse("specified source doesn't exist");
	}

	OBSDataAutoRelease response = obs_data_create();
	obs_data_set_string(response, "name", obs_source_get_name(source));
	obs_data_set_double(response, "volume", obs_source_get_volume(source));
	obs_data_set_bool(response, "muted", obs_source_muted(source));

	return req->SendOKResponse(response);
}

/**
 * Set the volume of the specified source.
 *
 * @param {String} `source` Source name.
 * @param {double} `volume` Desired volume. Must be between `0.0` and `1.0`.
 *
 * @api requests
 * @name SetVolume
 * @category sources
 * @since 4.0.0
 */
HandlerResponse WSRequestHandler::HandleSetVolume(WSRequestHandler* req)
 {
	if (!req->hasField("source") || !req->hasField("volume")) {
		return req->SendErrorResponse("missing request parameters");
	}

	QString sourceName = obs_data_get_string(req->data, "source");
	float sourceVolume = obs_data_get_double(req->data, "volume");

	if (sourceName.isEmpty() || sourceVolume < 0.0 || sourceVolume > 1.0) {
		return req->SendErrorResponse("invalid request parameters");
	}

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName.toUtf8());
	if (!source) {
		return req->SendErrorResponse("specified source doesn't exist");
	}

	obs_source_set_volume(source, sourceVolume);
	return req->SendOKResponse();
}

/**
* Get the mute status of a specified source.
*
* @param {String} `source` Source name.
*
* @return {String} `name` Source name.
* @return {boolean} `muted` Mute status of the source.
*
* @api requests
* @name GetMute
* @category sources
* @since 4.0.0
*/
HandlerResponse WSRequestHandler::HandleGetMute(WSRequestHandler* req)
{
	if (!req->hasField("source")) {
		return req->SendErrorResponse("missing request parameters");
	}

	QString sourceName = obs_data_get_string(req->data, "source");
	if (sourceName.isEmpty()) {
		return req->SendErrorResponse("invalid request parameters");
	}

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName.toUtf8());
	if (!source) {
		return req->SendErrorResponse("specified source doesn't exist");
	}

	OBSDataAutoRelease response = obs_data_create();
	obs_data_set_string(response, "name", obs_source_get_name(source));
	obs_data_set_bool(response, "muted", obs_source_muted(source));

	return req->SendOKResponse(response);
}

/**
 * Sets the mute status of a specified source.
 *
 * @param {String} `source` Source name.
 * @param {boolean} `mute` Desired mute status.
 *
 * @api requests
 * @name SetMute
 * @category sources
 * @since 4.0.0
 */
HandlerResponse WSRequestHandler::HandleSetMute(WSRequestHandler* req)
{
	if (!req->hasField("source") || !req->hasField("mute")) {
		return req->SendErrorResponse("missing request parameters");
	}

	QString sourceName = obs_data_get_string(req->data, "source");
	bool mute = obs_data_get_bool(req->data, "mute");

	if (sourceName.isEmpty()) {
		return req->SendErrorResponse("invalid request parameters");
	}

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName.toUtf8());
	if (!source) {
		return req->SendErrorResponse("specified source doesn't exist");
	}

	obs_source_set_muted(source, mute);
	return req->SendOKResponse();
}

/**
* Inverts the mute status of a specified source.
*
* @param {String} `source` Source name.
*
* @api requests
* @name ToggleMute
* @category sources
* @since 4.0.0
*/
HandlerResponse WSRequestHandler::HandleToggleMute(WSRequestHandler* req)
{
	if (!req->hasField("source")) {
		return req->SendErrorResponse("missing request parameters");
	}

	QString sourceName = obs_data_get_string(req->data, "source");
	if (sourceName.isEmpty()) {
		return req->SendErrorResponse("invalid request parameters");
	}

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName.toUtf8());
	if (!source) {
		return req->SendErrorResponse("invalid request parameters");
	}

	obs_source_set_muted(source, !obs_source_muted(source));
	return req->SendOKResponse();
}

/**
 * Set the audio sync offset of a specified source.
 *
 * @param {String} `source` Source name.
 * @param {int} `offset` The desired audio sync offset (in nanoseconds).
 *
 * @api requests
 * @name SetSyncOffset
 * @category sources
 * @since 4.2.0
 */
HandlerResponse WSRequestHandler::HandleSetSyncOffset(WSRequestHandler* req)
{
	if (!req->hasField("source") || !req->hasField("offset")) {
		return req->SendErrorResponse("missing request parameters");
	}

	QString sourceName = obs_data_get_string(req->data, "source");
	int64_t sourceSyncOffset = (int64_t)obs_data_get_int(req->data, "offset");

	if (sourceName.isEmpty() || sourceSyncOffset < 0) {
		return req->SendErrorResponse("invalid request parameters");
	}

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName.toUtf8());
	if (!source) {
		return req->SendErrorResponse("specified source doesn't exist");
	}

	obs_source_set_sync_offset(source, sourceSyncOffset);
	return req->SendOKResponse();
}

/**
 * Get the audio sync offset of a specified source.
 *
 * @param {String} `source` Source name.
 *
 * @return {String} `name` Source name.
 * @return {int} `offset` The audio sync offset (in nanoseconds).
 *
 * @api requests
 * @name GetSyncOffset
 * @category sources
 * @since 4.2.0
 */
HandlerResponse WSRequestHandler::HandleGetSyncOffset(WSRequestHandler* req)
{
	if (!req->hasField("source")) {
		return req->SendErrorResponse("missing request parameters");
	}

	QString sourceName = obs_data_get_string(req->data, "source");
	if (sourceName.isEmpty()) {
		return req->SendErrorResponse("invalid request parameters");
	}

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName.toUtf8());
	if (!source) {
		return req->SendErrorResponse("specified source doesn't exist");
	}

	OBSDataAutoRelease response = obs_data_create();
	obs_data_set_string(response, "name", obs_source_get_name(source));
	obs_data_set_int(response, "offset", obs_source_get_sync_offset(source));

	return req->SendOKResponse(response);
}

/**
* Get settings of the specified source
*
* @param {String} `sourceName` Source name.
* @param {String (optional)} `sourceType` Type of the specified source. Useful for type-checking if you expect a specific settings schema.
*
* @return {String} `sourceName` Source name
* @return {String} `sourceType` Type of the specified source
* @return {Object} `sourceSettings` Source settings (varies between source types, may require some probing around).
*
* @api requests
* @name GetSourceSettings
* @category sources
* @since 4.3.0
*/
HandlerResponse WSRequestHandler::HandleGetSourceSettings(WSRequestHandler* req)
{
	if (!req->hasField("sourceName")) {
		return req->SendErrorResponse("missing request parameters");
	}

	const char* sourceName = obs_data_get_string(req->data, "sourceName");
	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		return req->SendErrorResponse("specified source doesn't exist");
	}

	if (req->hasField("sourceType")) {
		QString actualSourceType = obs_source_get_id(source);
		QString requestedType = obs_data_get_string(req->data, "sourceType");

		if (actualSourceType != requestedType) {
			return req->SendErrorResponse("specified source exists but is not of expected type");
		}
	}

	OBSDataAutoRelease sourceSettings = obs_source_get_settings(source);

	OBSDataAutoRelease response = obs_data_create();
	obs_data_set_string(response, "sourceName", obs_source_get_name(source));
	obs_data_set_string(response, "sourceType", obs_source_get_id(source));
	obs_data_set_obj(response, "sourceSettings", sourceSettings);

	return req->SendOKResponse(response);
}

/**
* Set settings of the specified source.
*
* @param {String} `sourceName` Source name.
* @param {String (optional)} `sourceType` Type of the specified source. Useful for type-checking to avoid settings a set of settings incompatible with the actual source's type.
* @param {Object} `sourceSettings` Source settings (varies between source types, may require some probing around).
*
* @return {String} `sourceName` Source name
* @return {String} `sourceType` Type of the specified source
* @return {Object} `sourceSettings` Updated source settings
*
* @api requests
* @name SetSourceSettings
* @category sources
* @since 4.3.0
*/
HandlerResponse WSRequestHandler::HandleSetSourceSettings(WSRequestHandler* req)
{
	if (!req->hasField("sourceName") || !req->hasField("sourceSettings")) {
		return req->SendErrorResponse("missing request parameters");
	}

	const char* sourceName = obs_data_get_string(req->data, "sourceName");
	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		return req->SendErrorResponse("specified source doesn't exist");
	}

	if (req->hasField("sourceType")) {
		QString actualSourceType = obs_source_get_id(source);
		QString requestedType = obs_data_get_string(req->data, "sourceType");

		if (actualSourceType != requestedType) {
			return req->SendErrorResponse("specified source exists but is not of expected type");
		}
	}

	OBSDataAutoRelease currentSettings = obs_source_get_settings(source);
	OBSDataAutoRelease newSettings = obs_data_get_obj(req->data, "sourceSettings");

	OBSDataAutoRelease sourceSettings = obs_data_create();
	obs_data_apply(sourceSettings, currentSettings);
	obs_data_apply(sourceSettings, newSettings);

	obs_source_update(source, sourceSettings);
	obs_source_update_properties(source);

	OBSDataAutoRelease response = obs_data_create();
	obs_data_set_string(response, "sourceName", obs_source_get_name(source));
	obs_data_set_string(response, "sourceType", obs_source_get_id(source));
	obs_data_set_obj(response, "sourceSettings", sourceSettings);

	return req->SendOKResponse(response);
}

/**
 * Get the current properties of a Text GDI Plus source.
 *
 * @param {String} `source` Source name.
 *
 * @return {String} `source` Source name.
 * @return {String} `align` Text Alignment ("left", "center", "right").
 * @return {int} `bk-color` Background color.
 * @return {int} `bk-opacity` Background opacity (0-100).
 * @return {boolean} `chatlog` Chat log.
 * @return {int} `chatlog_lines` Chat log lines.
 * @return {int} `color` Text color.
 * @return {boolean} `extents` Extents wrap.
 * @return {int} `extents_cx` Extents cx.
 * @return {int} `extents_cy` Extents cy.
 * @return {String} `file` File path name.
 * @return {boolean} `read_from_file` Read text from the specified file.
 * @return {Object} `font` Holds data for the font. Ex: `"font": { "face": "Arial", "flags": 0, "size": 150, "style": "" }`
 * @return {String} `font.face` Font face.
 * @return {int} `font.flags` Font text styling flag. `Bold=1, Italic=2, Bold Italic=3, Underline=5, Strikeout=8`
 * @return {int} `font.size` Font text size.
 * @return {String} `font.style` Font Style (unknown function).
 * @return {boolean} `gradient` Gradient enabled.
 * @return {int} `gradient_color` Gradient color.
 * @return {float} `gradient_dir` Gradient direction.
 * @return {int} `gradient_opacity` Gradient opacity (0-100).
 * @return {boolean} `outline` Outline.
 * @return {int} `outline_color` Outline color.
 * @return {int} `outline_size` Outline size.
 * @return {int} `outline_opacity` Outline opacity (0-100).
 * @return {String} `text` Text content to be displayed.
 * @return {String} `valign` Text vertical alignment ("top", "center", "bottom").
 * @return {boolean} `vertical` Vertical text enabled.
 *
 * @api requests
 * @name GetTextGDIPlusProperties
 * @category sources
 * @since 4.1.0
 */
HandlerResponse WSRequestHandler::HandleGetTextGDIPlusProperties(WSRequestHandler* req)
 {
	const char* sourceName = obs_data_get_string(req->data, "source");
	if (!sourceName) {
		return req->SendErrorResponse("invalid request parameters");
	}

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		return req->SendErrorResponse("specified source doesn't exist");
	}

	QString sourceId = obs_source_get_id(source);
	if (sourceId != "text_gdiplus") {
		return req->SendErrorResponse("not a text gdi plus source");
	}

	OBSDataAutoRelease response = obs_source_get_settings(source);
	obs_data_set_string(response, "source", obs_source_get_name(source));

	return req->SendOKResponse(response);
}

/**
 * Set the current properties of a Text GDI Plus source.
 *
 * @param {String} `source` Name of the source.
 * @param {String (optional)} `align` Text Alignment ("left", "center", "right").
 * @param {int (optional)} `bk-color` Background color.
 * @param {int (optional)} `bk-opacity` Background opacity (0-100).
 * @param {boolean (optional)} `chatlog` Chat log.
 * @param {int (optional)} `chatlog_lines` Chat log lines.
 * @param {int (optional)} `color` Text color.
 * @param {boolean (optional)} `extents` Extents wrap.
 * @param {int (optional)} `extents_cx` Extents cx.
 * @param {int (optional)} `extents_cy` Extents cy.
 * @param {String (optional)} `file` File path name.
 * @param {boolean (optional)} `read_from_file` Read text from the specified file.
 * @param {Object (optional)} `font` Holds data for the font. Ex: `"font": { "face": "Arial", "flags": 0, "size": 150, "style": "" }`
 * @param {String (optional)} `font.face` Font face.
 * @param {int (optional)} `font.flags` Font text styling flag. `Bold=1, Italic=2, Bold Italic=3, Underline=5, Strikeout=8`
 * @param {int (optional)} `font.size` Font text size.
 * @param {String (optional)} `font.style` Font Style (unknown function).
 * @param {boolean (optional)} `gradient` Gradient enabled.
 * @param {int (optional)} `gradient_color` Gradient color.
 * @param {float (optional)} `gradient_dir` Gradient direction.
 * @param {int (optional)} `gradient_opacity` Gradient opacity (0-100).
 * @param {boolean (optional)} `outline` Outline.
 * @param {int (optional)} `outline_color` Outline color.
 * @param {int (optional)} `outline_size` Outline size.
 * @param {int (optional)} `outline_opacity` Outline opacity (0-100).
 * @param {String (optional)} `text` Text content to be displayed.
 * @param {String (optional)} `valign` Text vertical alignment ("top", "center", "bottom").
 * @param {boolean (optional)} `vertical` Vertical text enabled.
 * @param {boolean (optional)} `render` Visibility of the scene item.
 *
 * @api requests
 * @name SetTextGDIPlusProperties
 * @category sources
 * @since 4.1.0
 */
HandlerResponse WSRequestHandler::HandleSetTextGDIPlusProperties(WSRequestHandler* req)
{
	if (!req->hasField("source")) {
		return req->SendErrorResponse("missing request parameters");
	}

	const char* sourceName = obs_data_get_string(req->data, "source");
	if (!sourceName) {
		return req->SendErrorResponse("invalid request parameters");
	}

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		return req->SendErrorResponse("specified source doesn't exist");
	}

	QString sourceId = obs_source_get_id(source);
	if (sourceId != "text_gdiplus") {
		return req->SendErrorResponse("not a text gdi plus source");
	}

	OBSDataAutoRelease settings = obs_source_get_settings(source);

	if (req->hasField("align")) {
		obs_data_set_string(settings, "align", obs_data_get_string(req->data, "align"));
	}

	if (req->hasField("bk_color")) {
		obs_data_set_int(settings, "bk_color", obs_data_get_int(req->data, "bk_color"));
	}

	if (req->hasField("bk-opacity")) {
		obs_data_set_int(settings, "bk_opacity", obs_data_get_int(req->data, "bk_opacity"));
	}

	if (req->hasField("chatlog")) {
		obs_data_set_bool(settings, "chatlog", obs_data_get_bool(req->data, "chatlog"));
	}

	if (req->hasField("chatlog_lines")) {
		obs_data_set_int(settings, "chatlog_lines", obs_data_get_int(req->data, "chatlog_lines"));
	}

	if (req->hasField("color")) {
		obs_data_set_int(settings, "color", obs_data_get_int(req->data, "color"));
	}

	if (req->hasField("extents")) {
		obs_data_set_bool(settings, "extents", obs_data_get_bool(req->data, "extents"));
	}

	if (req->hasField("extents_wrap")) {
		obs_data_set_bool(settings, "extents_wrap", obs_data_get_bool(req->data, "extents_wrap"));
	}

	if (req->hasField("extents_cx")) {
		obs_data_set_int(settings, "extents_cx", obs_data_get_int(req->data, "extents_cx"));
	}

	if (req->hasField("extents_cy")) {
		obs_data_set_int(settings, "extents_cy", obs_data_get_int(req->data, "extents_cy"));
	}

	if (req->hasField("file")) {
		obs_data_set_string(settings, "file", obs_data_get_string(req->data, "file"));
	}

	if (req->hasField("font")) {
		OBSDataAutoRelease font_obj = obs_data_get_obj(settings, "font");
		if (font_obj) {
			OBSDataAutoRelease req_font_obj = obs_data_get_obj(req->data, "font");

			if (obs_data_has_user_value(req_font_obj, "face")) {
				obs_data_set_string(font_obj, "face", obs_data_get_string(req_font_obj, "face"));
			}

			if (obs_data_has_user_value(req_font_obj, "flags")) {
				obs_data_set_int(font_obj, "flags", obs_data_get_int(req_font_obj, "flags"));
			}

			if (obs_data_has_user_value(req_font_obj, "size")) {
				obs_data_set_int(font_obj, "size", obs_data_get_int(req_font_obj, "size"));
			}

			if (obs_data_has_user_value(req_font_obj, "style")) {
				obs_data_set_string(font_obj, "style", obs_data_get_string(req_font_obj, "style"));
			}
		}
	}

	if (req->hasField("gradient")) {
		obs_data_set_bool(settings, "gradient", obs_data_get_bool(req->data, "gradient"));
	}

	if (req->hasField("gradient_color")) {
		obs_data_set_int(settings, "gradient_color", obs_data_get_int(req->data, "gradient_color"));
	}

	if (req->hasField("gradient_dir")) {
		obs_data_set_double(settings, "gradient_dir", obs_data_get_double(req->data, "gradient_dir"));
	}

	if (req->hasField("gradient_opacity")) {
		obs_data_set_int(settings, "gradient_opacity", obs_data_get_int(req->data, "gradient_opacity"));
	}

	if (req->hasField("outline")) {
		obs_data_set_bool(settings, "outline", obs_data_get_bool(req->data, "outline"));
	}

	if (req->hasField("outline_size")) {
		obs_data_set_int(settings, "outline_size", obs_data_get_int(req->data, "outline_size"));
	}

	if (req->hasField("outline_color")) {
		obs_data_set_int(settings, "outline_color", obs_data_get_int(req->data, "outline_color"));
	}

	if (req->hasField("outline_opacity")) {
		obs_data_set_int(settings, "outline_opacity", obs_data_get_int(req->data, "outline_opacity"));
	}

	if (req->hasField("read_from_file")) {
		obs_data_set_bool(settings, "read_from_file", obs_data_get_bool(req->data, "read_from_file"));
	}

	if (req->hasField("text")) {
		obs_data_set_string(settings, "text", obs_data_get_string(req->data, "text"));
	}

	if (req->hasField("valign")) {
		obs_data_set_string(settings, "valign", obs_data_get_string(req->data, "valign"));
	}

	if (req->hasField("vertical")) {
		obs_data_set_bool(settings, "vertical", obs_data_get_bool(req->data, "vertical"));
	}

	obs_source_update(source, settings);

	return req->SendOKResponse();
}

/**
 * Get the current properties of a Text Freetype 2 source.
 *
 * @param {String} `source` Source name.
 *
 * @return {String} `source` Source name
 * @return {int} `color1` Gradient top color.
 * @return {int} `color2` Gradient bottom color.
 * @return {int} `custom_width` Custom width (0 to disable).
 * @return {boolean} `drop_shadow` Drop shadow.
 * @return {Object} `font` Holds data for the font. Ex: `"font": { "face": "Arial", "flags": 0, "size": 150, "style": "" }`
 * @return {String} `font.face` Font face.
 * @return {int} `font.flags` Font text styling flag. `Bold=1, Italic=2, Bold Italic=3, Underline=5, Strikeout=8`
 * @return {int} `font.size` Font text size.
 * @return {String} `font.style` Font Style (unknown function).
 * @return {boolean} `from_file` Read text from the specified file.
 * @return {boolean} `log_mode` Chat log.
 * @return {boolean} `outline` Outline.
 * @return {String} `text` Text content to be displayed.
 * @return {String} `text_file` File path.
 * @return {boolean} `word_wrap` Word wrap.
 *
 * @api requests
 * @name GetTextFreetype2Properties
 * @category sources
 * @since 4.5.0
 */
HandlerResponse WSRequestHandler::HandleGetTextFreetype2Properties(WSRequestHandler* req)
{
	const char* sourceName = obs_data_get_string(req->data, "source");
	if (!sourceName) {
		return req->SendErrorResponse("invalid request parameters");
	}

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		return req->SendErrorResponse("specified source doesn't exist");
	}

	QString sourceId = obs_source_get_id(source);
	if (sourceId != "text_ft2_source") {
		return req->SendErrorResponse("not a freetype 2 source");
	}

	OBSDataAutoRelease response = obs_source_get_settings(source);
	obs_data_set_string(response, "source", sourceName);

	return req->SendOKResponse(response);
}

/**
 * Set the current properties of a Text Freetype 2 source.
 *
 * @param {String} `source` Source name.
 * @param {int (optional)} `color1` Gradient top color.
 * @param {int (optional)} `color2` Gradient bottom color.
 * @param {int (optional)} `custom_width` Custom width (0 to disable).
 * @param {boolean (optional)} `drop_shadow` Drop shadow.
 * @param {Object (optional)} `font` Holds data for the font. Ex: `"font": { "face": "Arial", "flags": 0, "size": 150, "style": "" }`
 * @param {String (optional)} `font.face` Font face.
 * @param {int (optional)} `font.flags` Font text styling flag. `Bold=1, Italic=2, Bold Italic=3, Underline=5, Strikeout=8`
 * @param {int (optional)} `font.size` Font text size.
 * @param {String (optional)} `font.style` Font Style (unknown function).
 * @param {boolean (optional)} `from_file` Read text from the specified file.
 * @param {boolean (optional)} `log_mode` Chat log.
 * @param {boolean (optional)} `outline` Outline.
 * @param {String (optional)} `text` Text content to be displayed.
 * @param {String (optional)} `text_file` File path.
 * @param {boolean (optional)} `word_wrap` Word wrap.
 *
 * @api requests
 * @name SetTextFreetype2Properties
 * @category sources
 * @since 4.5.0
 */
HandlerResponse WSRequestHandler::HandleSetTextFreetype2Properties(WSRequestHandler* req)
{
    const char* sourceName = obs_data_get_string(req->data, "source");
    if (!sourceName) {
		return req->SendErrorResponse("invalid request parameters");
    }

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		return req->SendErrorResponse("specified source doesn't exist");
	}

	QString sourceId = obs_source_get_id(source);
	if (sourceId != "text_ft2_source") {
		return req->SendErrorResponse("not text freetype 2 source");
	}

	OBSDataAutoRelease settings = obs_source_get_settings(source);

	if (req->hasField("color1")) {
		obs_data_set_int(settings, "color1", obs_data_get_int(req->data, "color1"));
	}

	if (req->hasField("color2")) {
		obs_data_set_int(settings, "color2", obs_data_get_int(req->data, "color2"));
	}

	if (req->hasField("custom_width")) {
		obs_data_set_int(settings, "custom_width", obs_data_get_int(req->data, "custom_width"));
	}

	if (req->hasField("drop_shadow")) {
		obs_data_set_bool(settings, "drop_shadow", obs_data_get_bool(req->data, "drop_shadow"));
	}

	if (req->hasField("font")) {
		OBSDataAutoRelease font_obj = obs_data_get_obj(settings, "font");
		if (font_obj) {
			OBSDataAutoRelease req_font_obj = obs_data_get_obj(req->data, "font");

			if (obs_data_has_user_value(req_font_obj, "face")) {
				obs_data_set_string(font_obj, "face", obs_data_get_string(req_font_obj, "face"));
			}

			if (obs_data_has_user_value(req_font_obj, "flags")) {
				obs_data_set_int(font_obj, "flags", obs_data_get_int(req_font_obj, "flags"));
			}

			if (obs_data_has_user_value(req_font_obj, "size")) {
				obs_data_set_int(font_obj, "size", obs_data_get_int(req_font_obj, "size"));
			}

			if (obs_data_has_user_value(req_font_obj, "style")) {
				obs_data_set_string(font_obj, "style", obs_data_get_string(req_font_obj, "style"));
			}
		}
	}

	if (req->hasField("from_file")) {
		obs_data_set_bool(settings, "from_file", obs_data_get_bool(req->data, "from_file"));
	}

	if (req->hasField("log_mode")) {
		obs_data_set_bool(settings, "log_mode", obs_data_get_bool(req->data, "log_mode"));
	}

	if (req->hasField("outline")) {
		obs_data_set_bool(settings, "outline", obs_data_get_bool(req->data, "outline"));
	}

	if (req->hasField("text")) {
		obs_data_set_string(settings, "text", obs_data_get_string(req->data, "text"));
	}

	if (req->hasField("text_file")) {
		obs_data_set_string(settings, "text_file", obs_data_get_string(req->data, "text_file"));
	}

	if (req->hasField("word_wrap")) {
		obs_data_set_bool(settings, "word_wrap", obs_data_get_bool(req->data, "word_wrap"));
	}

	obs_source_update(source, settings);

	return req->SendOKResponse();
}

/**
 * Get current properties for a Browser Source.
 *
 * @param {String} `source` Source name.
 *
 * @return {String} `source` Source name.
 * @return {boolean} `is_local_file` Indicates that a local file is in use.
 * @return {String} `local_file` file path.
 * @return {String} `url` Url.
 * @return {String} `css` CSS to inject.
 * @return {int} `width` Width.
 * @return {int} `height` Height.
 * @return {int} `fps` Framerate.
 * @return {boolean} `shutdown` Indicates whether the source should be shutdown when not visible.
 *
 * @api requests
 * @name GetBrowserSourceProperties
 * @category sources
 * @since 4.1.0
 */
HandlerResponse WSRequestHandler::HandleGetBrowserSourceProperties(WSRequestHandler* req)
{
	const char* sourceName = obs_data_get_string(req->data, "source");
	if (!sourceName) {
		return req->SendErrorResponse("invalid request parameters");
	}

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		return req->SendErrorResponse("specified source doesn't exist");
	}

	QString sourceId = obs_source_get_id(source);
	if (sourceId != "browser_source" && sourceId != "linuxbrowser-source") {
		return req->SendErrorResponse("not a browser source");
	}

	OBSDataAutoRelease response = obs_source_get_settings(source);
	obs_data_set_string(response, "source", obs_source_get_name(source));

	return req->SendOKResponse(response);
}

/**
 * Set current properties for a Browser Source.
 *
 * @param {String} `source` Name of the source.
 * @param {boolean (optional)} `is_local_file` Indicates that a local file is in use.
 * @param {String (optional)} `local_file` file path.
 * @param {String (optional)} `url` Url.
 * @param {String (optional)} `css` CSS to inject.
 * @param {int (optional)} `width` Width.
 * @param {int (optional)} `height` Height.
 * @param {int (optional)} `fps` Framerate.
 * @param {boolean (optional)} `shutdown` Indicates whether the source should be shutdown when not visible.
 * @param {boolean (optional)} `render` Visibility of the scene item.
 *
 * @api requests
 * @name SetBrowserSourceProperties
 * @category sources
 * @since 4.1.0
 */
HandlerResponse WSRequestHandler::HandleSetBrowserSourceProperties(WSRequestHandler* req)
{
	if (!req->hasField("source")) {
		return req->SendErrorResponse("missing request parameters");
	}

	const char* sourceName = obs_data_get_string(req->data, "source");
	if (!sourceName) {
		return req->SendErrorResponse("invalid request parameters");
	}

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		return req->SendErrorResponse("specified source doesn't exist");
	}

	QString sourceId = obs_source_get_id(source);
	if(sourceId != "browser_source" && sourceId != "linuxbrowser-source") {
		return req->SendErrorResponse("not a browser source");
	}

	OBSDataAutoRelease settings = obs_source_get_settings(source);

	if (req->hasField("restart_when_active")) {
		obs_data_set_bool(settings, "restart_when_active", obs_data_get_bool(req->data, "restart_when_active"));
	}

	if (req->hasField("shutdown")) {
		obs_data_set_bool(settings, "shutdown", obs_data_get_bool(req->data, "shutdown"));
	}

	if (req->hasField("is_local_file")) {
		obs_data_set_bool(settings, "is_local_file", obs_data_get_bool(req->data, "is_local_file"));
	}

	if (req->hasField("local_file")) {
		obs_data_set_string(settings, "local_file", obs_data_get_string(req->data, "local_file"));
	}

	if (req->hasField("url")) {
		obs_data_set_string(settings, "url", obs_data_get_string(req->data, "url"));
	}

	if (req->hasField("css")) {
		obs_data_set_string(settings, "css", obs_data_get_string(req->data, "css"));
	}

	if (req->hasField("width")) {
		obs_data_set_int(settings, "width", obs_data_get_int(req->data, "width"));
	}

	if (req->hasField("height")) {
		obs_data_set_int(settings, "height", obs_data_get_int(req->data, "height"));
	}

	if (req->hasField("fps")) {
		obs_data_set_int(settings, "fps", obs_data_get_int(req->data, "fps"));
	}

	obs_source_update(source, settings);

	return req->SendOKResponse();
}

/**
 * Get configured special sources like Desktop Audio and Mic/Aux sources.
 *
 * @return {String (optional)} `desktop-1` Name of the first Desktop Audio capture source.
 * @return {String (optional)} `desktop-2` Name of the second Desktop Audio capture source.
 * @return {String (optional)} `mic-1` Name of the first Mic/Aux input source.
 * @return {String (optional)} `mic-2` Name of the second Mic/Aux input source.
 * @return {String (optional)} `mic-3` NAme of the third Mic/Aux input source.
 *
 * @api requests
 * @name GetSpecialSources
 * @category sources
 * @since 4.1.0
 */
HandlerResponse WSRequestHandler::HandleGetSpecialSources(WSRequestHandler* req)
 {
	OBSDataAutoRelease response = obs_data_create();

	QMap<const char*, int> sources;
	sources["desktop-1"] = 1;
	sources["desktop-2"] = 2;
	sources["mic-1"] = 3;
	sources["mic-2"] = 4;
	sources["mic-3"] = 5;

	QMapIterator<const char*, int> i(sources);
	while (i.hasNext()) {
		i.next();

		const char* id = i.key();
		OBSSourceAutoRelease source = obs_get_output_source(i.value());
		if (source) {
			obs_data_set_string(response, id, obs_source_get_name(source));
		}
	}

	return req->SendOKResponse(response);
}

/**
* List filters applied to a source
*
* @param {String} `sourceName` Source name
*
* @return {Array<Object>} `filters` List of filters for the specified source
* @return {String} `filters.*.type` Filter type
* @return {String} `filters.*.name` Filter name
* @return {Object} `filters.*.settings` Filter settings
*
* @api requests
* @name GetSourceFilters
* @category sources
* @since 4.5.0
*/
HandlerResponse WSRequestHandler::HandleGetSourceFilters(WSRequestHandler* req)
{
	if (!req->hasField("sourceName")) {
		return req->SendErrorResponse("missing request parameters");
	}

	const char* sourceName = obs_data_get_string(req->data, "sourceName");
	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		return req->SendErrorResponse("specified source doesn't exist");
	}

	OBSDataArrayAutoRelease filters = Utils::GetSourceFiltersList(source, true);

	OBSDataAutoRelease response = obs_data_create();
	obs_data_set_array(response, "filters", filters);
	return req->SendOKResponse(response);
}

/**
* Add a new filter to a source. Available source types along with their settings properties are available from `GetSourceTypesList`.
*
* @param {String} `sourceName` Name of the source on which the filter is added
* @param {String} `filterName` Name of the new filter
* @param {String} `filterType` Filter type
* @param {Object} `filterSettings` Filter settings
*
* @api requests
* @name AddFilterToSource
* @category sources
* @since 4.5.0
*/
HandlerResponse WSRequestHandler::HandleAddFilterToSource(WSRequestHandler* req)
{
	if (!req->hasField("sourceName") || !req->hasField("filterName") ||
		!req->hasField("filterType") || !req->hasField("filterSettings"))
	{
		return req->SendErrorResponse("missing request parameters");
	}

	const char* sourceName = obs_data_get_string(req->data, "sourceName");
	const char* filterName = obs_data_get_string(req->data, "filterName");
	const char* filterType = obs_data_get_string(req->data, "filterType");
	OBSDataAutoRelease filterSettings = obs_data_get_obj(req->data, "filterSettings");

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		return req->SendErrorResponse("specified source doesn't exist");
	}

	OBSSourceAutoRelease existingFilter = obs_source_get_filter_by_name(source, filterName);
	if (existingFilter) {
		return req->SendErrorResponse("filter name already taken");
	}

	OBSSourceAutoRelease filter = obs_source_create_private(filterType, filterName, filterSettings);
	if (!filter) {
		return req->SendErrorResponse("filter creation failed");
	}
	if (obs_source_get_type(filter) != OBS_SOURCE_TYPE_FILTER) {
		return req->SendErrorResponse("invalid filter type");
	}

	obs_source_filter_add(source, filter);

	return req->SendOKResponse();
}

/**
* Remove a filter from a source
*
* @param {String} `sourceName` Name of the source from which the specified filter is removed
* @param {String} `filterName` Name of the filter to remove
*
* @api requests
* @name RemoveFilterFromSource
* @category sources
* @since 4.5.0
*/
HandlerResponse WSRequestHandler::HandleRemoveFilterFromSource(WSRequestHandler* req)
{
	if (!req->hasField("sourceName") || !req->hasField("filterName")) {
		return req->SendErrorResponse("missing request parameters");
	}

	const char* sourceName = obs_data_get_string(req->data, "sourceName");
	const char* filterName = obs_data_get_string(req->data, "filterName");

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		return req->SendErrorResponse("specified source doesn't exist");
	}

	OBSSourceAutoRelease filter = obs_source_get_filter_by_name(source, filterName);
	if (!filter) {
		return req->SendErrorResponse("specified filter doesn't exist");
	}

	obs_source_filter_remove(source, filter);

	return req->SendOKResponse();
}

/**
* Move a filter in the chain (absolute index positioning)
*
* @param {String} `sourceName` Name of the source to which the filter belongs
* @param {String} `filterName` Name of the filter to reorder
* @param {Integer} `newIndex` Desired position of the filter in the chain
*
* @api requests
* @name ReorderSourceFilter
* @category sources
* @since 4.5.0
*/
HandlerResponse WSRequestHandler::HandleReorderSourceFilter(WSRequestHandler* req)
{
	if (!req->hasField("sourceName") || !req->hasField("filterName") || !req->hasField("newIndex")) {
		return req->SendErrorResponse("missing request parameters");
	}

	const char* sourceName = obs_data_get_string(req->data, "sourceName");
	const char* filterName = obs_data_get_string(req->data, "filterName");
	int newIndex = obs_data_get_int(req->data, "newIndex");

	if (newIndex < 0) {
		return req->SendErrorResponse("invalid index");
	}

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		return req->SendErrorResponse("specified source doesn't exist");
	}

	OBSSourceAutoRelease filter = obs_source_get_filter_by_name(source, filterName);
	if (!filter) {
		return req->SendErrorResponse("specified filter doesn't exist");
	}

	struct filterSearch {
		int i;
		int filterIndex;
		obs_source_t* filter;
	};
	struct filterSearch ctx = { 0, 0, filter };
	obs_source_enum_filters(source, [](obs_source_t *parent, obs_source_t *child, void *param)
	{
		struct filterSearch* ctx = (struct filterSearch*)param;
		if (child == ctx->filter) {
			ctx->filterIndex = ctx->i;
		}
		ctx->i++;
	}, &ctx);

	int lastFilterIndex = ctx.i + 1;
	if (newIndex > lastFilterIndex) {
		return req->SendErrorResponse("index out of bounds");
	}

	int currentIndex = ctx.filterIndex;
	if (newIndex > currentIndex) {
		int downSteps = newIndex - currentIndex;
		for (int i = 0; i < downSteps; i++) {
			obs_source_filter_set_order(source, filter, OBS_ORDER_MOVE_DOWN);
		}
	}
	else if (newIndex < currentIndex) {
		int upSteps = currentIndex - newIndex;
		for (int i = 0; i < upSteps; i++) {
			obs_source_filter_set_order(source, filter, OBS_ORDER_MOVE_UP);
		}
	}

	return req->SendOKResponse();
}

/**
* Move a filter in the chain (relative positioning)
*
* @param {String} `sourceName` Name of the source to which the filter belongs
* @param {String} `filterName` Name of the filter to reorder
* @param {String} `movementType` How to move the filter around in the source's filter chain. Either "up", "down", "top" or "bottom".
*
* @api requests
* @name MoveSourceFilter
* @category sources
* @since 4.5.0
*/
HandlerResponse WSRequestHandler::HandleMoveSourceFilter(WSRequestHandler* req)
{
	if (!req->hasField("sourceName") || !req->hasField("filterName") || !req->hasField("movementType")) {
		return req->SendErrorResponse("missing request parameters");
	}

	const char* sourceName = obs_data_get_string(req->data, "sourceName");
	const char* filterName = obs_data_get_string(req->data, "filterName");
	QString movementType(obs_data_get_string(req->data, "movementType"));

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		return req->SendErrorResponse("specified source doesn't exist");
	}

	OBSSourceAutoRelease filter = obs_source_get_filter_by_name(source, filterName);
	if (!filter) {
		return req->SendErrorResponse("specified filter doesn't exist");
	}

	obs_order_movement movement;
	if (movementType == "up") {
		movement = OBS_ORDER_MOVE_UP;
	}
	else if (movementType == "down") {
		movement = OBS_ORDER_MOVE_DOWN;
	}
	else if (movementType == "top") {
		movement = OBS_ORDER_MOVE_TOP;
	}
	else if (movementType == "bottom") {
		movement = OBS_ORDER_MOVE_BOTTOM;
	}
	else {
		return req->SendErrorResponse("invalid value for movementType: must be either 'up', 'down', 'top' or 'bottom'.");
	}

	obs_source_filter_set_order(source, filter, movement);

	return req->SendOKResponse();
}

/**
* Update settings of a filter
*
* @param {String} `sourceName` Name of the source to which the filter belongs
* @param {String} `filterName` Name of the filter to reconfigure
* @param {Object} `filterSettings` New settings. These will be merged to the current filter settings.
*
* @api requests
* @name SetSourceFilterSettings
* @category sources
* @since 4.5.0
*/
HandlerResponse WSRequestHandler::HandleSetSourceFilterSettings(WSRequestHandler* req)
{
	if (!req->hasField("sourceName") || !req->hasField("filterName") || !req->hasField("filterSettings")) {
		return req->SendErrorResponse("missing request parameters");
	}

	const char* sourceName = obs_data_get_string(req->data, "sourceName");
	const char* filterName = obs_data_get_string(req->data, "filterName");
	OBSDataAutoRelease newFilterSettings = obs_data_get_obj(req->data, "filterSettings");

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		return req->SendErrorResponse("specified source doesn't exist");
	}

	OBSSourceAutoRelease filter = obs_source_get_filter_by_name(source, filterName);
	if (!filter) {
		return req->SendErrorResponse("specified filter doesn't exist");
	}

	OBSDataAutoRelease settings = obs_source_get_settings(filter);
	obs_data_apply(settings, newFilterSettings);
	obs_source_update(filter, settings);

	return req->SendOKResponse();
}

/**
* Takes a picture snapshot of a source and then can either or both:
*    - Send it over as a Data URI (base64-encoded data) in the response (by specifying `embedPictureFormat` in the request)
*    - Save it to disk (by specifying `saveToFilePath` in the request)
*
* At least `embedPictureFormat` or `saveToFilePath` must be specified.
*
* Clients can specify `width` and `height` parameters to receive scaled pictures. Aspect ratio is
* preserved if only one of these two parameters is specified.
*
* @param {String} `sourceName` Source name
* @param {String (optional)} `embedPictureFormat` Format of the Data URI encoded picture. Can be "png", "jpg", "jpeg" or "bmp" (or any other value supported by Qt's Image module)
* @param {String (optional)} `saveToFilePath` Full file path (file extension included) where the captured image is to be saved. Can be in a format different from `pictureFormat`. Can be a relative path.
* @param {int (optional)} `width` Screenshot width. Defaults to the source's base width.
* @param {int (optional)} `height` Screenshot height. Defaults to the source's base height.
*
* @return {String} `sourceName` Source name
* @return {String} `img` Image Data URI (if `embedPictureFormat` was specified in the request)
* @return {String} `imageFile` Absolute path to the saved image file (if `saveToFilePath` was specified in the request)
*
* @api requests
* @name TakeSourceScreenshot
* @category sources
* @since 4.6.0
*/
HandlerResponse WSRequestHandler::HandleTakeSourceScreenshot(WSRequestHandler* req) {
	if (!req->hasField("sourceName")) {
		return req->SendErrorResponse("missing request parameters");
	}

	if (!req->hasField("embedPictureFormat") && !req->hasField("saveToFilePath")) {
		return req->SendErrorResponse("At least 'embedPictureFormat' or 'saveToFilePath' must be specified");
	}

	const char* sourceName = obs_data_get_string(req->data, "sourceName");
	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		return req->SendErrorResponse("specified source doesn't exist");;
	}

	const uint32_t sourceWidth = obs_source_get_base_width(source);
	const uint32_t sourceHeight = obs_source_get_base_height(source);
	const double sourceAspectRatio = ((double)sourceWidth / (double)sourceHeight);

	uint32_t imgWidth = sourceWidth;
	uint32_t imgHeight = sourceHeight;

	if (req->hasField("width")) {
		imgWidth = obs_data_get_int(req->data, "width");

		if (!req->hasField("height")) {
			imgHeight = ((double)imgWidth / sourceAspectRatio);
		}
	}

	if (req->hasField("height")) {
		imgHeight = obs_data_get_int(req->data, "height");

		if (!req->hasField("width")) {
			imgWidth = ((double)imgHeight * sourceAspectRatio);
		}
	}

	QImage sourceImage(imgWidth, imgHeight, QImage::Format::Format_RGBA8888);
	sourceImage.fill(0);

	uint8_t* videoData = nullptr;
	uint32_t videoLinesize = 0;

	obs_enter_graphics();

	gs_texrender_t* texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	gs_stagesurf_t* stagesurface = gs_stagesurface_create(imgWidth, imgHeight, GS_RGBA);

	bool renderSuccess = false;
	gs_texrender_reset(texrender);
	if (gs_texrender_begin(texrender, imgWidth, imgHeight)) {
		vec4 background;
		vec4_zero(&background);

		gs_clear(GS_CLEAR_COLOR, &background, 0.0f, 0);
		gs_ortho(0.0f, (float)sourceWidth, 0.0f, (float)sourceHeight, -100.0f, 100.0f);

		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

		obs_source_inc_showing(source);
		obs_source_video_render(source);
		obs_source_dec_showing(source);

		gs_blend_state_pop();
		gs_texrender_end(texrender);

		gs_stage_texture(stagesurface, gs_texrender_get_texture(texrender));
		if (gs_stagesurface_map(stagesurface, &videoData, &videoLinesize)) {
			int linesize = sourceImage.bytesPerLine();
			for (int y = 0; y < imgHeight; y++) {
			 	memcpy(sourceImage.scanLine(y), videoData + (y * videoLinesize), linesize);
			}
			gs_stagesurface_unmap(stagesurface);
			renderSuccess = true;
		}
	}

	gs_stagesurface_destroy(stagesurface);
	gs_texrender_destroy(texrender);

	obs_leave_graphics();

	if (!renderSuccess) {
		return req->SendErrorResponse("Source render failed");
	}

	OBSDataAutoRelease response = obs_data_create();

	if (req->hasField("embedPictureFormat")) {
		const char* pictureFormat = obs_data_get_string(req->data, "embedPictureFormat");

		QByteArrayList supportedFormats = QImageWriter::supportedImageFormats();
		if (!supportedFormats.contains(pictureFormat)) {
			QString errorMessage = QString("Unsupported picture format: %1").arg(pictureFormat);
			return req->SendErrorResponse(errorMessage.toUtf8());
		}

		QByteArray encodedImgBytes;
		QBuffer buffer(&encodedImgBytes);
		buffer.open(QBuffer::WriteOnly);
		if (!sourceImage.save(&buffer, pictureFormat)) {
			return req->SendErrorResponse("Embed image encoding failed");
		}
		buffer.close();

		QString imgBase64(encodedImgBytes.toBase64());
		imgBase64.prepend(
			QString("data:image/%1;base64,").arg(pictureFormat)
		);

		obs_data_set_string(response, "img", imgBase64.toUtf8());
	}

	if (req->hasField("saveToFilePath")) {
		QString filePathStr = obs_data_get_string(req->data, "saveToFilePath");
		QFileInfo filePathInfo(filePathStr);
		QString absoluteFilePath = filePathInfo.absoluteFilePath();

		if (!sourceImage.save(absoluteFilePath)) {
			return req->SendErrorResponse("Image save failed");
		}
		obs_data_set_string(response, "imageFile", absoluteFilePath.toUtf8());
	}

	obs_data_set_string(response, "sourceName", obs_source_get_name(source));
	return req->SendOKResponse(response);
}
