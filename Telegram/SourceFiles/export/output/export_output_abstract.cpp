/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "export/output/export_output_abstract.h"

#include "export/output/export_output_html_and_json.h"
#include "export/output/export_output_html.h"
#include "export/output/export_output_json.h"
#include "export/output/export_output_file.h"
#include "export/output/export_output_result.h"
#include "export/data/export_data_types.h"

#include <QtCore/QDir>
#include <QtCore/QDate>
#include <QtCore/QFile>
#include <QtCore/QDateTime>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

namespace Export {
namespace Output {

Result WriteAvatarManifest(
		const Data::DialogInfo &dialog,
		const QString &rootPath,
		Stats *stats) {
	const auto path = rootPath + dialog.relativePath + "avatars/history.json";
	if (QFile::exists(path)) {
		return Result::Success();
	}
	auto history = QJsonArray();
	for (const auto &avatar : dialog.avatars) {
		auto item = QJsonObject();
		item.insert("installed_at_unix", int64(avatar.date));
		item.insert("installed_at", QDateTime::fromSecsSinceEpoch(
			avatar.date,
			Qt::UTC).toString(Qt::ISODate));
		item.insert("removed", avatar.removed);
		if (!avatar.removed) {
			item.insert("photo_id", QString::number(avatar.photo.id));
			item.insert("file", avatar.photo.image.file.relativePath);
			item.insert("width", avatar.photo.image.width);
			item.insert("height", avatar.photo.image.height);
		}
		history.append(item);
	}
	auto document = QJsonObject();
	document.insert("peer_id", QString::number(dialog.peerId.value));
	document.insert("chat_name", QString::fromUtf8(dialog.name));
	document.insert("history", history);
	return File(path, stats).writeBlock(
		QJsonDocument(document).toJson(QJsonDocument::Indented));
}

QString NormalizePath(const Settings &settings) {
	QDir folder(settings.path);
	const auto path = folder.absolutePath();
	auto result = path.endsWith('/') ? path : (path + '/');
	if (!folder.exists() && !settings.forceSubPath) {
		return result;
	}
	const auto mode = QDir::AllEntries | QDir::NoDotAndDotDot;
	const auto list = folder.entryInfoList(mode);
	if (list.isEmpty() && !settings.forceSubPath) {
		return result;
	}
	const auto date = QDate::currentDate();
	const auto base = QString(settings.onlySinglePeer()
		? "ChatExport_%1"
		: "DataExport_%1"
	).arg(date.toString(Qt::ISODate));
	const auto add = [&](int i) {
		return base + (i ? " (" + QString::number(i) + ')' : QString());
	};
	auto index = 0;
	while (!folder.mkdir(add(index))) {
		if (!QDir(result + add(index)).exists()) {
			break;
		}
		++index;
	}
	result += add(index) + '/';
	return result;
}

std::unique_ptr<AbstractWriter> CreateWriter(Format format) {
	switch (format) {
	case Format::Html: return std::make_unique<HtmlWriter>();
	case Format::Json: return std::make_unique<JsonWriter>();
	case Format::HtmlAndJson: return std::make_unique<HtmlAndJsonWriter>();
	}
	Unexpected("Format in Export::Output::CreateWriter.");
}

} // namespace Output
} // namespace Export
