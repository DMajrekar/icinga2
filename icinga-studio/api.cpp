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

#include "icinga-studio/api.hpp"
#include "remote/base64.hpp"
#include "base/json.hpp"
#include "base/logger.hpp"
#include "base/exception.hpp"
#include <boost/foreach.hpp>

using namespace icinga;

ApiClient::ApiClient(const String& host, const String& port,
    const String& user, const String& password)
    : m_Connection(new HttpClientConnection(host, port, true)), m_User(user), m_Password(password)
{
	m_Connection->Start();
}

void ApiClient::GetTypes(const TypesCompletionCallback& callback) const
{
	boost::shared_ptr<HttpRequest> req = m_Connection->NewRequest();
	req->RequestMethod = "GET";
	req->RequestUrl = new Url("https://" + m_Connection->GetHost() + ":" + m_Connection->GetPort() + "/v1/types");
	req->AddHeader("Authorization", "Basic " + Base64::Encode(m_User + ":" + m_Password));
	m_Connection->SubmitRequest(req, boost::bind(TypesHttpCompletionCallback, _1, _2, callback));
}

void ApiClient::TypesHttpCompletionCallback(HttpRequest& request, HttpResponse& response,
    const TypesCompletionCallback& callback)
{
	Dictionary::Ptr result;

	String body;
	char buffer[1024];
	size_t count;

	while ((count = response.ReadBody(buffer, sizeof(buffer))) > 0)
		body += String(buffer, buffer + count);

	std::vector<ApiType::Ptr> types;

	try {
		result = JsonDecode(body);

		Array::Ptr results = result->Get("results");

		ObjectLock olock(results);
		BOOST_FOREACH(const Dictionary::Ptr typeInfo, results)
		{
			ApiType::Ptr type = new ApiType();;
			type->Abstract = typeInfo->Get("abstract");
			type->BaseName = typeInfo->Get("base");
			type->Name = typeInfo->Get("name");
			type->PluralName = typeInfo->Get("plural_name");
			// TODO: attributes
			types.push_back(type);
		}
	} catch (const std::exception& ex) {
		Log(LogCritical, "ApiClient")
		    << "Error while decoding response: " << DiagnosticInformation(ex);
	}

	callback(types);
}

void ApiClient::GetObjects(const String& pluralType, const ObjectsCompletionCallback& callback,
    const std::vector<String>& names, const std::vector<String>& attrs) const
{
	String url = "https://" + m_Connection->GetHost() + ":" + m_Connection->GetPort() + "/v1/" + pluralType;
	String qp;

	BOOST_FOREACH(const String& name, names) {
		if (!qp.IsEmpty())
			qp += "&";

		qp += pluralType.ToLower() + "=" + name;
	}

	BOOST_FOREACH(const String& attr, attrs) {
		if (!qp.IsEmpty())
			qp += "&";

		qp += "attrs[]=" + attr;
	}

	boost::shared_ptr<HttpRequest> req = m_Connection->NewRequest();
	req->RequestMethod = "GET";
	req->RequestUrl = new Url(url + "?" + qp);
	req->AddHeader("Authorization", "Basic " + Base64::Encode(m_User + ":" + m_Password));
	m_Connection->SubmitRequest(req, boost::bind(ObjectsHttpCompletionCallback, _1, _2, callback));
}

void ApiClient::ObjectsHttpCompletionCallback(HttpRequest& request,
    HttpResponse& response, const ObjectsCompletionCallback& callback)
{
	Dictionary::Ptr result;

	String body;
	char buffer[1024];
	size_t count;

	while ((count = response.ReadBody(buffer, sizeof(buffer))) > 0)
		body += String(buffer, buffer + count);

	std::vector<ApiObject::Ptr> objects;

	try {
		result = JsonDecode(body);

		Array::Ptr results = result->Get("results");

		ObjectLock olock(results);
		BOOST_FOREACH(const Dictionary::Ptr objectInfo, results)
		{
			ApiObject::Ptr object = new ApiObject();

			Dictionary::Ptr attrs = objectInfo->Get("attrs");

			{
				ObjectLock olock(attrs);
				BOOST_FOREACH(const Dictionary::Pair& kv, attrs)
				{
					object->Attrs[kv.first] = kv.second;
				}
			}

			Array::Ptr used_by = objectInfo->Get("used_by");

			{
				ObjectLock olock(used_by);
				BOOST_FOREACH(const Dictionary::Ptr& refInfo, used_by)
				{
					ApiObjectReference ref;
					ref.Name = refInfo->Get("name");
					ref.Type = refInfo->Get("type");
					object->UsedBy.push_back(ref);
				}
			}

			objects.push_back(object);
		}
	} catch (const std::exception& ex) {
		Log(LogCritical, "ApiClient")
			<< "Error while decoding response: " << DiagnosticInformation(ex);
	}

	callback(objects);
}