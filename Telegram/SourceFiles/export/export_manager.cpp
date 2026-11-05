/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "export/export_manager.h"

#include "export/export_controller.h"
#include "export/view/export_view_panel_controller.h"
#include "data/data_peer.h"
#include "main/main_session.h"
#include "main/main_account.h"
#include "ui/layers/box_content.h"
#include "base/unixtime.h"

namespace Export {

struct Manager::Task {
	std::unique_ptr<Controller> controller;
	std::unique_ptr<View::PanelController> panel;
};

Manager::Manager() = default;

Manager::~Manager() = default;

void Manager::start(not_null<PeerData*> peer) {
	start(&peer->session(), peer->input());
}

void Manager::startTopic(
		not_null<PeerData*> peer,
		MsgId topicRootId,
		const QString &topicTitle) {
	auto controller = std::make_unique<Controller>(
		&peer->session().mtp(),
		peer->input(),
		int32(topicRootId.bare),
		uint64(peer->id.value),
		topicTitle);
	setupPanel(&peer->session(), std::move(controller));
}

void Manager::start(
		not_null<Main::Session*> session,
		const MTPInputPeer &singlePeer) {
	auto controller = std::make_unique<Controller>(
		&session->mtp(), singlePeer);
	setupPanel(session, std::move(controller));
}

void Manager::setupPanel(
		not_null<Main::Session*> session,
		std::unique_ptr<Controller> controller) {
	auto task = std::make_unique<Task>();
	task->controller = std::move(controller);
	task->panel = std::make_unique<View::PanelController>(
		session, task->controller.get());
	const auto raw = task.get();
	_tasks.push_back(std::move(task));
	session->account().sessionChanges(
	) | rpl::filter([=](Main::Session *value) {
		return value != session;
	}) | rpl::on_next([=] {
		stopTask(raw);
	}, raw->panel->lifetime());
	_viewChanges.fire(raw->panel.get());
	raw->panel->stopRequests(
	) | rpl::on_next([=] {
		stopTask(raw);
	}, raw->controller->lifetime());
}

rpl::producer<View::PanelController*> Manager::currentView() const {
	return _viewChanges.events_starting_with(
		_tasks.empty() ? nullptr : _tasks.back()->panel.get());
}

bool Manager::inProgress() const {
	return !_tasks.empty();
}

bool Manager::inProgress(not_null<Main::Session*> session) const {
	return ranges::any_of(_tasks, [&](const auto &task) {
		return &task->panel->session() == session;
	});
}

void Manager::stopWithConfirmation(
		Fn<void()> callback,
		Main::Session *session) {
	const auto i = ranges::find_if(_tasks, [=](const auto &entry) {
		return !session || &entry->panel->session() == session;
	});
	if (i == end(_tasks)) {
		if (callback) {
			callback();
		}
		return;
	}
	const auto task = i->get();
	task->panel->stopWithConfirmation([=, callback = std::move(callback)]() mutable {
		stopTask(task);
		if (callback) {
			callback();
		}
	});
}

void Manager::stopTask(Task *task) {
	const auto i = ranges::find_if(_tasks, [=](const auto &entry) {
		return entry.get() == task;
	});
	if (i == end(_tasks)) {
		return;
	}
	_viewChanges.fire(nullptr);
	_tasks.erase(i);
	if (!_tasks.empty()) {
		_viewChanges.fire(_tasks.back()->panel.get());
	}
}

void Manager::stop() {
	_viewChanges.fire(nullptr);
	_tasks.clear();
}

} // namespace Export
