#include <cstring>
#include "errno_exception.h"

using namespace std;

errno_exception::errno_exception (const string &msg, int code)
{
	char buf[200];
	this->msg = msg + ": " + strerror_r (code, buf, sizeof(buf) / sizeof(*buf));
}

const char *errno_exception::what () const noexcept
{
	return msg.c_str();
}
