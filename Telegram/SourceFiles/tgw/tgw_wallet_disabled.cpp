/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "tgw/tgw_wallet.h"

#include "twidget/sdk_policy.h"

namespace Tgw {

UserData *QueryBot(not_null<HistoryItem*> item) {
	return nullptr;
}

void RunWalletCall(
		not_null<HistoryItem*> item,
		not_null<QWidget*> window,
		const twidget::WalletCall &call,
		Fn<void(const twidget::WidgetRequestOutcome&)> respond) {
	respond({ .failure = twidget::WidgetRequestFailure::Failed });
}

} // namespace Tgw
