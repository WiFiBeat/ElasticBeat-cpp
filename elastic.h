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
#ifndef BEAT_PROTOCOL_ELASTIC_H
#define BEAT_PROTOCOL_ELASTIC_H

#include <string>
#include <vector>
#include <istream>
#include <map>
#include <rapidjson/document.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/InflatingStream.h>
#include <Poco/URI.h>
//#include <Poco/Timespan.h>
#include <istream>
#include <sstream>
#include <time.h>
#include "utils.h"

using std::string;
using std::vector;
using std::istream;
using std::map;
using std::stringstream;
using namespace rapidjson;

#define _ESB_USER_AGENT "elasticbeat-cpp/0.1"
#define _CONTENT_TYPE_JSON "application/json; charset=UTF-8"


namespace beat {
	namespace protocols {

		struct BulkResponse {
			unsigned short int httpStatus;
			bool errors;
			string error;
			vector <string> IDs;
			BulkResponse() : httpStatus(0), errors(true), error(""), IDs(vector <string>()) { }
		};

		enum IndexType {
			Daily,
			Monthly,
			Yearly,
			NoTime
		};

		enum HTTPVerb {
			GET,
			PUT,
			POST,
			HEAD
		};

		class elastic
		{
			private:
				string _host;
				bool _validConnection;
				string _elasticSearchVersion;

				string _bulkURL;
				string _indicesURL;

				static unsigned short doRequest(const string & URL, const HTTPVerb verb, Document & response, const string & data = "", const string & contentType = "")
				{
					bool send_body = false;
					Poco::URI uri(URL);

					// Memory leak: https://stackoverflow.com/questions/6375411/linking-poco-c-library-gives-numerous-memory-leaks
					Poco::Net::HTTPClientSession session(uri.getHost(), uri.getPort());
					session.setKeepAlive(false);
					//session.setKeepAlive(true);
					//session.setKeepAliveTimeout(Poco::Timespan(60, 0));
					string path(uri.getPathAndQuery());
					
					// Prepare request
					Poco::Net::HTTPRequest req(
						(verb == HTTPVerb::POST) ? Poco::Net::HTTPRequest::HTTP_POST :
							(verb == HTTPVerb::GET) ? Poco::Net::HTTPRequest::HTTP_GET :
							(verb == HTTPVerb::HEAD) ? Poco::Net::HTTPRequest::HTTP_HEAD :
												Poco::Net::HTTPRequest::HTTP_PUT,
							path, Poco::Net::HTTPMessage::HTTP_1_1);
					send_body = !data.empty() && verb != HTTPVerb::GET && verb != HTTPVerb::HEAD;
					if (send_body) {
						req.setContentType(contentType);
						req.setContentLength(data.size());
					}
					req.add("User-Agent", _ESB_USER_AGENT);
					req.add("Accept", _CONTENT_TYPE_JSON);

					// Send request
					try {
						session.sendRequest(req);

						// Get data
						std::ostream& os = session.sendRequest(req);
						if (send_body) {
							os << data;  // sends the body
						}

						// TODO: Support gzipped output from ES (use zlib if necessary)
						//       https://pocoproject.org/slides/100-Streams.pdf
						Poco::Net::HTTPResponse res;
						istream &is = session.receiveResponse(res);
						string responseStr = "";
						beat::utils::istream2string(is, responseStr);

						// If there is any data, parse it
						if (responseStr.empty() == false) {
							const char * temp = beat::utils::copyString2charstar(responseStr);
							response.Parse(temp);
						}

						return res.getStatus();
					} catch (...) {
						return 0;
					}
				}



				string buildURL(const string & path)
				{
					// Build the base of the URL
					stringstream ss;
					ss << this->_host;
					if (this->_host[this->_host.size() -1] != '/') {
						ss << '/';
					}
					
					if (path.empty()) {
						return ss.str();
					}

					// Add path
					if (path[0] == '/') {
						ss << path.substr(1);
					} else {
						ss << path;
					}

					return ss.str();
				}

				static string getIndexFromDocument(string & json, const string & indexBasename, IndexType indexType = Daily)
				{
					// No time required in index name, return it as is
					if (indexType == NoTime) {
						return string(indexBasename);
					}

					Document d;
					const char * dStr = utils::copyString2charstar(json); // Do I need to deallocate memory?
					d.Parse(dStr);
					if (d.IsObject() == false) {
						return "";
					}
					const char * ts = d["@timestamp"].GetString();
					if (ts == NULL || strlen(ts) != 24 || !strchr(ts, 'T') || ts[23] != 'Z') {
						// Should look like 2017-06-03T16:45:40.000Z
						// Invalid timestamp
						return "";
					}

					// Cut the string for the date
					char * indexDate = (char *)calloc(1, 11);
					if (indexType != NoTime) {
						switch (indexType) {
							case Daily:
								strncpy(indexDate, ts, 10);
								break;
							case Monthly:
								strncpy(indexDate, ts, 7);
								break;
							case Yearly:
								strncpy(indexDate, ts, 4);
								break;
							default:
								break;
						}
					}

					if (indexDate[0] == 0) {
						free(indexDate);
						return "";
					}

					// Assemble it
					stringstream ss;
					ss << indexBasename << '-' << indexDate;
					free(indexDate);

					return ss.str();
				}

				static string getServerVersion(const string & url)
				{
					string ret = "";
					Document d;
					unsigned short httpStatus = doRequest(url, HTTPVerb::GET, d);
					if (httpStatus == 0) {
						throw string("Error while querying server <" + url + "> or invalid JSON"); 
					}
					if (httpStatus != 200) {
						throw string("Server returned an error, HTTP " + std::to_string(httpStatus));
					}

					/*
					 * {
					 *   "name" : "WSIPBPh",
					 *   "cluster_name" : "elasticsearch",
					 *   "cluster_uuid" : "06UtkWGDQTyIUlKjqk59qg",
					 *   "version" : {
					 *     "number" : "5.4.0",
					 *     "build_hash" : "780f8c4",
					 *     "build_date" : "2017-04-28T17:43:27.229Z",
					 *     "build_snapshot" : false,
					 *     "lucene_version" : "6.5.0"
					 *   },
					 *   "tagline" : "You Know, for Search"
					 * }
					 */
					
					try {
						const Value & version = d["version"];
						const Value & number = version["number"];
						if (!number.IsString()) {
							throw string("");
						}
						ret = string(number.GetString());
					} catch (...) {
						throw string("Failed parsing information from Elasticsearch, please do a packet capture between wifibeat and ES then submit a bug report");
					}

					return ret;
				}

			public:
				elastic(const string & host) : _host(host), _elasticSearchVersion(""), _bulkURL(""), _indicesURL("")
				{
					if (this->_host.empty()) {
						throw string("Elastic: Host cannot be empty");
					}

					// Test connection
					string url = this->buildURL("/");
					this->_elasticSearchVersion = getServerVersion(url); // If it throws an error, let it go through

					// Connection is valid if we have a version number.
					this->_validConnection = (this->_elasticSearchVersion.empty() == false);
				}

				inline string Version()
				{
					return this->_elasticSearchVersion;
				}

				inline string toString()
				{
					return this->_host;
				}

				bool retryConnection()
				{
					// Test connection
					string url = this->buildURL("/");
					try {
						this->_elasticSearchVersion = getServerVersion(url); // If it throws an error, let it go through
						// Connection is valid if we have a version number.
						this->_validConnection = (this->_elasticSearchVersion.empty() == false);
					} catch (...) {
						return false;
					}

					return this->_validConnection;
				}

				vector <string> getIndices()
				{
					/*
					 * curl -XGET 'localhost:9200/_cat/indices'
					 * yellow open packetbeat-2017.04.23 J6IL8noZQHOaysiXVSugZg 5 1  112 0 318.5kb 318.5kb
					 * yellow open twitter2              Z6T09PwRSuCGXIwin9z1Vw 5 1    0 0    650b    650b
					 * yellow open packetbeat-2017.04.22 NETg_881QpqUFco5PRa1RQ 5 1 3901 0   1.9mb   1.9mb
					 * yellow open twitter               Xewjr9vaT3GHXdnKyS5qOg 3 2    0 0    390b    390b
					 * yellow open .kibana               XPSVr7a7RN2aNxsRaxhvAA 1 1    3 0  40.8kb  40.8kb
					 */

					/* 
					 * JSON:
					 * [
					 *   {"health":"yellow","status":"open","index":"packetbeat-2017.04.23","uuid":"J6IL8noZQHOaysiXVSugZg","pri":"5","rep":"1","docs.count":"112","docs.deleted":"0","store.size":"318.5kb","pri.store.size":"318.5kb"},
					 *   {"health":"yellow","status":"open","index":"twitter2","uuid":"Z6T09PwRSuCGXIwin9z1Vw","pri":"5","rep":"1","docs.count":"0","docs.deleted":"0","store.size":"800b","pri.store.size":"800b"},
					 *   {"health":"yellow","status":"open","index":"packetbeat-2017.06.03","uuid":"21Yg9JeeS9uej5CK0bBm3Q","pri":"5","rep":"1","docs.count":"12988","docs.deleted":"0","store.size":"4.2mb","pri.store.size":"4.2mb"},
					 *   {"health":"yellow","status":"open","index":"packetbeat-2017.04.22","uuid":"NETg_881QpqUFco5PRa1RQ","pri":"5","rep":"1","docs.count":"3901","docs.deleted":"0","store.size":"1.9mb","pri.store.size":"1.9mb"},
					 *   {"health":"yellow","status":"open","index":"twitter","uuid":"Xewjr9vaT3GHXdnKyS5qOg","pri":"3","rep":"2","docs.count":"0","docs.deleted":"0","store.size":"480b","pri.store.size":"480b"},
					 *   {"health":"yellow","status":"open","index":".kibana","uuid":"XPSVr7a7RN2aNxsRaxhvAA","pri":"1","rep":"1","docs.count":"4","docs.deleted":"0","store.size":"44.2kb","pri.store.size":"44.2kb"}
					 * ]
					 */
					vector<string> ret;
					if (!this->_validConnection) {
						return ret;
					}

					if (this->_indicesURL.empty()) {
						this->_indicesURL = this->buildURL("_cat/indices");
					}

					Document response;
					if (doRequest(this->_indicesURL, HTTPVerb::GET, response) != 200) {
						return ret;
					}

					// Parse it and get index.
					for	(Value::ConstValueIterator itr = response.Begin(); itr != response.End(); ++itr) {
						ret.push_back(string((*itr)["index"].GetString()));
					}

					return ret;
				}

				bool indexExists(const string & index)
				{
					/*
					 * curl --head 'localhost:9200/twitter?pretty'
					 * HTTP/1.1 200 OK
					 * content-type: text/plain; charset=UTF-8
					 * content-length: 0
					 * 
					 */

					if (!this->_validConnection) {
						return false;
					}

					string url = this->buildURL(index);
					Document d;
					return doRequest(url, HTTPVerb::HEAD, d) == 200;
				}

				bool createIndex(const string & index)
				{
					/*
					 * curl -XPUT 'localhost:9200/twitter2?pretty' -H 'Content-Type: application/json' -d'{ "settings" : { "index" : { } } }'
					 * {
					 *   "acknowledged" : true,
					 *   "shards_acknowledged" : true
					 * }
					 */

					/*
					 * Error, duplicate:
					 * {
					 *   "error" : {
					 *     "root_cause" : [
					 *       {
					 *         "type" : "index_already_exists_exception",
					 *         "reason" : "index [twitter2/Z6T09PwRSuCGXIwin9z1Vw] already exists",
					 *         "index_uuid" : "Z6T09PwRSuCGXIwin9z1Vw",
					 *         "index" : "twitter2"
					 *       }
					 *     ],
					 *     "type" : "index_already_exists_exception",
					 *     "reason" : "index [twitter2/Z6T09PwRSuCGXIwin9z1Vw] already exists",
					 *     "index_uuid" : "Z6T09PwRSuCGXIwin9z1Vw",
					 *     "index" : "twitter2"
					 *   },
					 *   "status" : 400
					 * }
					 */

					if (!this->_validConnection) {
						return false;
					}

					// Prepare parameters
					const string BASE_SETTINGS = "{ \"settings\" : { \"index\" : { } } }";
					string url = this->buildURL(index);
					Document response;

					// Do request. We should check there is no error returned but the server should already do that with the status code
					return doRequest(url, HTTPVerb::PUT, response, BASE_SETTINGS, _CONTENT_TYPE_JSON) == 200;
				}

				BulkResponse * bulkRequest(vector<string> & docs, const string & indexBasename, IndexType indexType = Daily)
				{
					// https://www.elastic.co/guide/en/elasticsearch/reference/current/docs-bulk.html
					// It simply sends all the documents (rapidjson) and sends a response with the
					// IDs of the newly added documents.
					// XXX: May not need to create index, they might be created automatically
					// XXX: How does ES behaves when multiple indices are specified in the request?

					/*
					It matches order of the items


					Request
					=======

					{"index":{"_index":"packetbeat-2017.06.03","_type":"flow"}}
					{"@timestamp":"2017-06-03T16:45:40.000Z","beat":{"hostname":"ubuntu","name":"ubuntu","version":"5.4.0"},"dest":{"ip":"172.16.30.1","stats":{"net_bytes_total":300,"net_packets_total":3}},"final":false,"flow_id":"kAD/////AP//CP////8AAAGsEB4BrBAegagR","icmp_id":4520,"last_time":"2017-06-03T16:45:37.257Z","source":{"ip":"172.16.30.129","stats":{"net_bytes_total":300,"net_packets_total":3}},"start_time":"2017-06-03T16:45:37.257Z","type":"flow"}
					{"index":{"_index":"packetbeat-2017.06.03","_type":"flow"}}
					{"@timestamp":"2017-06-03T16:45:40.000Z","beat":{"hostname":"ubuntu","name":"ubuntu","version":"5.4.0"},"dest":{"ip":"127.0.0.1","port":9200,"stats":{"net_bytes_total":2242,"net_packets_total":11}},"final":false,"flow_id":"EAT/////AP//////CP8AAAF/AAABfwAAATrr8CM","last_time":"2017-06-03T16:45:37.738Z","source":{"ip":"127.0.0.1","port":60218,"stats":{"net_bytes_total":2904,"net_packets_total":11}},"start_time":"2017-06-03T16:45:37.738Z","transport":"tcp","type":"flow"}



					Response:
					=========

					{
						"took":112,
						"errors":false,
						"items":[{
							"index" 
								{"_index":"packetbeat-2017.06.03","_type":"flow","_id":"AVxu2Rn5FCZ-9tFqUXLy","_version":1,"result":"created","_shards":{"total":2,"successful":1,"failed":0},"created":true,"status":201}},
								{"index":{"_index":"packetbeat-2017.06.03","_type":"flow","_id":"AVxu2Rn5FCZ-9tFqUXLz","_version":1,"result":"created","_shards":{"total":2,"successful":1,"failed":0},"created":true,"status":201}}]}

					Notes: first is _index, second one is index/_index

					Other with single item in request
					=================================

					{
						"took":11,
						"errors":false,
						"items":[{
							"index":
								{"_index":"packetbeat-2017.06.03","_type":"icmp","_id":"AVxu2RYQFCZ-9tFqUXLx","_version":1,"result":"created","_shards":{"total":2,"successful":1,"failed":0},"created":true,"status":201}}]}
					*/

					if (!this->_validConnection) {
						return NULL;
					}

					if (this->_bulkURL.empty()) {
						this->_bulkURL = this->buildURL("_bulk");
					}

					unsigned int i;
					BulkResponse * ret = NULL;
					if (indexBasename.empty()) {
						return NULL;
					}
					ret = new BulkResponse();
					if (docs.size() == 0) {
						ret->errors = false;
						return ret;
					}

					// Get list of indices for the documents
					vector <string> indices;
					for (i = 0; i < docs.size(); ++i) {
						indices.push_back(getIndexFromDocument(docs[i], indexBasename, indexType));
					}


					// Create the bulk request
					stringstream ss;
					for (unsigned int i = 0; i < indices.size(); ++i) {
						// Add index name
						ss << "{\"index\":{\"_index\":\""
							<< indices[i] << "\"";

						// Be careful about future breaking changes:
						// https://www.elastic.co/blog/index-type-parent-child-join-now-future-in-elasticsearch
						if (this->_elasticSearchVersion.empty() || this->_elasticSearchVersion[0] - '0' < 6 ) {
							ss << ",\"_type\":\"doc\"";
						}
						ss << "}}\n" << docs[i] << '\n';
					}

					// Send all the data
					Document response;
					ret->httpStatus = doRequest(this->_bulkURL, HTTPVerb::POST, response, ss.str(), _CONTENT_TYPE_JSON);
					ret->errors = (ret->httpStatus != 200 || response.IsObject() == false);
					
					// Error handling
					// ret->error should also be enabled if error is set to true in the 'response'
					// Need to get the error as well as the cause

					if (response.HasMember("errors")) {
						const Value & errorsPresent = response["errors"];
						if (!errorsPresent.IsBool()) {
							throw string("");
						}
						if (errorsPresent.GetBool()) {
							/*
							 * {
							 * 	"took": 4,
							 * 	"errors": true,
							 * 	"items": [{
							 * 		"index": {
							 * 			"_index": "wifibeat-2017-06-09",
							 * 			"_type": "doc",
							 * 			"_id": "AVyNbZQtSscC9gsusF8v",
							 * 			"status": 400,
							 * 			"error": {
							 * 				"type": "mapper_parsing_exception",
							 * 				"reason": "failed to parse [wlan.type_subtype]",
							 * 				"caused_by": {
							 * 					"type": "number_format_exception",
							 * 					"reason": "For input string: \"Beacon\""
							 * 				}
							 * 			}
							 * 		}
							 * 	}]
							 * }
							 * or
							 * {
							 *	"error": {
							 *		"root_cause": [{
							 *			"type": "action_request_validation_exception",
							 *			"reason": "Validation Failed: 1: no requests added;"
							 *		}],
							 *		"type": "action_request_validation_exception",
							 *		"reason": "Validation Failed: 1: no requests added;"
							 *	},
							 *	"status": 400
							 * }
							 */
							bool first = true;
							ret->errors = true;
							stringstream ss;
							
							// Get all errors
							if (response.HasMember("items")) {
								const Value & items = response["items"];
								if (!items.IsArray()) {
									ret->error = "Failed to find items array containing the errors.";
									return ret;
								}

								for (SizeType i = 0; i < items.Size(); i++) {
									stringstream ss2;
									const Value & idx = items[i]["index"];
									if (!idx.IsObject()) {
										continue;
									}

									const Value & err = idx["error"];
									if (!err.IsObject()) {
										continue;
									}

									if (first) {
										first = false;
									} else {
										ss << '\n';
									}

									ss2 << err["type"].GetString() << ": " << err["reason"].GetString();
									if (err.HasMember("caused_by")) {
										const Value & caused_by = err["caused_by"];
										if (caused_by.IsObject()) {
											ss2 << " (" << caused_by["type"].GetString() << ' ' << caused_by["reason"].GetString(); 
										}
									}

									ss << ss2.str();
								}
							} else {
								ss << "Cannot parse error. Capture elasticsearch traffic using tcpdump and report it"; 
							}
							ret->error = ss.str();
						} else {
							// TODO: Get IDs from output
							/*
							 * {
							 *	"took": 276,
							 *	"errors": false,
							 *	"items": [{
							 *		"index": {
							 *			"_index": "wifibeat-2017-06-09",
							 *			"_type": "doc",
							 *			"_id": "AVyNd-_7SscC9gsusF8x",
							 *			"_version": 1,
							 *			"result": "created",
							 *			"_shards": {
							 *				"total": 2,
							 *				"successful": 1,
							 *				"failed": 0
							 *			},
							 *			"created": true,
							 *			"status": 201
							 *		}
							 *	}]
							 * }
							 */
							// Get the IDs
							const Value & items = response["items"];
							if (!items.IsArray()) {
								ret->errors = true;
								ret->error = "Cannot find items array that contains the IDs in server response";
								return ret;
							}
							for (SizeType i = 0; i < items.Size(); i++) {
								const Value & idx = items[i]["index"];
								if (!idx.IsObject()) {
									continue;
								}

								// And put them in the array
								string id(idx["_id"].GetString());
								ret->IDs.push_back(id);
							}
						}
					} else if (response.HasMember("error")) {
						const Value & err = response["error"];
						if (!err.IsObject()) {
							ss << "Cannot parse 'error' field. Capture elasticsearch traffic using tcpdump and report it"; 
						} else {
							ss << err["type"].GetString() << ": " << err["reason"].GetString();
						}
						if (response.HasMember("status")) {
							ret->httpStatus = (unsigned short int)response["status"].GetUint();
						}
					}

					return ret;
				}
		};

	}
}

#endif // BEAT_PROTOCOL_ELASTIC_H
