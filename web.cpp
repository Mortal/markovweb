#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <markov.h>
#include <sstream>

using namespace Poco::Net;

struct FileRequestHandler : public HTTPRequestHandler {
    virtual void handleRequest(HTTPServerRequest & request, HTTPServerResponse & response) {
	std::string path = request.getURI();
	if (path == "/") path = "/index.html";
	if (path.find("..") != std::string::npos || path[0] != '/') return send404(response);
	path = "./static" + path;
	std::ifstream is(path.c_str());
	if (!is.is_open()) return send404(response);
	sendFile(response, is, guess_media_type(path));
	response.send() << "Yoyo" << std::endl;
    }

private:
    static std::string guess_media_type(const std::string & path) {
	//const char * def = "application/octet-stream";
	const char * def = "text/plain";
	size_t pos = path.find_last_of('.');
	if (pos == std::string::npos) return def;
	std::string ext = path.substr(pos+1);
	if (ext == "txt") return "text/plain";
	if (ext == "html") return "text/html";
	return def;
    }

    void send404(HTTPServerResponse & response) {
	response.setStatus(HTTPResponse::HTTP_NOT_FOUND);
	response.send() << "Four oh four" << std::endl;
    }

    void sendFile(HTTPServerResponse & response, std::istream & is, const std::string & mediaType) {
	response.setContentType(mediaType);
	response.send() << is.rdbuf();
    }
};

struct markov_streambuf : public std::streambuf {
    typedef std::streambuf::traits_type traits_type;
    typedef traits_type::int_type int_type;
    typedef traits_type::pos_type pos_type;
    typedef traits_type::off_type off_type;

    markov_streambuf(std::streambuf * psource)
	: source(*psource)
	, state(DEF)
    {
	buf = new char[BUFFER_SIZE];
	this->setg(0, 0, 0);
    }

    virtual ~markov_streambuf() {
	delete [] buf;
    }

protected:
    static char hex_to_num(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	else if (c >= 'A' && c <= 'F') return 10 + c - 'A';
	else if (c >= 'a' && c <= 'f') return 10 + c - 'a';
	return 0;
    }

    virtual int_type underflow() {
	std::streamsize len = source.sgetn(buf, BUFFER_SIZE);

	char * output = buf;

	for (char * p = buf; p != buf + len; ++p) {
	    switch (state) {
		case DEF:
		    if (*p == '+') *output++ = ' ';
		    else if (*p == '%') state = FIRSTHEX;
		    else *output++ = *p;
		    break;
		case FIRSTHEX:
		    h = hex_to_num(*p);
		    state = SECONDHEX;
		    break;
		case SECONDHEX:
		    char g = hex_to_num(*p);
		    char c = h*16 + g;
		    if (c != '\r')
			*output++ = c;
		    state = DEF;
		    break;
	    }
	}

	setg(buf, buf, output);
	if (buf == output)
	    return traits_type::eof();

	return traits_type::not_eof(buf[0]);
    }

private:
    const static size_t BUFFER_SIZE = 1024;
    std::streambuf & source;
    char * buf;
    std::streamsize n;
    enum State { DEF, FIRSTHEX, SECONDHEX };
    State state;
    char h;
};

struct MarkovRequestHandler : public HTTPRequestHandler {
    MarkovRequestHandler(std::mt19937 & rng) : rng(rng) {}

    virtual void handleRequest(HTTPServerRequest & request, HTTPServerResponse & response) {
	std::istream & body = request.stream();
	markov_streambuf filterbuf(body.rdbuf());
	std::string mode = "c6";
	{
	    const char * expect = "mode=";
	    while (*expect) {
		int c = filterbuf.sbumpc();
		if (c == EOF || c != *expect) {
		    std::cout << "Expect " << *expect << " got " << static_cast<char>(c) << std::endl;
		    return send400(response, "1");
		}
		++expect;
	    }
	}
	{
	    std::stringstream modestream;
	    while (true) {
		int c = filterbuf.sbumpc();
		if (static_cast<char>(c) == '&') break;
		if (c == EOF) return send400(response, "2");
		modestream << static_cast<char>(c);
	    }
	    mode = modestream.str();
	}
	{
	    const char * expect = "text=";
	    while (*expect) {
		int c = filterbuf.sbumpc();
		if (c == EOF || c != *expect) return send400(response, "3");
		++expect;
	    }
	}
	std::istream filterbody(&filterbuf);
	response.setContentType("text/plain");
	markov(filterbody, response.send(), mode, 1000, rng);
    }

private:
    void send400(HTTPServerResponse & response, const char * error) {
	response.setStatus(HTTPResponse::HTTP_BAD_REQUEST);
	response.send() << "Bad request\n" << error << std::endl;
    }

    std::mt19937 & rng;
};

struct rqhdlfact : public HTTPRequestHandlerFactory {
    virtual HTTPRequestHandler * createRequestHandler(const HTTPServerRequest & request) {
	const std::string & url = request.getURI();
	if (url == "/markov") return new MarkovRequestHandler(rng);
	return new FileRequestHandler();
    }

    std::mt19937 rng;
};

void goserve() {
    HTTPRequestHandlerFactory::Ptr f(new rqhdlfact());
    ServerSocket sock(8080);
    HTTPServerParams::Ptr p(new HTTPServerParams());
    HTTPServer s(f, sock, p);
    s.start();
    while (sleep(1) || 1);
}

int main() {
    goserve();
    return 0;
}

// vim:set sw=4 ts=8 sts=4 noet:
