// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/message_history/history_item.h"

#include "history/history_item.h"
#include "api/api_text_entities.h"
#include "ayu/data/entities.h"
#include "ayu/ui/message_history/history_inner.h"
#include "ayu/utils/ayu_mapper.h"
#include "base/unixtime.h"
#include "core/application.h"
#include "core/click_handler_types.h"
#include "core/file_location.h"
#include "core/version.h"
#include "data/data_channel.h"
#include "data/data_document.h"
#include "data/data_file_origin.h"
#include "data/data_forum_topic.h"
#include "data/data_photo.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/view/history_view_element.h"
#include "storage/serialize_document.h"
#include "ui/basic_click_handlers.h"
#include "ui/image/image_location.h"
#include "ui/text/text_utilities.h"

#include <QBuffer>
#include <QDataStream>
#include <QFileInfo>

namespace MessageHistory {

OwnedItem::OwnedItem(std::nullptr_t) {
}

OwnedItem::OwnedItem(
	not_null<HistoryView::ElementDelegate*> delegate,
	not_null<HistoryItem*> data)
	: _data(data), _view(_data->createView(delegate)) {
}

OwnedItem::OwnedItem(OwnedItem &&other)
	: _data(base::take(other._data)), _view(base::take(other._view)) {
}

OwnedItem &OwnedItem::operator=(OwnedItem &&other) {
	_data = base::take(other._data);
	_view = base::take(other._view);
	return *this;
}

OwnedItem::~OwnedItem() {
	clearView();
	if (_data) {
		_data->destroy();
	}
}

void OwnedItem::refreshView(
	not_null<HistoryView::ElementDelegate*> delegate) {
	_view = _data->createView(delegate);
}

void OwnedItem::clearView() {
	_view = nullptr;
}

void GenerateItems(
	not_null<HistoryView::ElementDelegate*> delegate,
	not_null<History*> history,
	AyuMessageBase message,
	Fn<void(OwnedItem item, TimeId sentDate, MsgId)> callback) {
	PeerData *from = history->owner().userLoaded(message.fromId);
	if (!from) {
		from = history->owner().channelLoaded(message.fromId);
	}
	if (!from) {
		from = reinterpret_cast<PeerData*>(history->owner().chatLoaded(message.fromId));
	}
	const auto date = message.entityCreateDate;
	const auto addPart = [&](
		not_null<HistoryItem*> item,
		TimeId sentDate = 0,
		MsgId realId = MsgId())
	{
		return callback(OwnedItem(delegate, item), sentDate, realId);
	};

	const auto makeFields = [&] {
		base::flags<MessageFlag> flags = MessageFlag::AdminLogEntry;
		if (from) {
			flags |= MessageFlag::HasFromId;
		} else {
			flags |= MessageFlag::HasPostAuthor;
		}
		if (!message.postAuthor.empty()) {
			flags |= MessageFlag::HasPostAuthor;
		}
		return HistoryItemCommonFields{
			.id = history->nextNonHistoryEntryId(),
			.flags = flags,
			.from = from ? from->id : 0,
			.date = date,
			.postAuthor = !message.postAuthor.empty()
				? QString::fromStdString(message.postAuthor)
				: from
				? QString()
				: QString("unknown user: %1").arg(message.fromId),
		};
	};

	const auto makeSimpleTextMessage = [&](TextWithEntities &&text)
	{
		return history->makeMessage(
										makeFields(),
									std::move(text),
									MTP_messageMediaEmpty());
	};

	const auto addSimpleTextMessage = [&](TextWithEntities &&text)
	{
		addPart(makeSimpleTextMessage(std::move(text)));
	};

	const auto text = QString::fromStdString(message.text);
	auto textAndEntities = Ui::Text::WithEntities(text);
	const auto entities = AyuMapper::deserializeTextWithEntities(message.textEntities);
	textAndEntities.entities = Api::EntitiesFromMTP(&history->session(), entities.v);
	const auto mediaPath = QString::fromStdString(message.mediaPath);
	const auto hasLocalMedia = QFileInfo(mediaPath).isFile();
	if (message.documentType == int(AyuMessageMediaType::Document)
			&& !message.documentSerialized.empty()) {
		auto serialized = QByteArray(
			message.documentSerialized.data(),
			int(message.documentSerialized.size()));
		auto buffer = QBuffer(&serialized);
		if (buffer.open(QIODevice::ReadOnly)) {
			auto stream = QDataStream(&buffer);
			if (const auto document = Serialize::Document::readFromStream(
					&history->session(),
					AppVersion,
					stream);
				document && stream.status() == QDataStream::Ok) {
				if (hasLocalMedia) {
					document->setLocation(Core::FileLocation(mediaPath));
				}
				addPart(history->makeMessage(
					makeFields(),
					not_null<DocumentData*>(document),
					textAndEntities));
				return;
			}
		}
	} else if (message.documentType == int(AyuMessageMediaType::Photo)
			&& !message.documentSerialized.empty()
			&& !message.documentAttributesSerialized.empty()
			&& !message.thumbsSerialized.empty()) {
		auto documentSerialized = QByteArray(
			message.documentSerialized.data(),
			int(message.documentSerialized.size()));
		auto documentBuffer = QBuffer(&documentSerialized);
		if (documentBuffer.open(QIODevice::ReadOnly)) {
			auto documentStream = QDataStream(&documentBuffer);
			qint64 photoId = 0;
			qint64 accessHash = 0;
			QByteArray fileReference;
			documentStream >> photoId >> accessHash >> fileReference;
			if (documentStream.status() == QDataStream::Ok && photoId > 0) {
				auto attributesSerialized = QByteArray(
					message.documentAttributesSerialized.data(),
					int(message.documentAttributesSerialized.size()));
				auto attributesBuffer = QBuffer(&attributesSerialized);
				if (attributesBuffer.open(QIODevice::ReadOnly)) {
					auto attributesStream = QDataStream(&attributesBuffer);
					qint32 photoDate = 0;
					qint32 dc = 0;
					qint32 hasAttachedStickers = 0;
					qint64 videoStartPosition = 0;
					attributesStream >> photoDate >> dc
						>> hasAttachedStickers >> videoStartPosition;
					if (attributesStream.status() == QDataStream::Ok) {
						auto thumbsSerialized = QByteArray(
							message.thumbsSerialized.data(),
							int(message.thumbsSerialized.size()));
						auto thumbsBuffer = QBuffer(&thumbsSerialized);
						if (thumbsBuffer.open(QIODevice::ReadOnly)) {
							auto thumbsStream = QDataStream(&thumbsBuffer);
							QByteArray inlineThumbnailBytes;
							thumbsStream >> inlineThumbnailBytes;
							const auto readImage = [&](ImageWithLocation &image) {
								QByteArray location;
								qint32 bytesCount = 0;
								thumbsStream >> location >> bytesCount;
								if (const auto parsed = ImageLocation::FromSerialized(
										location)) {
									image.location = *parsed;
								}
								image.bytesCount = std::max(bytesCount, 0);
							};
							ImageWithLocation small;
							ImageWithLocation thumbnail;
							ImageWithLocation large;
							ImageWithLocation videoSmall;
							ImageWithLocation videoLarge;
							readImage(small);
							readImage(thumbnail);
							readImage(large);
							readImage(videoSmall);
							readImage(videoLarge);
							if (thumbsStream.status() == QDataStream::Ok) {
								const auto photo = history->owner().photo(photoId);
								photo->setRemoteLocation(dc, accessHash, fileReference);
								photo->setFields(photoDate, (hasAttachedStickers != 0));
								photo->updateImages(
									inlineThumbnailBytes,
									small,
									thumbnail,
									large,
									videoSmall,
									videoLarge,
									videoStartPosition);
								if (hasLocalMedia) {
									photo->setLocation(Core::FileLocation(mediaPath));
								}
								addPart(history->makeMessage(
									makeFields(),
									photo,
									textAndEntities));
								return;
							}
						}
					}
				}
			}
		}
	}
	addSimpleTextMessage(std::move(textAndEntities));
}

} // namespace MessageHistory
