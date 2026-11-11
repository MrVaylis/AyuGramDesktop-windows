// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/data/messages_storage.h"

#include "ayu/data/ayu_database.h"
#include "ayu/utils/ayu_mapper.h"
#include "ayu/utils/telegram_helpers.h"
#include "base/unixtime.h"
#include "core/file_location.h"
#include "data/data_document.h"
#include "data/data_file_origin.h"
#include "data/data_forum_topic.h"
#include "data/data_photo.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "main/main_session.h"
#include "storage/serialize_document.h"
#include "ui/image/image_location.h"

#include <QBuffer>
#include <QDataStream>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace AyuMessages {
namespace {

QString MakeMediaFilePath(
		const AyuMessageBase &message,
		const QString &fileName) {
	const auto directory = QDir::current().filePath(
		"tdata/ayu_message_history");
	if (!QDir().mkpath(directory)) {
		return QString();
	}
	auto name = QDate::currentDate().toString("yyyy.MM.dd")
		+ "_" + QString::number(message.userId)
		+ "_" + QString::number(message.dialogId)
		+ "_" + QString::number(message.messageId)
		+ "_" + QString::number(QDateTime::currentMSecsSinceEpoch());
	const auto suffix = QFileInfo(fileName).suffix();
	if (!suffix.isEmpty()) {
		name += "." + suffix;
	}
	const auto path = QDir(directory).filePath(name);
	return QFileInfo(path).absoluteFilePath();
}

rpl::lifetime &PhotoBackupLifetime() {
	static auto lifetime = rpl::lifetime();
	return lifetime;
}

void BackupMedia(not_null<HistoryItem*> item, const AyuMessageBase &message) {
	if (message.documentType == int(AyuMessageMediaType::None)) {
		return;
	}
	const auto media = item->media();
	if (!media) {
		return;
	}
	const auto origin = Data::FileOrigin(item->fullId());
	if (const auto document = media->document()) {
		const auto path = QString::fromStdString(message.mediaPath);
		if (!path.isEmpty() && !QFileInfo(path).isFile()) {
		document->save(origin, path, LoadFromCloudOrLocal, true);
		}
	} else if (const auto photo = media->photo()) {
		const auto path = QString::fromStdString(message.mediaPath);
		if (path.isEmpty() || QFileInfo(path).isFile()) {
			return;
		}
		const auto source = photo->location(true).name();
		if (QFileInfo(source).isFile()) {
			QFile::copy(source, path);
			return;
		}
		photo->session().data().photoLoadProgress()
		| rpl::filter([=](not_null<PhotoData*> loaded) {
			return loaded == photo && !photo->loading();
		})
		| rpl::take(1)
		| rpl::on_next([=](not_null<PhotoData*>) {
			const auto loadedPath = photo->location(true).name();
			if (QFileInfo(loadedPath).isFile()) {
				QFile::copy(loadedPath, path);
			}
		}, PhotoBackupLifetime());
		photo->load(
			Data::PhotoSize::Large,
			origin,
			LoadFromCloudOrLocal,
			true);
	}
}

std::vector<char> ToBytes(const QByteArray &data) {
	return std::vector<char>(data.begin(), data.end());
}

void mapMedia(not_null<HistoryItem*> item, AyuMessageBase &message) {
	const auto media = item->media();
	if (!media) {
		return;
	}
	if (const auto document = media->document()) {
		auto serialized = QByteArray();
		auto buffer = QBuffer(&serialized);
		if (buffer.open(QIODevice::WriteOnly)) {
			auto stream = QDataStream(&buffer);
			Serialize::Document::writeToStream(stream, document);
			if (stream.status() == QDataStream::Ok) {
				message.documentType = int(AyuMessageMediaType::Document);
				message.documentSerialized = ToBytes(serialized);
				message.mimeType = document->mimeString().toStdString();
				const auto path = MakeMediaFilePath(message, document->filename());
				if (!path.isEmpty()) {
					const auto source = document->filepath(true);
					if (QFileInfo(source).isFile()) {
						QFile::copy(source, path);
					}
					message.mediaPath = path.toStdString();
				}
			}
		}
		return;
	}
	if (const auto photo = media->photo()) {
		const auto input = photo->mtpInput();
		const auto &inputData = input.c_inputPhoto();
		auto documentSerialized = QByteArray();
		auto documentBuffer = QBuffer(&documentSerialized);
		if (documentBuffer.open(QIODevice::WriteOnly)) {
			auto stream = QDataStream(&documentBuffer);
			stream << qint64(inputData.vid().v)
				<< qint64(inputData.vaccess_hash().v)
				<< inputData.vfile_reference().v;
			if (stream.status() == QDataStream::Ok) {
				message.documentSerialized = ToBytes(documentSerialized);
			}
		}

		auto attributesSerialized = QByteArray();
		auto attributesBuffer = QBuffer(&attributesSerialized);
		if (attributesBuffer.open(QIODevice::WriteOnly)) {
			auto stream = QDataStream(&attributesBuffer);
			stream << qint32(photo->date())
				<< qint32(photo->getDC())
				<< qint32(photo->hasAttachedStickers())
				<< qint64(photo->videoStartPosition());
			if (stream.status() == QDataStream::Ok) {
				message.documentAttributesSerialized = ToBytes(
					attributesSerialized);
			}
		}

		auto thumbsSerialized = QByteArray();
		auto thumbsBuffer = QBuffer(&thumbsSerialized);
		if (thumbsBuffer.open(QIODevice::WriteOnly)) {
			auto stream = QDataStream(&thumbsBuffer);
			const auto writeImage = [&](
					Data::PhotoSize size,
					const ImageLocation &location,
					int bytesCount) {
				stream << location.serialize() << qint32(bytesCount);
			};
			stream << photo->inlineThumbnailBytes();
			writeImage(
				Data::PhotoSize::Small,
				photo->location(Data::PhotoSize::Small),
				photo->imageByteSize(Data::PhotoSize::Small));
			writeImage(
				Data::PhotoSize::Thumbnail,
				photo->location(Data::PhotoSize::Thumbnail),
				photo->imageByteSize(Data::PhotoSize::Thumbnail));
			writeImage(
				Data::PhotoSize::Large,
				photo->location(Data::PhotoSize::Large),
				photo->imageByteSize(Data::PhotoSize::Large));
			writeImage(
				Data::PhotoSize::Small,
				photo->videoLocation(Data::PhotoSize::Small),
				photo->videoByteSize(Data::PhotoSize::Small));
			writeImage(
				Data::PhotoSize::Large,
				photo->videoLocation(Data::PhotoSize::Large),
				photo->videoByteSize(Data::PhotoSize::Large));
			if (stream.status() == QDataStream::Ok) {
				message.thumbsSerialized = ToBytes(thumbsSerialized);
			}
		}

		if (!message.documentSerialized.empty()
				&& !message.documentAttributesSerialized.empty()
				&& !message.thumbsSerialized.empty()) {
			message.documentType = int(AyuMessageMediaType::Photo);
			message.mimeType = "image/jpeg";
			const auto source = photo->location(true).name();
			const auto path = MakeMediaFilePath(
				message,
				source.isEmpty() ? QString("photo.jpg") : source);
			if (!path.isEmpty()) {
				if (QFileInfo(source).isFile()) {
					QFile::copy(source, path);
				}
				message.mediaPath = path.toStdString();
			}
		}
	}
}

} // namespace

template<typename DerivedMessage>
std::vector<AyuMessageBase> convertToBase(const std::vector<DerivedMessage> &messages) {
	std::vector<AyuMessageBase> based;
	based.reserve(messages.size());
	for (const auto &msg : messages) {
		based.push_back(static_cast<AyuMessageBase>(msg));
	}
	return based;
}

void map(not_null<HistoryItem*> item, AyuMessageBase &message) {
	const ID userId = item->history()->owner().session().userId().bare & PeerId::kChatTypeMask;

	message.userId = userId;
	message.dialogId = getDialogIdFromPeer(item->history()->peer);
	message.groupedId = item->groupId().raw();
	message.peerId = item->history()->peer->id.value & PeerId::kChatTypeMask;
	message.fromId = item->from()->id.value & PeerId::kChatTypeMask;
	if (item->topic()) {
		message.topicId = item->topicRootId().bare;
	} else {
		message.topicId = 0;
	}
	message.messageId = item->id.bare;
	message.date = item->date();
	message.flags = AyuMapper::mapItemFlagsToMTPFlags(item);

	if (const auto edited = item->Get<HistoryMessageEdited>()) {
		message.editDate = edited->date;
	} else {
		message.editDate = base::unixtime::now();
	}

	message.views = item->viewsCount();
	message.fwdFlags = 0;
	message.fwdFromId = 0;
	// message.fwdName
	message.fwdDate = 0;
	// message.fwdPostAuthor
	if (const auto msgsigned = item->Get<HistoryMessageSigned>()) {
		message.postAuthor = msgsigned->author.toStdString();
	}
	message.replyFlags = 0;
	message.replyMessageId = 0;
	message.replyPeerId = 0;
	message.replyTopId = 0;
	message.replyForumTopic = false;
	// message.replySerialized
	// message.replyMarkupSerialized
	message.entityCreateDate = base::unixtime::now();

	auto serializedText = AyuMapper::serializeTextWithEntities(item);
	message.text = serializedText.first;
	message.textEntities = serializedText.second;
	message.documentType = int(AyuMessageMediaType::None);
	mapMedia(item, message);
}

void addEditedMessage(not_null<HistoryItem *> item) {
	EditedMessage message;
	map(item, message);

	if (message.text.empty()
		&& message.documentType == int(AyuMessageMediaType::None)) {
		return;
	}

	AyuDatabase::addEditedMessage(message);
	BackupMedia(item, message);
}

std::vector<AyuMessageBase> getEditedMessages(not_null<HistoryItem*> item, ID minId, ID maxId, int totalLimit) {
	const ID userId = item->history()->owner().session().userId().bare & PeerId::kChatTypeMask;
	const auto dialogId = getDialogIdFromPeer(item->history()->peer);
	const auto msgId = item->id.bare;

	return convertToBase(AyuDatabase::getEditedMessages(userId, dialogId, msgId, minId, maxId, totalLimit));
}

bool hasRevisions(not_null<HistoryItem*> item) {
	const ID userId = item->history()->owner().session().userId().bare & PeerId::kChatTypeMask;
	const auto dialogId = getDialogIdFromPeer(item->history()->peer);
	const auto msgId = item->id.bare;

	return AyuDatabase::hasRevisions(userId, dialogId, msgId);
}

void addDeletedMessage(not_null<HistoryItem*> item) {
	DeletedMessage message;
	map(item, message);

	if (message.text.empty()
		&& message.documentType == int(AyuMessageMediaType::None)) {
		return;
	}

	AyuDatabase::addDeletedMessage(message);
	BackupMedia(item, message);
}

std::vector<AyuMessageBase>
getDeletedMessages(not_null<PeerData*> peer, ID topicId, ID minId, ID maxId, int totalLimit, const QString &searchQuery) {
	const ID userId = peer->session().userId().bare & PeerId::kChatTypeMask;
	return convertToBase(
		AyuDatabase::getDeletedMessages(userId, getDialogIdFromPeer(peer), topicId, minId, maxId, totalLimit, searchQuery.toStdString()));
}

bool hasDeletedMessages(not_null<PeerData*> peer, ID topicId) {
	const ID userId = peer->session().userId().bare & PeerId::kChatTypeMask;
	return AyuDatabase::hasDeletedMessages(userId, getDialogIdFromPeer(peer), topicId);
}

void removeDeletedMessage(not_null<HistoryItem*> item) {
	const auto peer = item->history()->peer;
	const ID userId = peer->session().userId().bare & PeerId::kChatTypeMask;
	AyuDatabase::removeDeletedMessage(userId, getDialogIdFromPeer(peer), item->id.bare);
}

void clearDeletedMessages(not_null<PeerData*> peer, ID topicId) {
	const ID userId = peer->session().userId().bare & PeerId::kChatTypeMask;
	AyuDatabase::clearDeletedMessages(userId, getDialogIdFromPeer(peer), topicId);
}

}
