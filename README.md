# Description

It is a header-only C++ library to interact with ElasticSearch.

# What does header-only mean?

It means there is no library to link against and the library doesn't have any code to compile. Just drop it in your project and you're done. All the code in the header and it will be compiled when including the header file in your project. Another advantage is that there is no need to cross-compile.

# Dependencies

The following dependencies will need to be added to the project.

- libpoco-dev -> HTTP Client (link against PocoNet, PocoNetSSL and PocoFoundation)
- rapidjson-dev -> JSON (fastest according to https://github.com/miloyip/nativejson-benchmark#parsing-time )

# Example

```
#include <elasticbeat-cpp/elastic.h>

// Connect to ES and get version
elastic * e = new elastic(“http://localhost:9200”);

cout << “ES Version: “ << e->Version() << endl;

// Get all indices in ElasticSearch
vector <string> indicesList = e->getIndices();
for (const string & index : indicesList) {
	cout << "Index: " << index << endl;
}

// Check if twitter index exists
if (!e->indexExists(“twitter”)) 
	e->createIndex(“twitter”);

// Let’s assume we have a vector with JSON documents as strings called ‘docs’
// It will automatically create the index based on the date (@timestamp) in each document
// And in this case a daily index. If timestamp says it's 2017-05-13, it will become
// myIndex-2017-05-13
BulkReponse * r = e->bulkRequest(docs, “myIndex”, Daily);

// Handle response
if (r) {
	if (r->errors) {
		cout << “Errors happened: ” << r->error << endl;
	} else {
		cout << "Newly created IDs:" << endl;
		for (const string & id: r->IDs) {
			// Newly stored document IDs are in
			// r->IDs and it matches order in ‘docs’
			cout << "- " << id << endl;
		}
	}
} else {
	cout << "There is something wrong with the request" << endl;
}
```

# Future

- Have rapidJSON in the project and allow to switch between distro-provided version and built-in.
- Performance testing and improvements (see https://github.com/jrfonseca/gprof2dot)
- Beat API (Logstash)
- Improve connection to ElasticSearch (use a short timeout)
- Improved error handling (Bulk API)
- If connection fails, allow to start a thread that keeps trying to connect, then when it succeeds, mark as successful
- Built-in HTTP client
  - HTTPS with all the goodies
  - Chunked encoding
  - Basic authentication
  - gzip
