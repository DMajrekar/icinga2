/******************************************************************************
 * Icinga 2                                                                   *
 * Copyright (C) 2012-2014 Icinga Development Team (http://www.icinga.org)    *
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

#include "base/json.hpp"
#include "base/debug.hpp"
#include "base/dictionary.hpp"
#include "base/array.hpp"
#include "base/objectlock.hpp"
#include "base/convert.hpp"
#include <boost/foreach.hpp>
#include <boost/exception_ptr.hpp>
#include <yajl/yajl_gen.h>
#include <yajl/yajl_parse.h>
#include <stack>

using namespace icinga;

static void Encode(yajl_gen handle, const Value& value);

static void EncodeDictionary(yajl_gen handle, const Dictionary::Ptr& dict)
{
	yajl_gen_map_open(handle);

	ObjectLock olock(dict);
	BOOST_FOREACH(const Dictionary::Pair& kv, dict) {
		yajl_gen_string(handle, reinterpret_cast<const unsigned char *>(kv.first.CStr()), kv.first.GetLength());
		Encode(handle, kv.second);
	}

	yajl_gen_map_close(handle);
}

static void EncodeArray(yajl_gen handle, const Array::Ptr& arr)
{
	yajl_gen_array_open(handle);

	ObjectLock olock(arr);
	BOOST_FOREACH(const Value& value, arr) {
		Encode(handle, value);
	}

	yajl_gen_array_close(handle);
}

static void Encode(yajl_gen handle, const Value& value)
{
	String str;

	switch (value.GetType()) {
		case ValueNumber:
			if (yajl_gen_double(handle, static_cast<double>(value)) == yajl_gen_invalid_number)
				yajl_gen_double(handle, 0);

			break;
		case ValueString:
			str = value;
			yajl_gen_string(handle, reinterpret_cast<const unsigned char *>(str.CStr()), str.GetLength());

			break;
		case ValueObject:
			if (value.IsObjectType<Dictionary>())
				EncodeDictionary(handle, value);
			else if (value.IsObjectType<Array>())
				EncodeArray(handle, value);
			else
				yajl_gen_null(handle);

			break;
		case ValueEmpty:
			yajl_gen_null(handle);

			break;
		default:
			VERIFY(!"Invalid variant type.");
	}
}

String icinga::JsonEncode(const Value& value)
{
	yajl_gen handle = yajl_gen_alloc(NULL);
	Encode(handle, value);

	const unsigned char *buf;
	size_t len;
	yajl_gen_get_buf(handle, &buf, &len);

	String result = String(buf, buf + len);

	yajl_gen_free(handle);

	return result;
}

struct JsonElement
{
	String Key;
	bool KeySet;
	Value EValue;

	JsonElement(void)
		: KeySet(false)
	{ }
};

struct JsonContext
{
public:
	void Push(const Value& value)
	{
		JsonElement element;
		element.EValue = value;

		m_Stack.push(element);
	}

	JsonElement Pop(void)
	{
		JsonElement value = m_Stack.top();
		m_Stack.pop();
		return value;
	}

	void AddValue(const Value& value)
	{
		if (m_Stack.empty()) {
			JsonElement element;
			element.EValue = value;
			m_Stack.push(element);
			return;
		}

		JsonElement& element = m_Stack.top();

		if (element.EValue.IsObjectType<Dictionary>()) {
			if (!element.KeySet) {
				element.Key = value;
				element.KeySet = true;
			} else {
				Dictionary::Ptr dict = element.EValue;
				dict->Set(element.Key, value);
				element.KeySet = false;
			}
		} else if (element.EValue.IsObjectType<Array>()) {
			Array::Ptr arr = element.EValue;
			arr->Add(value);
		} else {
			BOOST_THROW_EXCEPTION(std::invalid_argument("Cannot add value to JSON element."));
		}
	}

	Value GetValue(void) const
	{
		ASSERT(m_Stack.size() == 1);
		return m_Stack.top().EValue;
	}

	void SaveException(void)
	{
		m_Exception = boost::current_exception();
	}

	void ThrowException(void) const
	{
		boost::rethrow_exception(m_Exception);
	}

private:
	std::stack<JsonElement> m_Stack;
	Value m_Key;
	boost::exception_ptr m_Exception;
};

static int DecodeNull(void *ctx)
{
	JsonContext *context = static_cast<JsonContext *>(ctx);

	try {
		context->AddValue(Empty);
	} catch (...) {
		context->SaveException();
		return 0;
	}

	return 1;
}

static int DecodeBoolean(void *ctx, int value)
{
	JsonContext *context = static_cast<JsonContext *>(ctx);

	try {
		context->AddValue(value);
	} catch (...) {
		context->SaveException();
		return 0;
	}

	return 1;
}

static int DecodeNumber(void *ctx, const char *str, size_t len)
{
	JsonContext *context = static_cast<JsonContext *>(ctx);

	try {
		String jstr = String(str, str + len);
		context->AddValue(Convert::ToDouble(jstr));
	} catch (...) {
		context->SaveException();
		return 0;
	}

	return 1;
}

static int DecodeString(void *ctx, const unsigned char *str, size_t len)
{
	JsonContext *context = static_cast<JsonContext *>(ctx);

	try {
		context->AddValue(String(reinterpret_cast<const char *>(str), reinterpret_cast<const char *>(str) + len));
	} catch (...) {
		context->SaveException();
		return 0;
	}

	return 1;
}

static int DecodeStartMap(void *ctx)
{
	JsonContext *context = static_cast<JsonContext *>(ctx);

	try {
		context->Push(make_shared<Dictionary>());
	} catch (...) {
		context->SaveException();
		return 0;
	}

	return 1;
}

static int DecodeEndMap(void *ctx)
{
	JsonContext *context = static_cast<JsonContext *>(ctx);

	try {
		context->AddValue(context->Pop().EValue);
	} catch (...) {
		context->SaveException();
		return 0;
	}

	return 1;
}

static int DecodeStartArray(void *ctx)
{
	JsonContext *context = static_cast<JsonContext *>(ctx);
	
	try {
		context->Push(make_shared<Array>());
	} catch (...) {
		context->SaveException();
		return 0;
	}

	return 1;
}

static int DecodeEndArray(void *ctx)
{
	JsonContext *context = static_cast<JsonContext *>(ctx);

	try {
		context->AddValue(context->Pop().EValue);
	} catch (...) {
		context->SaveException();
		return 0;
	}

	return 1;
}

Value icinga::JsonDecode(const String& data)
{
	static const yajl_callbacks callbacks = {
		DecodeNull,
		DecodeBoolean,
		NULL,
		NULL,
		DecodeNumber,
		DecodeString,
		DecodeStartMap,
		DecodeString,
		DecodeEndMap,
		DecodeStartArray,
		DecodeEndArray
	};

	yajl_handle handle;
	JsonContext context;

	handle = yajl_alloc(&callbacks, NULL, &context);

	yajl_parse(handle, reinterpret_cast<const unsigned char *>(data.CStr()), data.GetLength());

	if (yajl_complete_parse(handle) != yajl_status_ok) {
		unsigned char *internal_err_str = yajl_get_error(handle, 1, reinterpret_cast<const unsigned char *>(data.CStr()), data.GetLength());
		String msg = reinterpret_cast<char *>(internal_err_str);
		yajl_free_error(handle, internal_err_str);

		yajl_free(handle);

		BOOST_THROW_EXCEPTION(std::invalid_argument(msg));
	}

	yajl_free(handle);

	return context.GetValue();
}