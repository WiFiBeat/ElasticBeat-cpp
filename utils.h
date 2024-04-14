/*
 *   Copyright 2017 Thomas d'Otreppe de Bouvette <tdotreppe@aircrack-ng.org>
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at

 *       http://www.apache.org/licenses/LICENSE-2.0

 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */
#ifndef ELASTICBEAT_CPP_UTILS
#define ELASTICBEAT_CPP_UTILS

#include <string>
#include <istream>
#include <string.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

using std::istream;
using std::string;
using namespace rapidjson;

#define _EB_UTILS_BUFFER_LEN 65535

namespace beat {
	class utils {
		public:
			static bool istream2string(istream &in, string & ret)
			{
				// flawfinder: ignore
				char buffer[_EB_UTILS_BUFFER_LEN];
				// flawfinder: ignore
				while (in.read(buffer, sizeof(buffer))) {
					ret.append(buffer, sizeof(buffer));
				}
				ret.append(buffer, in.gcount());
				return true;
			}

			static char * copyString2charstar(const string & str)
			{
				char * buffer = static_cast<char *>(calloc(1, str.size() + 1));
				if (buffer != NULL) {
					// flawfinder: ignore
					strncpy(buffer, str.c_str(), str.size() + 1);
				}
				return buffer;
			}

			static bool JSONDocument2String(rapidjson::Document & d, string & ret)
			{
				rapidjson::StringBuffer buffer;
				rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
				d.Accept(writer);
				ret = buffer.GetString();
				return ret.empty() == false;
			}
	};
}

#endif // ELASTICBEAT_CPP_UTILS