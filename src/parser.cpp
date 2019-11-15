#include "sdptransform.hpp"
#include <cstddef>   // size_t
#include <memory>    // std::addressof()
#include <sstream>   // std::stringstream, std::istringstream
#include <ios>       // std::noskipws
#include <algorithm> // std::find_if()
#include <cctype>    // std::isspace()
#include <re2/re2.h>

namespace sdptransform
{
	void parseReg(
			std::string* match,
			size_t match_size,
			const grammar::Rule& rule,
			json& location,
			const std::string& content
	);

	void attachProperties(
		std::string* match,
		size_t match_size,
		json& location,
		const std::vector<std::string>& names,
		const std::string& rawName,
		const std::vector<char>& types
	);

	json toType(const std::string& str, char type);

	bool isNumber(const std::string& str);

	bool isInt(const std::string& str);

	bool isFloat(const std::string& str);

	void trim(std::string& str);

	void insertParam(json& o, const std::string& str);

	json parse(const std::string& sdp)
	{
		//printf("Parsing sdp: %s\n", sdp.c_str());
		static const RE2 ValidLineRegex("^([a-z])=(.*)");

		json session = json::object();
		std::stringstream sdpstream(sdp);
		std::string line;
		json media = json::array();
		json* location = std::addressof(session);

		while (std::getline(sdpstream, line, '\n'))
		{
			// Remove \r if lines are separated with \r\n (as mandated in SDP).
			if (line.size() && line[line.length() - 1] == '\r')
				line.pop_back();

			// Ensure it's a valid SDP line.
			std::string s1;
			std::string s2;
			if (!RE2::FullMatch(line, ValidLineRegex, &s1, &s2)) {
				//printf("Reject %s for ^([a-z])=(.*)\n", line.c_str());
				continue;
			}

			char type = line[0];
			std::string content = line.substr(2);

			if (type == 'm')
			{
				json m = json::object();

				m["rtp"] = json::array();
				m["fmtp"] = json::array();

				media.push_back(m);

				// Point at latest media line.
				location = std::addressof(media[media.size() - 1]);
			}

			auto it = grammar::rulesMap.find(type);

			if (it == grammar::rulesMap.end())
				continue;

			auto& rules = it->second;
			bool is_passed = false;
			for (size_t j = 0; j < rules.size(); ++j) {
				auto& rule = rules[j];
				if (!RE2::FullMatch(content, *rule.reg))
					continue;

                int args_len = rule.types.size();
				std::string _str_args[args_len];
				RE2::Arg _args[args_len];
				RE2::Arg* args[args_len];
				for(int i = 0; i < args_len; i++) {
					_args[i] = &_str_args[i];
					args[i] = &_args[i];
				}
				if (RE2::PartialMatchN(content, *rule.reg, args, args_len))
				{
					//printf("Parse (%d - %s) %s %s\n", args_len, rule.format.c_str(), rule.reg->pattern().c_str(), content.c_str());
					parseReg(_str_args, args_len, rule, *location, content);
					is_passed = true;
					break;
				} else {
					printf("Why happend for (%d - %s) %s %s\n", args_len, rule.format.c_str(), rule.reg->pattern().c_str(), content.c_str());
				}
			}
			if(is_passed == false) {
				printf("invaild |%s|\n", content.c_str());
				for (size_t j = 0; j < rules.size(); ++j) {
					auto &rule = rules[j];
					printf(" invaild match(%s) |%s|\n", rule.format.c_str(), rule.reg->pattern().c_str());
				}
			}
		}

		// Link it up.
		session["media"] = media;

		return session;
	}

	json parseParams(const std::string& str)
	{
		json obj = json::object();
		std::stringstream ss(str);
		std::string param;

		while (std::getline(ss, param, ';'))
		{
			trim(param);

			if (param.length() == 0)
				continue;

			insertParam(obj, param);
		}

		return obj;
	}

	std::vector<int> parsePayloads(const std::string& str)
	{
		std::vector<int> arr;

		std::stringstream ss(str);
		std::string payload;

		while (std::getline(ss, payload, ' '))
		{
			arr.push_back(std::stoi(payload));
		}

		return arr;
	}

	json parseImageAttributes(const std::string& str)
	{
		json arr = json::array();
		std::stringstream ss(str);
		std::string item;

		while (std::getline(ss, item, ' '))
		{
			trim(item);

			// Special case for * value.
			if (item == "*")
				return item;

			if (item.length() < 5) // [x=0]
				continue;

			json obj = json::object();
			std::stringstream ss2(item.substr(1, item.length() - 2));
			std::string param;

			while (std::getline(ss2, param, ','))
			{
				trim(param);

				if (param.length() == 0)
					continue;

				insertParam(obj, param);
			}

			arr.push_back(obj);
		}

		return arr;
	}

	json parseSimulcastStreamList(const std::string& str)
	{
		json arr = json::array();
		std::stringstream ss(str);
		std::string item;

		while (std::getline(ss, item, ';'))
		{
			if (item.length() == 0)
				continue;

			json arr2 = json::array();
			std::stringstream ss2(item);
			std::string format;

			while (std::getline(ss2, format, ','))
			{
				if (format.length() == 0)
					continue;

				json obj = json::object();

				if (format[0] != '~')
				{
					obj["scid"] = format;
					obj["paused"] = false;
				}
				else
				{
					obj["scid"] = format.substr(1);
					obj["paused"] = true;
				}

				arr2.push_back(obj);
			}

			arr.push_back(arr2);
		}

		return arr;
	}

	void parseReg(std::string* match, size_t match_size, const grammar::Rule& rule, json& location, const std::string& content)
	{
		bool needsBlank = !rule.name.empty() && !rule.names.empty();

		if (!rule.push.empty() && location.find(rule.push) == location.end())
		{
			location[rule.push] = json::array();
		}
		else if (needsBlank && location.find(rule.name) == location.end())
		{
			location[rule.name] = json::object();
		}

		json object = json::object();
		json& keyLocation = !rule.push.empty()
			// Blank object that will be pushed.
			? object
			// Otherwise named location or root.
			: needsBlank
				? location[rule.name]
				: location;

		attachProperties(match, match_size, keyLocation, rule.names, rule.name, rule.types);

		if (!rule.push.empty())
			location[rule.push].push_back(keyLocation);
	}

	void attachProperties(
		std::string* match,
		size_t match_size,
		json& location,
		const std::vector<std::string>& names,
		const std::string& rawName,
		const std::vector<char>& types
	)
	{
		if (!rawName.empty() && names.empty())
		{
			//printf("attach0 %s %s %c\n", rawName.c_str(), match[0].c_str(), types[0]);
			location[rawName] = toType(match[0], types[0]);
		}
		else
		{
			for (size_t i = 0; i < names.size(); ++i)
			{
				if (i < match_size && !match[i].empty())
				{
					//printf("attach0 %s %s %c\n", names[i].c_str(), match[i].c_str(), types[i]);
					location[names[i]] = toType(match[i], types[i]);
				}
			}
		}
	}

	bool isInt(const std::string& str)
	{
		std::istringstream iss(str);
		long l;

		iss >> std::noskipws >> l;

		return iss.eof() && !iss.fail();
	}

	bool isFloat(const std::string& str)
	{
		std::istringstream iss(str);
		float f;

		iss >> std::noskipws >> f;

		return iss.eof() && !iss.fail();
	}

	json toType(const std::string& str, char type)
	{
		// https://stackoverflow.com/a/447307/4827838.

		switch (type)
		{
			case 's':
			{
				return str;
			}

			case 'd':
			{
				return atoi(str.c_str());
			}

			case 'f':
			{
				return atof(str.c_str());
			}
		}

		return nullptr;
	}

	void trim(std::string& str)
	{
		str.erase(
			str.begin(),
			std::find_if(
				str.begin(), str.end(), [](unsigned char ch) { return !std::isspace(ch); }
			)
		);

		str.erase(
			std::find_if(
				str.rbegin(), str.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(),
			str.end()
		);
	}

	void insertParam(json& o, const std::string& str)
	{
		static const RE2 KeyValueRegex("^\\s*([^= ]+)(?:\\s*=\\s*([^ ]+))?$");

		std::string p0;
		std::string p1;
		if (!RE2::FullMatch(str, KeyValueRegex, &p0, &p1))
			return;

		// NOTE: match[2] maybe not exist in the given str if the param has no
		// value. We may insert nullptr then, but it's easier to just set an empty
		// string.

		char type;

		if (isInt(p1))
			type = 'd';
		else if (isFloat(p1))
			type = 'f';
		else
			type = 's';

		// Insert into the given JSON object.
		o[p0] = toType(p1, type);
	}
}
