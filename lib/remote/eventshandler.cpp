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

#include "remote/eventshandler.hpp"
#include "remote/httputility.hpp"
#include "remote/filterutility.hpp"
#include "base/objectlock.hpp"
#include "base/json.hpp"
#include <boost/foreach.hpp>
#include <boost/algorithm/string/replace.hpp>

using namespace icinga;

REGISTER_URLHANDLER("/v1/events", EventsHandler);

bool EventsHandler::HandleRequest(const ApiUser::Ptr& user, HttpRequest& request, HttpResponse& response)
{
	if (request.RequestUrl->GetPath().size() != 2)
		return false;

	if (request.RequestMethod != "POST")
		return false;

	if (request.ProtocolVersion == HttpVersion10) {
		HttpUtility::SendJsonError(response, 400, "HTTP/1.0 not supported for event streams.");
		return true;
	}

	Dictionary::Ptr params = HttpUtility::FetchRequestParameters(request);

	Array::Ptr types = params->Get("types");

	if (types) {
		ObjectLock olock(types);
		BOOST_FOREACH(const String& type, types) {
			FilterUtility::CheckPermission(user, "events/" + type);
		}
	}

	String queueName = HttpUtility::GetLastParameter(params, "queue");

	if (queueName.IsEmpty()) {
		HttpUtility::SendJsonError(response, 400, "'queue' attribute is required.");
		return true;
	}

	// TODO: add ttl cache and fetch value from params
	double ttl = 0.0;
	// TODO: add filter
	Expression *filter = NULL;

	/* create a new queue or update an existing one */
	EventQueue::Ptr queue = EventQueue::GetByName(queueName);

	if (!queue) {
		queue = new EventQueue();
		EventQueue::Register(queueName, queue);
	}

	queue->SetTypes(types->ToSet<String>());
	queue->SetTtl(ttl);
	queue->SetFilter(filter);

	/* add http client to queue */
	queue->AddClient(&request);

	response.SetStatus(200, "OK");
	response.AddHeader("Content-Type", "application/json");

	for (;;) {
		Dictionary::Ptr result = queue->WaitForEvent(&request);

		// TODO: Check if client is still connected

		if (!result)
			continue;

		String body = JsonEncode(result);

		boost::algorithm::replace_all(body, "\n", "");

		try {
			response.WriteBody(body.CStr(), body.GetLength());
			response.WriteBody("\n", 1);
		} catch (const std::exception&) {
			queue->RemoveClient(&request);
			EventQueue::UnregisterIfUnused(queueName, queue);
			throw;
		}
	}
}

