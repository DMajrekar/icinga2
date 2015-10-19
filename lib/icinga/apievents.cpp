/******************************************************************************
 * Icinga 2                                                                   *
 * Copyright (C) 2012-2015 Icinga Development Team (http://www.icinga.org)    *
 *                                                                            *
 * This program is free software; you can redistribute it and/or              *
 * modify it under the terms of the GNU General Public License                *
 * as published by the Free Software Foundation; either version 2             *
 * of the License, or (at your option) any later version.                     *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with this program; if not, write to the Free Software Foundation     *
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.             *
 ******************************************************************************/

#include "icinga/apievents.hpp"
#include "icinga/service.hpp"
#include "remote/eventqueue.hpp"
#include "base/initialize.hpp"
#include "base/serializer.hpp"

using namespace icinga;

INITIALIZE_ONCE(&ApiEvents::StaticInitialize);

void ApiEvents::StaticInitialize(void)
{
	Checkable::OnNewCheckResult.connect(&ApiEvents::CheckResultHandler);
	Checkable::OnStateChange.connect(&ApiEvents::StateChangeHandler);
	Checkable::OnNotificationSentToAllUsers.connect(&ApiEvents::NotificationSentToAllUsersHandler);

	Checkable::OnFlappingChanged.connect(&ApiEvents::FlappingChangedHandler);

	Checkable::OnAcknowledgementSet.connect(&ApiEvents::AcknowledgementSetHandler);
	Checkable::OnAcknowledgementCleared.connect(&ApiEvents::AcknowledgementClearedHandler);

	Checkable::OnCommentAdded.connect(&ApiEvents::CommentAddedHandler);
	Checkable::OnCommentRemoved.connect(&ApiEvents::CommentRemovedHandler);

	Checkable::OnDowntimeAdded.connect(&ApiEvents::DowntimeAddedHandler);
	Checkable::OnDowntimeRemoved.connect(&ApiEvents::DowntimeRemovedHandler);
	Checkable::OnDowntimeTriggered.connect(&ApiEvents::DowntimeTriggeredHandler);
}

void ApiEvents::CheckResultHandler(const Checkable::Ptr& checkable, const CheckResult::Ptr& cr, const MessageOrigin::Ptr& origin)
{
	std::vector<EventQueue::Ptr> queues = EventQueue::GetQueuesForType("CheckResult");

	if (queues.empty())
		return;

	Log(LogDebug, "ApiEvents", "Processing event type 'CheckResult'.");

	Dictionary::Ptr result = new Dictionary();
	result->Set("type", "CheckResult");
	result->Set("timestamp", Utility::GetTime());

	Host::Ptr host;
	Service::Ptr service;
	tie(host, service) = GetHostService(checkable);

	result->Set("host", host->GetName());
	if (service)
		result->Set("service", service->GetShortName());

	result->Set("check_result", Serialize(cr));

	BOOST_FOREACH(const EventQueue::Ptr& queue, queues) {
		queue->ProcessEvent(result);
	}
}

void ApiEvents::StateChangeHandler(const Checkable::Ptr& checkable, const CheckResult::Ptr& cr, StateType type, const MessageOrigin::Ptr& origin)
{
	std::vector<EventQueue::Ptr> queues = EventQueue::GetQueuesForType("StateChange");

	if (queues.empty())
		return;

	Log(LogDebug, "ApiEvents", "Processing event type 'StateChange'.");

	Dictionary::Ptr result = new Dictionary();
	result->Set("type", "StateChange");
	result->Set("timestamp", Utility::GetTime());

	Host::Ptr host;
	Service::Ptr service;
	tie(host, service) = GetHostService(checkable);

	result->Set("host", host->GetName());
	if (service)
		result->Set("service", service->GetShortName());

	result->Set("state", service ? service->GetState() : host->GetState());
	result->Set("state_type", type);
	result->Set("check_result", Serialize(cr));

	BOOST_FOREACH(const EventQueue::Ptr& queue, queues) {
		queue->ProcessEvent(result);
	}
}

void ApiEvents::NotificationSentToAllUsersHandler(const Notification::Ptr& notification, const Checkable::Ptr& checkable, const std::set<User::Ptr>& users, NotificationType type,
    const CheckResult::Ptr& cr, const String& author, const String& text)
{
	/* TODO: implement. */
}

void ApiEvents::FlappingChangedHandler(const Checkable::Ptr& checkable, const MessageOrigin::Ptr& origin)
{
	/* TODO: implement. */
}

void ApiEvents::AcknowledgementSetHandler(const Checkable::Ptr& checkable,
    const String& author, const String& comment, AcknowledgementType type,
    bool notify, double expiry, const MessageOrigin::Ptr& origin)
{
	/* TODO: implement. */
}

void ApiEvents::AcknowledgementClearedHandler(const Checkable::Ptr& checkable, const MessageOrigin::Ptr& origin)
{
	/* TODO: implement. */
}

void ApiEvents::CommentAddedHandler(const Checkable::Ptr& checkable, const Comment::Ptr& comment, const MessageOrigin::Ptr& origin)
{
	/* TODO: implement. */
}

void ApiEvents::CommentRemovedHandler(const Checkable::Ptr& checkable, const Comment::Ptr& comment, const MessageOrigin::Ptr& origin)
{
	/* TODO: implement. */
}

void ApiEvents::DowntimeAddedHandler(const Checkable::Ptr& checkable, const Downtime::Ptr& downtime, const MessageOrigin::Ptr& origin)
{
	/* TODO: implement. */
}

void ApiEvents::DowntimeRemovedHandler(const Checkable::Ptr& checkable, const Downtime::Ptr& downtime, const MessageOrigin::Ptr& origin)
{
	/* TODO: implement. */
}

void ApiEvents::DowntimeTriggeredHandler(const Checkable::Ptr& checkable, const Downtime::Ptr& downtime)
{
	/* TODO: implement. */
}
