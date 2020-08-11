#ifndef __ERRNO_EXCEPTION_H
#define __ERRNO_EXCEPTION_H

#include <exception>
#include <string>

class errno_exception : public std::exception
{
protected:
	std::string msg;

public:
	errno_exception (const std::string &msg, int code);

	const char *what () const noexcept override;
};

#endif /* __ERRNO_EXCEPTION_H */
